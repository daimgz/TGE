/* 06_snake_grid - Snake with square pixels, a grid theme and the standard
 * TGE layering (world / renderer).
 *
 * Terminal cells are not square (~1:2), so the plain 01_snake looks
 * stretched. This version renders the whole playfield through a TGE_GridView
 * at cell size 2x1 (square pixels): every logical cell becomes a 2-character
 * block, so cells look square and horizontal motion matches vertical.
 *
 * It is the reference example of the game architecture the extra modules
 * serve, so the code is split into three concerns:
 *
 *   SnakeWorld     pure game state and rules: the snake, the food, scoring,
 *                  the step timer (TGE_FixedStep), the playfield layout
 *                  (TGE_View: min size, validity, first layout) and the queued
 *                  turns (TGE_InputBuffer). No canvas, no drawing.
 *   SnakeRenderer  everything that touches the screen: canvas clear, HUD
 *                  texts (tge_printf), and the playfield through a
 *                  TGE_GridView (theme, cell size, origin). The world is only
 *                  read, never changed.
 *   SnakeGame      the whole game: the scene glue (TGE_App/Scene wiring) that
 *                  owns one SnakeWorld + one SnakeRenderer and moves turns
 *                  into the input buffer, resizes into world_layout and draws
 *                  through the renderer.
 *
 * The playfield (logical LX x LY in grid cells) comes from the terminal size
 * through TGE_View: on a resize the interior (view.area) is recomputed and
 * the snake is clamped into the new bounds; the first VALID layout spawns a
 * fresh snake, so a too-small start simply waits until the terminal grows.
 * tge_view_update() returns what the new layout means
 * (TGE_VIEW_INVALID/RESIZED/FIRST_VALID), so the world reacts without
 * tracking its own "laid out" flag. TGE_View and the grid draw everything in
 * logical coordinates, so the same game logic runs at any terminal
 * resolution.
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/fixedstep.h"
#include "tge-extra/grid.h"
#include "tge-extra/grid_view.h"
#include "tge-extra/input.h"
#include "tge-extra/input_buffer.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdlib.h>

/* Minimum playfield interior (cells inside the border): the game asks for at
 * least this before a layout counts as valid. At cell size 2x1 with a HUD row
 * the minimum canvas is 2*(MIN_FW+2) by MIN_FH+3. */
#define MIN_FW 10
#define MIN_FH 6
#define MOVE_INTERVAL 0.10f
#define DIR_QUEUE 4

/* ------------------------------------------------------------------ world */

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

static int clamp_i(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

typedef struct {
    TGE_Vec2i *body; /* growable; capacity = playfield area */
    int cap;
    int len;
    TGE_Direction dir;
    TGE_InputBuffer input; /* queued turns, one applied per step */
    TGE_Vec2i food;
    int score;
    SnakeState state;
    TGE_FixedStep step;
    TGE_View view; /* logical playfield layout; view.area is the interior */
    int gw, gh;    /* last logical grid size the view was computed for */
} SnakeWorld;

static void world_init(SnakeWorld *s)
{
    tge_view_init(&s->view, MIN_FW, MIN_FH);
    tge_input_buffer_init(&s->input, DIR_QUEUE);
}

static bool world_spawn_food(SnakeWorld *s)
{
    if (s->view.area.w * s->view.area.h - s->len <= 0)
        return false;
    for (;;) {
        TGE_Vec2i p =
            tge_vec2i(rand() % s->view.area.w, rand() % s->view.area.h);
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

static void world_reset(SnakeWorld *s)
{
    int cx = s->view.area.w / 2;
    int cy = s->view.area.h / 2;
    s->len = 3;
    s->body[0] = tge_vec2i(cx, cy);
    s->body[1] = tge_vec2i(cx - 1, cy);
    s->body[2] = tge_vec2i(cx - 2, cy);
    s->dir = TGE_DIR_RIGHT;
    s->score = 0;
    s->state = SNAKE_RUNNING;
    tge_fixedstep_init(&s->step, MOVE_INTERVAL);
    tge_input_buffer_clear(&s->input);
    world_spawn_food(s);
}

/* Recompute the playfield layout for a new logical grid size (gw x gh grid
 * cells, the renderer decides how a canvas maps to them). The view decides
 * what the layout means: the first valid one spawns a fresh snake, later
 * resizes keep the current one (clamped into the new bounds, food respawned
 * if it no longer fits), and a too-small size stays inactive until the
 * terminal grows. */
static void world_layout(SnakeWorld *s, int gw, int gh)
{
    s->gw = gw;
    s->gh = gh;
    TGE_ViewUpdate upd = tge_view_update(&s->view, gw, gh);

    int cap = s->view.area.w * s->view.area.h;
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

    switch (upd) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(s);
        break;
    case TGE_VIEW_RESIZED:
        for (int i = 0; i < s->len; i++) {
            s->body[i].x = clamp_i(s->body[i].x, 0, s->view.area.w - 1);
            s->body[i].y = clamp_i(s->body[i].y, 0, s->view.area.h - 1);
        }
        if (s->food.x >= s->view.area.w || s->food.y >= s->view.area.h) {
            if (!world_spawn_food(s))
                s->state = SNAKE_OVER;
        }
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static bool world_step(SnakeWorld *s)
{
    TGE_Direction d;
    while (tge_input_buffer_pop(&s->input, &d)) {
        if (d == tge_direction_opposite(s->dir) || d == s->dir)
            continue;
        s->dir = d;
        break;
    }

    TGE_Vec2i nh = tge_vec2i_add(s->body[0], tge_direction_vec(s->dir));

    if (nh.x < 0 || nh.x >= s->view.area.w || nh.y < 0 ||
        nh.y >= s->view.area.h)
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
        return world_spawn_food(s);
    }
    return true;
}

static void world_update(SnakeWorld *s, float dt)
{
    if (s->state != SNAKE_RUNNING || !s->view.valid)
        return;
    tge_fixedstep_update(&s->step, dt);
    while (tge_fixedstep_next(&s->step)) {
        if (!world_step(s)) {
            s->state = SNAKE_OVER;
            break;
        }
    }
}

/* --------------------------------------------------------------- renderer */

typedef struct {
    const TGE_GridTheme *theme;
    TGE_GridView view;
} SnakeRenderer;

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

/* Sync the renderer with the canvas currently being drawn. The app swaps its
 * double buffers every frame, so the view is re-attached to the current
 * canvas on every draw; the theme, cell size and origin are configured once
 * at creation and re-applied here. */
static void renderer_bind(SnakeRenderer *r, TGE_Canvas *canvas)
{
    tge_grid_view_init(&r->view, canvas, r->theme, TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&r->view.grid, 0, 1);
}

static void renderer_draw(SnakeRenderer *r, TGE_Canvas *canvas,
                          const SnakeWorld *s)
{
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    renderer_bind(r, canvas);

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK, " SCORE: %d ",
               s->score);

    tge_grid_view_draw_border(&r->view, TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    if (!s->view.valid) {
        tge_draw_centered_text(canvas, h / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_BLACK);
        return;
    }

    int ox = s->view.area.x;
    int oy = s->view.area.y;
    for (int i = 1; i < s->len; i++)
        tge_grid_view_set_cell(&r->view, ox + s->body[i].x, oy + s->body[i].y,
                               TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put(&r->view, ox + s->body[0].x, oy + s->body[0].y,
                      &SPR_HEAD, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_view_put(&r->view, ox + s->food.x, oy + s->food.y, &SPR_FOOD,
                      TGE_COLOR_RED, TGE_COLOR_BLACK);

    if (s->state == SNAKE_OVER) {
        const char *msg = " GAME OVER ";
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_fill_rect(canvas, 1, h / 2 - 1, w - 2, 3, ' ', TGE_COLOR_BLACK,
                      TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, h / 2 - 1, msg, TGE_COLOR_RED,
                               TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, h / 2 + 1, again, TGE_COLOR_WHITE,
                               TGE_COLOR_BLACK);
    }
}

/* ----------------------------------------------------------------- scenes */

/* The game as a whole: the world (rules) + the renderer (screen), wired
 * together by the scene callbacks. */
typedef struct {
    SnakeWorld world;
    SnakeRenderer renderer;
} SnakeGame;

static TGE_App *g_app = NULL;
static TGE_Scene *g_title = NULL;
static TGE_Scene *g_game = NULL;

static void game_update(TGE_Scene *scene, float dt)
{
    SnakeGame *g = (SnakeGame *)scene->userdata;
    world_update(&g->world, dt);
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    SnakeGame *g = (SnakeGame *)scene->userdata;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    int gw, gh;
    tge_grid_view_size_for(&g->renderer.view, w, h, &gw, &gh);
    if (g->world.gw != gw || g->world.gh != gh)
        world_layout(&g->world, gw, gh);
    renderer_draw(&g->renderer, canvas, &g->world);
}

static void game_event(TGE_Scene *scene, TGE_Event *ev)
{
    SnakeGame *g = (SnakeGame *)scene->userdata;

    if (ev->type == TGE_EVENT_RESIZE) {
        int gw, gh;
        tge_grid_view_size_for(&g->renderer.view, ev->data.resize.w,
                               ev->data.resize.h, &gw, &gh);
        world_layout(&g->world, gw, gh);
        return;
    }
    TGE_Direction d = tge_input_direction(ev);
    if (d != TGE_DIR_NONE) {
        tge_input_buffer_push(&g->world.input, d);
        return;
    }
    if (tge_input_quit(ev)) {
        if (g->world.state == SNAKE_OVER)
            TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        if (g->world.state == SNAKE_OVER && g->world.view.valid)
            world_reset(&g->world);
        return;
    }
    if (tge_input_cancel(ev)) {
        g_game = NULL;
        TGE_PopScene(g_app);
    }
}

static void game_destroy(TGE_Scene *scene)
{
    SnakeGame *g = (SnakeGame *)scene->userdata;
    free(g->world.body);
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
    tge_draw_centered_text(canvas, h / 2 - 4, title, TGE_COLOR_GREEN,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 - 2, subtitle, TGE_COLOR_CYAN,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 + 1, controls, TGE_COLOR_WHITE,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 + 3, start, TGE_COLOR_YELLOW,
                           TGE_COLOR_BLACK);

    TGE_GridView gv;
    tge_grid_view_init(&gv, canvas, &SNAKE_THEME, TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&gv.grid, 0, 15);
    tge_grid_view_put(&gv, 6, 0, &SPR_HEAD, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    for (int i = 0; i < 3; i++)
        tge_grid_view_set_cell(&gv, 7 + i, 0, TGE_COLOR_GREEN,
                               TGE_COLOR_BLACK);
    tge_grid_view_put(&gv, 12, 0, &SPR_FOOD, TGE_COLOR_RED, TGE_COLOR_BLACK);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    if (tge_input_cancel(ev) || tge_input_quit(ev)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        TGE_Scene *game = NULL;
        SnakeGame *g = (SnakeGame *)tge_scene_create(
            &game, sizeof(SnakeGame), game_update, game_draw, game_event,
            game_destroy);
        g->renderer.theme = &SNAKE_THEME;
        tge_grid_view_init(&g->renderer.view, NULL, &SNAKE_THEME,
                           TGE_GRID_SCALE_2X1);
        tge_grid_set_origin(&g->renderer.view.grid, 0, 1);
        world_init(&g->world);
        g_game = game;
        TGE_PushScene(g_app, game);
    }
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *title = NULL;
    tge_scene_create(&title, 0, NULL, title_draw, title_event, NULL);
    title->opaque = false;
    g_title = title;
    TGE_PushScene(app, title);
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
    tge_scene_destroy(g_game);
    tge_scene_destroy(g_title);
    TGE_Destroy(app);
    return 0;
}
