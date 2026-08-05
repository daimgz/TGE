/* 06_snake_grid - Snake with square pixels and a grid theme.
 *
 * Terminal cells are not square (~1:2), so the plain 01_snake looks
 * stretched. This version renders the whole playfield through a TGE_Grid at
 * cell size 2x1 (tge_grid_square_pixels): every logical cell becomes a
 * 2-character block, so cells look square and horizontal motion matches
 * vertical.
 *
 * The grid draws through a TGE_GridTheme, so the game code only says WHAT a
 * cell is, never how it looks:
 *   empty   "  "   playfield background (clear)
 *   default "▓▓"   snake body, via tge_grid_set_cell
 *   border  "██"   playfield wall, via tge_grid_draw_border
 *   head    "██"   only the head and the food are special (tge_grid_put)
 *   food    "()"
 *
 * Same gameplay as 01_snake (vec2i, direction, fixedstep, input helpers);
 * only the drawing layer and the grid size differ. Both snakes adapt to the
 * terminal: TGE_Create requests a minimum and the core starts with the real
 * size; the playfield (logical LX x LY from the canvas size, HUD row on top)
 * is recomputed on every resize and the snake is clamped into the new bounds.
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/fixedstep.h"
#include "tge-extra/grid.h"
#include "tge-extra/input.h"
#include "tge-extra/vec2i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimum logical playfield interior (cells inside the border). At cell size
 * 2x1 the grid is canvas_w/2 by canvas_h-1, and the interior is the grid
 * minus its 1-cell border, so the minimum canvas is 2*(MIN_FW+2) by
 * MIN_FH+3. */
#define MIN_FW 10
#define MIN_FH 6
#define MOVE_INTERVAL 0.10f
#define DIR_QUEUE 4

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

typedef struct {
    TGE_Vec2i *body; /* growable; capacity = playfield area */
    int cap;
    int len;
    TGE_Direction dir;
    TGE_Direction dir_queue[DIR_QUEUE];
    int dir_head, dir_tail;
    TGE_Vec2i food;
    int score;
    SnakeState state;
    TGE_FixedStep step;
    TGE_Rect field; /* playfield interior in grid cells (the snake lives here) */
    int w, h;       /* last canvas size the playfield was computed for */
    bool laid_out;  /* body allocated and positioned at least once */
    bool too_small; /* terminal smaller than the MIN_FW x MIN_FH playfield */
} GameState;

static const TGE_Sprite SPR_EMPTY = { 2, 1, "  " };
static const TGE_Sprite SPR_BODY = { 2, 1, "\xE2\x96\x93\xE2\x96\x93" };
static const TGE_Sprite SPR_WALL = { 2, 1, "\xE2\x96\x88\xE2\x96\x88" };
static const TGE_Sprite SPR_HEAD = { 2, 1, "\xE2\x96\x88\xE2\x96\x88" };
static const TGE_Sprite SPR_FOOD = { 2, 1, "()" };
static const TGE_Sprite SPR_SELECT = { 2, 1, "::" };

static const TGE_GridTheme SNAKE_THEME = {
    .empty = &SPR_EMPTY,
    .default_sprite = &SPR_BODY,
    .border = &SPR_WALL,
    .selection = &SPR_SELECT,
};

static TGE_App *g_app = NULL;
static TGE_Scene *g_title = NULL;
static TGE_Scene *g_game = NULL;

static int clamp_i(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void push_dir(GameState *s, TGE_Direction d)
{
    int next = (s->dir_tail + 1) % DIR_QUEUE;
    if (next != s->dir_head) {
        s->dir_queue[s->dir_tail] = d;
        s->dir_tail = next;
    }
}

static bool spawn_food(GameState *s)
{
    if (s->field.w * s->field.h - s->len <= 0)
        return false;
    for (;;) {
        TGE_Vec2i p = tge_vec2i(rand() % s->field.w, rand() % s->field.h);
        bool free_spot = true;
        for (int i = 0; i < s->len; i++) {
            if (tge_vec2i_eq(s->body[i], p)) {
                free_spot = false;
                break;
            }
        }
        if (free_spot) {
            s->food = p;
            return true;
        }
    }
}

static void snake_reset(GameState *s)
{
    int cx = s->field.w / 2;
    int cy = s->field.h / 2;
    s->len = 3;
    s->body[0] = tge_vec2i(cx, cy);
    s->body[1] = tge_vec2i(cx - 1, cy);
    s->body[2] = tge_vec2i(cx - 2, cy);
    s->dir = TGE_DIR_RIGHT;
    s->dir_head = 0;
    s->dir_tail = 0;
    s->score = 0;
    s->state = SNAKE_RUNNING;
    tge_fixedstep_init(&s->step, MOVE_INTERVAL);
    spawn_food(s);
}

/* Recompute the logical playfield for a new canvas size. On the first layout
 * it spawns a fresh snake; on later resizes it keeps the current one, clamping
 * it into the new bounds and respawning the food if it no longer fits. */
static void snake_resize(GameState *s, int w, int h)
{
    bool first = !s->laid_out;
    s->w = w;
    s->h = h;
    s->field = tge_rect(1, 1, w / 2 - 2, h - 1 - 2);
    s->too_small = (s->field.w < MIN_FW || s->field.h < MIN_FH);

    int cap = s->field.w * s->field.h;
    if (cap < 1)
        cap = 1;
    if (cap != s->cap) {
        TGE_Vec2i *nb =
            (TGE_Vec2i *)realloc(s->body, (size_t)cap * sizeof(TGE_Vec2i));
        if (nb) {
            s->body = nb;
            s->cap = cap;
        }
    }
    if (s->len > s->cap)
        s->len = s->cap;

    if (s->too_small)
        return;
    if (first) {
        s->laid_out = true;
        snake_reset(s);
        return;
    }
    for (int i = 0; i < s->len; i++) {
        s->body[i].x = clamp_i(s->body[i].x, 0, s->field.w - 1);
        s->body[i].y = clamp_i(s->body[i].y, 0, s->field.h - 1);
    }
    if (s->food.x >= s->field.w || s->food.y >= s->field.h) {
        if (!spawn_food(s))
            s->state = SNAKE_OVER;
    }
}

static bool snake_step(GameState *s)
{
    while (s->dir_head != s->dir_tail) {
        TGE_Direction d = s->dir_queue[s->dir_head];
        s->dir_head = (s->dir_head + 1) % DIR_QUEUE;
        if (d == tge_direction_opposite(s->dir))
            continue;
        if (d == s->dir)
            continue;
        s->dir = d;
        break;
    }

    TGE_Vec2i nh = tge_vec2i_add(s->body[0], tge_direction_vec(s->dir));

    if (nh.x < 0 || nh.x >= s->field.w || nh.y < 0 || nh.y >= s->field.h)
        return false;

    bool ate = tge_vec2i_eq(nh, s->food);
    int check_to = s->len - (ate ? 0 : 1);
    for (int i = 0; i < check_to; i++) {
        if (tge_vec2i_eq(nh, s->body[i]))
            return false;
    }

    for (int i = s->len; i > 0; i--)
        s->body[i] = s->body[i - 1];
    s->body[0] = nh;

    if (ate) {
        s->len++;
        s->score += 10;
        return spawn_food(s);
    }
    return true;
}

static void game_update(TGE_Scene *scene, float dt)
{
    GameState *s = (GameState *)scene->userdata;
    if (s->state != SNAKE_RUNNING || !s->laid_out || s->too_small)
        return;
    tge_fixedstep_update(&s->step, dt);
    while (tge_fixedstep_next(&s->step)) {
        if (!snake_step(s)) {
            s->state = SNAKE_OVER;
            break;
        }
    }
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    GameState *s = (GameState *)scene->userdata;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    if (s->w != w || s->h != h)
        snake_resize(s, w, h);

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    char hud[32];
    snprintf(hud, sizeof(hud), " SCORE: %d ", s->score);
    tge_draw_text(canvas, 1, 0, hud, TGE_COLOR_YELLOW, TGE_COLOR_BLACK);

    TGE_Grid grid;
    tge_grid_init(&grid, canvas);
    tge_grid_square_pixels(&grid);
    tge_grid_set_origin(&grid, 0, 1);
    grid.theme = &SNAKE_THEME;
    tge_grid_draw_border(&grid, 0, 0, s->field.w + 2, s->field.h + 2,
                         TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    if (s->too_small) {
        const char *msg = " too small ";
        tge_draw_text(canvas, (w - (int)strlen(msg)) / 2, h / 2, msg,
                      TGE_COLOR_RED, TGE_COLOR_BLACK);
        return;
    }

    for (int i = 1; i < s->len; i++)
        tge_grid_set_cell(&grid, s->field.x + s->body[i].x,
                          s->field.y + s->body[i].y, TGE_COLOR_GREEN,
                          TGE_COLOR_BLACK);
    tge_grid_put(&grid, s->field.x + s->body[0].x, s->field.y + s->body[0].y,
                 &SPR_HEAD, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_put(&grid, s->field.x + s->food.x, s->field.y + s->food.y,
                 &SPR_FOOD, TGE_COLOR_RED, TGE_COLOR_BLACK);

    if (s->state == SNAKE_OVER) {
        const char *msg = " GAME OVER ";
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        int mx = (w - (int)strlen(msg)) / 2;
        tge_fill_rect(canvas, 1, h / 2 - 1, w - 2, 3, ' ', TGE_COLOR_BLACK,
                      TGE_COLOR_BLACK);
        tge_draw_text(canvas, mx, h / 2 - 1, msg, TGE_COLOR_RED,
                      TGE_COLOR_BLACK);
        int ax = (w - (int)strlen(again)) / 2;
        tge_draw_text(canvas, ax, h / 2 + 1, again, TGE_COLOR_WHITE,
                      TGE_COLOR_BLACK);
    }
}

static void game_event(TGE_Scene *scene, TGE_Event *ev)
{
    GameState *s = (GameState *)scene->userdata;

    if (ev->type == TGE_EVENT_RESIZE) {
        snake_resize(s, ev->data.resize.w, ev->data.resize.h);
        return;
    }
    TGE_Direction d = tge_input_direction(ev);
    if (d != TGE_DIR_NONE) {
        push_dir(s, d);
        return;
    }
    if (tge_input_quit(ev)) {
        if (s->state == SNAKE_OVER)
            TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        if (s->state == SNAKE_OVER && !s->too_small)
            snake_reset(s);
        return;
    }
    if (tge_input_cancel(ev)) {
        g_game = NULL;
        TGE_PopScene(g_app);
    }
}

static void game_destroy(TGE_Scene *scene)
{
    GameState *s = (GameState *)scene->userdata;
    free(s->body);
    free(s);
    free(scene);
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " SNAKE 2X1 ";
    const char *subtitle = " square pixels, grid adapts to terminal ";
    const char *controls = " Arrows or WASD to move ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(title)) / 2, h / 2 - 4, title,
                  TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(subtitle)) / 2, h / 2 - 2, subtitle,
                  TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(controls)) / 2, h / 2 + 1, controls,
                  TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(start)) / 2, h / 2 + 3, start,
                  TGE_COLOR_YELLOW, TGE_COLOR_BLACK);

    TGE_Grid grid;
    tge_grid_init(&grid, canvas);
    tge_grid_square_pixels(&grid);
    tge_grid_set_origin(&grid, 0, 15);
    grid.theme = &SNAKE_THEME;
    tge_grid_put(&grid, 6, 0, &SPR_HEAD, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    for (int i = 0; i < 3; i++)
        tge_grid_set_cell(&grid, 7 + i, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_put(&grid, 12, 0, &SPR_FOOD, TGE_COLOR_RED, TGE_COLOR_BLACK);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    if (tge_input_cancel(ev) || tge_input_quit(ev)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        TGE_Scene *game = (TGE_Scene *)calloc(1, sizeof(TGE_Scene));
        GameState *s = (GameState *)calloc(1, sizeof(GameState));
        game->userdata = s;
        game->opaque = true;
        game->update = game_update;
        game->draw = game_draw;
        game->event = game_event;
        game->destroy = game_destroy;
        g_game = game;
        TGE_PushScene(g_app, game);
    }
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *title = (TGE_Scene *)calloc(1, sizeof(TGE_Scene));
    title->opaque = false;
    title->draw = title_draw;
    title->event = title_event;
    g_title = title;
    TGE_PushScene(app, title);
}

static void cleanup_scene(TGE_Scene *sc)
{
    if (!sc)
        return;
    if (sc->destroy)
        sc->destroy(sc);
    else
        free(sc);
}

int main(void)
{
    /* Requested size is the minimum/fallback: the core starts with the real
     * terminal size when it can query it (TIOCGWINSZ). At 2x1 cells the grid
     * needs 2*(MIN_FW+2) columns and MIN_FH+3 rows. */
    TGE_App *app = TGE_Create(2 * (MIN_FW + 2), MIN_FH + 3, "TGE Snake 2x1");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    cleanup_scene(g_game);
    cleanup_scene(g_title);
    TGE_Destroy(app);
    return 0;
}
