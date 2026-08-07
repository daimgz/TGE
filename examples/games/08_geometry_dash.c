/* 08_geometry_dash - A real-time runner: continuous timing, scrolling and
 * AABB collision.
 *
 * The third game on the TGE_Game architecture, completing the physics trio:
 *
 *   Snake/Tetris      discrete logic stepped on a fixed grid (FixedStep).
 *   Breakout          continuous physics on a square-pixel grid: a single
 *                     entity (ball) against static walls and bricks.
 *   Geometry Dash     continuous physics with the world itself moving: the
 *                     ground scrolls under a fixed player, so the challenge
 *                     is timing a jump against moving obstacles.
 *
 * The split is the same as 06_snake_grid / 07_breakout:
 *
 *   DashWorld    pure game state and rules: player (float position, gravity
 *                and jump), a fixed array of obstacles that scroll left at an
 *                ever-growing speed, deterministic spawns, distance score and
 *                a MENU/RUNNING/OVER state machine. No canvas, no drawing.
 *   DashRenderer everything that touches the screen: canvas clear, HUD,
 *                floor band, player, obstacles and overlays. The world is
 *                only read, never changed. The playfield geometry lives in
 *                DashWorld.view (TGE_View, like the snakes); the renderer maps
 *                playfield local coordinates to the canvas with
 *                tge_view_translate, and caches the surface size the world
 *                layout was computed for (the world never remembers how it
 *                was presented).
 *   DashGame     the scene glue (a TGE_GameContext from tge-extra/game, first
 *                member) that owns one DashWorld + one DashRenderer, moves
 *                input into the world, resizes into world_layout and draws
 *                through the renderer.
 *
 * Teaching decisions, kept explicit because they are the point of the
 * example:
 *   - No tge-extra/collision and no FixedStep, on purpose: there are at most
 *     MAX_OBSTACLES (64) entities and their only motion is a horizontal
 *     scroll, so a handful of direct AABB tests per frame beat a spatial
 *     index, and continuous dt integration is the right model for a runner.
 *     This example shows when NOT to reach for an abstraction.
 *   - Spawns are deterministic: a tiny LCG (seed DASH_DEFAULT_SEED) drives
 *     obstacle kind and gap jitter, so a run is reproducible without rand().
 *   - Obstacle spacing is measured in cells, not seconds: as the scroll
 *     speeds up, the time between obstacles shrinks and the difficulty ramps
 *     by itself.
 *   - Anti-tunneling is a non-issue by design: only the obstacles move
 *     horizontally (at most MAX_SPEED cells/s, far below any frame rate), so
 *     a frame can never skip a collision. The only vertical motion is the
 *     player's jump, which does not move anything else.
 *   - Landing on top of blocks (the signature Geometry Dash mechanic) is
 *     intentionally left out: death on contact keeps the collision to a
 *     single AABB test, which is the lesson this example exists to teach.
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/game.h"
#include "tge-extra/input.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdint.h>
#include <string.h>

/* Playfield interior (cells), same convention as 07_breakout: the canvas is
 * MIN_FW+2 by MIN_FH+2 (the view adds a 1-cell margin; the renderer keeps
 * the top row for the HUD). No square pixels: 1 cell = 1 character. */
#define MIN_FW 24
#define MIN_FH 12
#define PLAYER_X 8            /* player cube left edge, fixed in local cells */
#define PLAYER_W 2
#define PLAYER_H 2
#define OBSTACLE_W 1
#define GRAVITY 40.0f         /* cells/s^2; peak jump ~3.7 cells, clears 2 */
#define JUMP_VY -16.0f        /* jump impulse, cells/s (negative = up) */
#define BASE_SPEED 7.0f       /* scroll speed at the start of a run */
#define MAX_SPEED 14.0f       /* scroll speed cap */
#define ACCEL 0.15f           /* scroll speed gain, cells/s^2 */
#define GAP_MIN 10            /* minimum obstacle gap, in scroll cells */
#define GAP_JITTER 8          /* random extra gap, drawn from the LCG */
#define SCORE_CELLS 4         /* scroll cells per score point */
#define MAX_OBSTACLES 64      /* fixed array, no malloc */
#define DASH_DEFAULT_SEED 12345u

/* ---------------------------------------------------------------- world */

typedef enum { DASH_MENU = 0, DASH_RUNNING, DASH_OVER } DashState;

typedef enum { DASH_OB_BLOCK = 0, DASH_OB_SPIKE } DashObstacleKind;

typedef struct {
    float x;           /* left edge, playfield local cells */
    DashObstacleKind kind;
    bool active;
} DashObstacle;

typedef struct {
    TGE_View view;     /* logical playfield layout; view.area is the field */
    DashState state;
    bool paused;
    float pos_x, pos_y; /* player cube top-left, local cell units (floats) */
    float vy;          /* player vertical velocity, cells/s */
    float floor_y;     /* cube top row when resting on the ground */
    float speed;       /* current scroll speed, cells/s */
    float scroll;      /* total scroll distance since the run started */
    float next_spawn;  /* scroll value at which the next obstacle appears */
    int score;
    uint32_t rng;      /* deterministic spawn LCG */
    DashObstacle obs[MAX_OBSTACLES];
} DashWorld;

static uint32_t lcg_next(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static bool rectf_intersects(float ax, float ay, float aw, float ah,
                             float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

/* Hitbox of an obstacle: both kinds stand on the floor; a block is the full
 * 2x2, a spike is its 2x1 base row (the triangle above is visual only). */
static void obstacle_rect(const DashWorld *w, const DashObstacle *o,
                          float *x, float *y, float *rw, float *rh)
{
    *x = o->x;
    *y = w->floor_y;
    *rw = OBSTACLE_W;
    *rh = (o->kind == DASH_OB_SPIKE) ? 1.0f : 2.0f;
}

static bool world_player_hits(const DashWorld *w, const DashObstacle *o)
{
    float ox, oy, ow, oh;
    obstacle_rect(w, o, &ox, &oy, &ow, &oh);
    return rectf_intersects(w->pos_x, w->pos_y, PLAYER_W, PLAYER_H, ox, oy,
                            ow, oh);
}

static void world_jump(DashWorld *w)
{
    if (w->pos_y >= w->floor_y)
        w->vy = JUMP_VY;
}

/* Start the run: the first tap both launches the scroll and jumps. */
static void world_start(DashWorld *w)
{
    w->state = DASH_RUNNING;
    w->scroll = 0.0f;
    w->speed = BASE_SPEED;
    w->next_spawn = 16.0f;
    w->score = 0;
    world_jump(w);
}

static void world_spawn(DashWorld *w)
{
    DashObstacle *slot = NULL;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!w->obs[i].active) {
            slot = &w->obs[i];
            break;
        }
    }
    if (!slot)
        return; /* all 64 busy (only possible at extreme speeds): skip one */
    slot->active = true;
    slot->x = (float)w->view.area.w;
    slot->kind = (lcg_next(&w->rng) & 1u) ? DASH_OB_SPIKE : DASH_OB_BLOCK;
}

static void world_update(DashWorld *w, float dt)
{
    if (!w->view.valid || w->paused || w->state != DASH_RUNNING)
        return;

    w->speed += ACCEL * dt;
    if (w->speed > MAX_SPEED)
        w->speed = MAX_SPEED;
    w->scroll += w->speed * dt;
    w->score = (int)(w->scroll / SCORE_CELLS);

    w->vy += GRAVITY * dt;
    w->pos_y += w->vy * dt;
    if (w->pos_y >= w->floor_y) {
        w->pos_y = w->floor_y;
        w->vy = 0.0f;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!w->obs[i].active)
            continue;
        w->obs[i].x -= w->speed * dt;
        if (w->obs[i].x + OBSTACLE_W < 0.0f)
            w->obs[i].active = false;
    }

    while (w->scroll >= w->next_spawn) {
        world_spawn(w);
        w->next_spawn += GAP_MIN + (float)(lcg_next(&w->rng) % GAP_JITTER);
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (w->obs[i].active && world_player_hits(w, &w->obs[i])) {
            w->state = DASH_OVER;
            return;
        }
    }
}

static void world_reset(DashWorld *w)
{
    w->state = DASH_MENU;
    w->paused = false;
    w->rng = DASH_DEFAULT_SEED;
    w->floor_y = (float)(w->view.area.h - PLAYER_H);
    w->pos_x = PLAYER_X;
    w->pos_y = w->floor_y;
    w->vy = 0.0f;
    w->speed = BASE_SPEED;
    w->scroll = 0.0f;
    w->next_spawn = 16.0f;
    w->score = 0;
    for (int i = 0; i < MAX_OBSTACLES; i++)
        w->obs[i].active = false;
}

/* Keep the player on the ground and drop obstacles that ended up entirely
 * off the playfield after a resize. */
static void world_resize_fix(DashWorld *w)
{
    w->floor_y = (float)(w->view.area.h - PLAYER_H);
    if (w->pos_y > w->floor_y)
        w->pos_y = w->floor_y;
    if (w->state == DASH_MENU) {
        w->pos_y = w->floor_y;
        w->vy = 0.0f;
    }
}

/* Recompute the playfield for a new surface size (the renderer feeds the
 * canvas size; 1 cell = 1 character). First valid layout spawns a fresh
 * game, later resizes keep the current game and too-small stays inactive. */
static void world_layout(DashWorld *w, int gw, int gh)
{
    TGE_ViewUpdate upd = tge_view_update(&w->view, gw, gh);
    switch (upd) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(w);
        break;
    case TGE_VIEW_RESIZED:
        world_resize_fix(w);
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static void world_init(DashWorld *w)
{
    tge_view_init(&w->view, MIN_FW, MIN_FH);
}

/* ------------------------------------------------------------- renderer */

/* Rendering is stateless apart from the spike glyph and the layout cache:
 * the playfield geometry lives in DashWorld.view, and layout_w/layout_h
 * remember the surface size the world layout was computed for (the world
 * itself never remembers how it was presented).
 *
 * No TGE_GridLayout here on purpose: this game has no grid. The cache
 * compares the surface size (cells) directly, so tge_grid_layout_sync would
 * only add a layer. 06_snake_grid and 07_breakout show the grid case; this
 * is the "when NOT to reach for the abstraction" counterpart. */
typedef struct {
    uint32_t spike_glyph; /* U+25B2 when the terminal supports Unicode */
    int layout_w;         /* surface width the world layout was computed for */
    int layout_h;         /* surface height the world layout was computed for */
} DashRenderer;

static void renderer_draw(DashRenderer *r, TGE_Canvas *canvas,
                          const DashWorld *w)
{
    int ch = tge_canvas_height(canvas);

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);

    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT,
               " SCORE: %d  SPEED: %d ", w->score, (int)w->speed);

    if (!w->view.valid) {
        tge_draw_centered_text(canvas, ch / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    /* Floor band: the last playfield row, drawn with tge_view_translate so
     * the renderer never hardcodes the HUD offset. */
    {
        TGE_Vec2i p = tge_view_translate(
            &w->view, tge_vec2i(0, w->view.area.h - 1));
        tge_fill_rect(canvas, p.x, p.y, w->view.area.w, 1, ' ',
                      TGE_COLOR_BLACK, TGE_COLOR_GREEN);
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        const DashObstacle *o = &w->obs[i];
        if (!o->active)
            continue;
        TGE_Vec2i p = tge_view_translate(&w->view, tge_vec2i((int)o->x,
                                                             (int)w->floor_y));
        if (o->kind == DASH_OB_BLOCK) {
            tge_fill_rect(canvas, p.x, p.y, OBSTACLE_W, 2, ' ',
                          TGE_COLOR_BLACK, TGE_COLOR_RED);
        } else {
            tge_set_cell(canvas, p.x, p.y, r->spike_glyph, TGE_COLOR_RED,
                         TGE_COLOR_DEFAULT);
            tge_set_cell(canvas, p.x + 1, p.y, r->spike_glyph, TGE_COLOR_RED,
                         TGE_COLOR_DEFAULT);
        }
    }

    {
        TGE_Vec2i p = tge_view_translate(&w->view,
                                         tge_vec2i((int)w->pos_x,
                                                   (int)w->pos_y));
        tge_fill_rect(canvas, p.x, p.y, PLAYER_W, PLAYER_H, ' ',
                      TGE_COLOR_BLACK, TGE_COLOR_CYAN);
    }

    if (w->state == DASH_OVER) {
        const char *again = " [SPACE]/[ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    } else if (w->paused) {
        const char *again = " [P] resume ";
        tge_draw_modal(canvas, " PAUSED ", again, TGE_COLOR_YELLOW);
    } else if (w->state == DASH_MENU) {
        tge_draw_centered_text(canvas, ch / 2 + 2, " [SPACE] jump to start ",
                               TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    }
}

/* ---------------------------------------------------------------- scenes */

/* The game as a whole: the scene glue (TGE_GameContext) + world + renderer,
 * wired together by the game callbacks. */
typedef struct {
    TGE_GameContext ctx; /* first member: scene->userdata == &game->ctx */
    DashWorld world;
    DashRenderer renderer;
} DashGame;

/* Only the title scene still talks directly to the app. Game scenes receive
 * ctx->app through TGE_GameContext. */
static TGE_App *g_app = NULL;

/* Jump input: UP/W (mapped by tge-extra) plus SPACE (KEYDOWN or TEXT), the
 * classic runner tap. */
static bool dash_is_jump(const TGE_Event *ev)
{
    if (tge_input_direction(ev) == TGE_DIR_UP)
        return true;
    if (ev->type == TGE_EVENT_TEXT && ev->data.text.codepoint == ' ')
        return true;
    if (ev->type == TGE_EVENT_KEYDOWN && ev->data.key.keycode == TGE_KEY_SPACE)
        return true;
    return false;
}

static void game_update(TGE_GameContext *ctx, float dt)
{
    DashGame *g = (DashGame *)tge_game_instance(ctx);
    world_update(&g->world, dt);
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    DashGame *g = (DashGame *)tge_game_instance(ctx);
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    if (g->renderer.layout_w != w || g->renderer.layout_h != h) {
        g->renderer.layout_w = w;
        g->renderer.layout_h = h;
        world_layout(&g->world, w, h);
    }
    renderer_draw(&g->renderer, canvas, &g->world);
}

static void game_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    DashGame *g = (DashGame *)tge_game_instance(ctx);

    if (ev->type == TGE_EVENT_RESIZE) {
        g->renderer.layout_w = ev->data.resize.w;
        g->renderer.layout_h = ev->data.resize.h;
        world_layout(&g->world, ev->data.resize.w, ev->data.resize.h);
        if (g->world.state != DASH_OVER)
            g->world.paused = true;
        return;
    }
    if (tge_input_pause(ev)) {
        if (g->world.state != DASH_OVER)
            g->world.paused = !g->world.paused;
        return;
    }
    if (g->world.paused && !tge_input_cancel(ev))
        return;
    if (dash_is_jump(ev)) {
        if (g->world.state == DASH_MENU)
            world_start(&g->world);
        else if (g->world.state == DASH_RUNNING)
            world_jump(&g->world);
        else if (g->world.state == DASH_OVER && g->world.view.valid)
            world_reset(&g->world);
        return;
    }
    if (tge_input_quit(ev)) {
        if (g->world.state == DASH_OVER)
            TGE_Quit(ctx->app);
        return;
    }
    bool confirm = tge_input_confirm(ev);
    if (!confirm && ev->type == TGE_EVENT_TEXT &&
        ev->data.text.codepoint == ' ')
        confirm = true;
    if (confirm && g->world.state == DASH_OVER && g->world.view.valid) {
        world_reset(&g->world);
        return;
    }
    if (tge_input_cancel(ev)) {
        TGE_PopScene(ctx->app);
    }
}

/* The game's interface, handed to tge_game_create(). No destroy: the
 * game owns no memory beyond the scene itself. */
static const TGE_GameCallbacks dash_callbacks = {
    game_update, game_draw, game_event, NULL,
};

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " GEOMETRY DASH ";
    const char *subtitle =
        " a real-time runner: continuous timing, scrolling, AABB collision ";
    const char *controls = " SPACE/UP/W jump  P: pause ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 - 4, title, TGE_COLOR_GREEN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 - 2, subtitle, TGE_COLOR_CYAN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 1, controls, TGE_COLOR_WHITE,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 3, start, TGE_COLOR_YELLOW,
                           TGE_COLOR_DEFAULT);

    uint32_t spike = tge_unicode_supported() ? 0x25B2u : '^';
    for (int i = 0; i < 6; i++)
        tge_set_cell(canvas, w / 2 - 3 + i, h / 2 + 5, spike,
                     TGE_COLOR_RED, TGE_COLOR_DEFAULT);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    if (tge_input_cancel(ev) || tge_input_quit(ev)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        DashGame *g = (DashGame *)tge_game_create(g_app, sizeof(DashGame),
                                                  &dash_callbacks);
        if (!g)
            return;
        g->renderer.spike_glyph = tge_unicode_supported() ? 0x25B2u : '^';
        world_init(&g->world);
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
     * terminal size when it can query it (TIOCGWINSZ). 1 cell = 1 character,
     * no square pixels, so the canvas is MIN_FW+2 by MIN_FH+2. */
    TGE_App *app = TGE_Create(MIN_FW + 2, MIN_FH + 2, "TGE Geometry Dash");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
