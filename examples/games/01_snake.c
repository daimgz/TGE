#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/fixedstep.h"
#include "tge-extra/input.h"
#include "tge-extra/input_buffer.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdlib.h>

/* The playfield adapts to the terminal: TGE_Create requests a minimum and the
 * core starts with the real terminal size when it can. The layout is
 * decoupled: canvas size -> TGE_View -> playfield-local cell coordinates. The
 * view insets the playfield by one cell (the frame margin; the HUD row
 * overlaps the top border), reports validity (too small to play) and the
 * first valid layout; the snake logic only knows local coordinates and
 * drawing maps them with view.area. Queued turns go through a
 * TGE_InputBuffer so fast input between steps is not lost. */
#define MIN_FW 10 /* minimum playfield cols (canvas >= 12 wide) */
#define MIN_FH 6  /* minimum playfield rows (canvas >= 8 tall)  */
#define MOVE_INTERVAL 0.10f
#define DIR_QUEUE 4

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

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
    TGE_View view; /* playfield layout; view.area is the interior */
    int w, h;      /* last canvas size the view was computed for */
} GameState;

static TGE_App *g_app = NULL;

static void snake_init(GameState *s)
{
    tge_view_init(&s->view, MIN_FW, MIN_FH);
    tge_input_buffer_init(&s->input, DIR_QUEUE);
}

static bool spawn_food(GameState *s)
{
    if (s->view.area.w * s->view.area.h - s->len <= 0)
        return false;
    for (;;) {
        TGE_Vec2i p = tge_rect_random_point(s->view.area);
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
    spawn_food(s);
}

/* Recompute the playfield layout for a new canvas size. The view decides what
 * the layout means: the first valid one spawns a fresh snake, later resizes
 * keep the current one (clamped into the new bounds, food respawned if it no
 * longer fits), and a too-small size stays inactive until the terminal grows. */
static void snake_resize(GameState *s, int w, int h)
{
    s->w = w;
    s->h = h;
    TGE_ViewUpdate upd = tge_view_update(&s->view, w, h);

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
        snake_reset(s);
        break;
    case TGE_VIEW_RESIZED:
        for (int i = 0; i < s->len; i++)
            s->body[i] = tge_vec2i_clamp_rect(s->body[i], s->view.area);
        if (s->food.x >= s->view.area.w || s->food.y >= s->view.area.h) {
            if (!spawn_food(s))
                s->state = SNAKE_OVER;
        }
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static bool snake_step(GameState *s)
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
        return spawn_food(s);
    }
    return true;
}

static void game_update(TGE_Scene *scene, float dt)
{
    GameState *s = (GameState *)scene->userdata;
    if (s->state != SNAKE_RUNNING || !s->view.valid)
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

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    tge_printf(canvas, 2, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK, " SCORE: %d ",
               s->score);

    if (!s->view.valid) {
        tge_draw_centered_text(canvas, h / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_BLACK);
        return;
    }

    for (int i = 0; i < s->len; i++) {
        TGE_Vec2i gp = tge_rect_translate_point(s->view.area, s->body[i]);
        tge_set_cell(canvas, gp.x, gp.y, (i == 0) ? '@' : 'o',
                     TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    }
    TGE_Vec2i fp = tge_rect_translate_point(s->view.area, s->food);
    tge_set_cell(canvas, fp.x, fp.y, '*', TGE_COLOR_RED, TGE_COLOR_BLACK);

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

static void game_event(TGE_Scene *scene, TGE_Event *ev)
{
    GameState *s = (GameState *)scene->userdata;

    if (ev->type == TGE_EVENT_RESIZE) {
        snake_resize(s, ev->data.resize.w, ev->data.resize.h);
        return;
    }
    TGE_Direction d = tge_input_direction(ev);
    if (d != TGE_DIR_NONE) {
        tge_input_buffer_push(&s->input, d);
        return;
    }
    if (tge_input_quit(ev)) {
        if (s->state == SNAKE_OVER)
            TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        if (s->state == SNAKE_OVER && s->view.valid)
            snake_reset(s);
        return;
    }
    if (tge_input_cancel(ev)) {
        TGE_PopScene(g_app);
    }
}

static void game_destroy(TGE_Scene *scene)
{
    GameState *s = (GameState *)scene->userdata;
    free(s->body);
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " SNAKE ";
    const char *controls = " Arrows or WASD to move ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 - 2, title, TGE_COLOR_GREEN,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2, controls, TGE_COLOR_WHITE,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, h / 2 + 2, start, TGE_COLOR_YELLOW,
                           TGE_COLOR_BLACK);
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
        GameState *s = (GameState *)tge_scene_create(
            &game, sizeof(GameState), game_update, game_draw, game_event,
            game_destroy);
        snake_init(s);
        TGE_PushScene(g_app, game);
    }
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *title = NULL;
    tge_scene_create(&title, 0, NULL, title_draw, title_event, NULL);
    title->opaque = false;
    TGE_PushScene(app, title);
}

int main(void)
{
    /* Requested size is the minimum/fallback: the core starts with the real
     * terminal size when it can query it (TIOCGWINSZ). */
    TGE_App *app = TGE_Create(MIN_FW + 2, MIN_FH + 2, "TGE Snake");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
