/* 07_breakout - Breakout on a square-pixel grid with continuous physics.
 *
 * The canonical example of the TGE game architecture, built on the pieces
 * 06_snake_grid established. The code is split into three concerns:
 *
 *   BreakoutWorld    pure game state and rules: paddle, ball, bricks, score,
 *                    lives, levels, SERVE/RUNNING/OVER. The ball is physics
 *                    in floats (local cell units: x += vx*dt), never rounded
 *                    to cells except when rendering. No canvas, no drawing.
 *   BreakoutRenderer everything that touches the screen: canvas clear, HUD
 *                    (tge_printf), and the playfield through a TGE_GridView
 *                    (theme, cell size, origin). The world is only read,
 *                    never changed. It also owns a TGE_GridLayout: the grid
 *                    size the world was last laid out for, so game_draw()
 *                    re-lays the world only when the terminal changed. The
 *                    world never remembers how it was presented.
 *   BreakoutGame     the whole game: the scene glue (a TGE_GameContext from
 *                    tge-extra/game, first member) that owns one BreakoutWorld
 *                    + one BreakoutRenderer, moves input into the world,
 *                    resizes into world_layout and draws through the renderer.
 *
 * The playfield (logical LX x LY in grid cells) comes from the terminal size
 * through TGE_View, exactly like the snakes: the renderer maps a canvas to a
 * grid of 2x1 cells (square pixels) with a HUD row on top, and the world
 * plays inside that grid. All coordinates the world sees are cell units.
 *
 * Physics decisions, kept explicit because they are the point of this
 * example:
 *   - The ball is a float position with a velocity; `(int)` casts appear
 *     only in the renderer.
 *   - Walls are 0.5 cells off the interior edges (the ball is a 1x1 cell),
 *     the paddle sits two rows above the bottom border.
 *   - Anti-tunneling: the frame motion is split into sub-steps of at most
 *     SUBSTEP_CELLS (0.25) cells, so the ball can never skip a brick cell
 *     no matter the frame rate or speed.
 *   - The paddle reflects the ball with an angle from the hit offset (the
 *     classic Breakout pattern): rel in [-1, 1] maps to the launch angle off
 *     the vertical, so a center hit goes straight up and a side hit leaves
 *     diagonally toward the side it hit. Speed stays constant. The bounce is
 *     detected the same way as the bricks, by crossing the paddle surface.
 */
#include "tge/tge.h"

#include "tge-extra/array.h"
#include "tge-extra/game.h"
#include "tge-extra/grid.h"
#include "tge-extra/grid_view.h"
#include "tge-extra/input.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Minimum playfield interior (cells inside the border), same convention as
 * 06_snake_grid: at 2x1 cells the minimum canvas is 2*(MIN_FW+2) by
 * MIN_FH+3. */
#define MIN_FW 12
#define MIN_FH 8
#define PADDLE_W 5
#define PADDLE_SPEED 9.0f
#define KEY_HOLD 0.12f /* Pong-style hold: auto-release without key repeat */
#define BALL_SPEED 8.5f
#define MAX_BALL_SPEED 13.0f
#define LEVEL_SPEED_UP 0.7f
#define PADDLE_MAX_ANGLE 1.22173f /* ~70 deg off the vertical, in radians */
#define BRICK_ROWS 4
#define BRICK_POINTS 10
#define SUBSTEP_CELLS 0.25f /* max ball travel per physics sub-step */

/* ---------------------------------------------------------------- world */

typedef enum { BREAKOUT_SERVE = 0, BREAKOUT_RUNNING, BREAKOUT_OVER } BreakoutState;

/* Pong-style held-key state: a KEYDOWN arms a direction, the flag auto-releases
 * after KEY_HOLD unless a new keydown re-arms it (terminals have no repeat). */
typedef struct {
    int left, right;
    float t;
} Held;

typedef struct {
    TGE_View view;      /* logical playfield layout; view.area is the interior */
    uint8_t *cells;     /* brick marks, area.w * area.h, 1 = brick */
    int cap;
    int bricks_left;
    float px;           /* paddle center, local cell units */
    float bx, by;       /* ball position, local cell units (floats) */
    float bvx, bvy;     /* ball velocity, cells per second */
    int score, lives, level;
    int serve_dir;      /* launch direction after a serve */
    BreakoutState state;
    bool paused;
    Held held;
} BreakoutWorld;

static int world_paddle_y(const BreakoutWorld *w)
{
    return w->view.area.h - 2; /* one empty row above the bottom border */
}

static float world_ball_speed(const BreakoutWorld *w)
{
    float s = BALL_SPEED + (float)(w->level - 1) * LEVEL_SPEED_UP;
    if (s > MAX_BALL_SPEED)
        s = MAX_BALL_SPEED;
    return s;
}

static bool world_brick(const BreakoutWorld *w, int x, int y)
{
    if (!w->cells || x < 0 || y < 0 || x >= w->view.area.w ||
        y >= w->view.area.h)
        return false;
    return w->cells[y * w->view.area.w + x] != 0;
}

static void world_remove_brick(BreakoutWorld *w, int x, int y)
{
    int idx = y * w->view.area.w + x;
    if (w->cells[idx]) {
        w->cells[idx] = 0;
        w->bricks_left--;
    }
}

/* Full-width rows of bricks in the top rows; every level adds a row and the
 * ball gets faster. Leaves at least three rows for gap + paddle + bottom. */
static void world_build_bricks(BreakoutWorld *w)
{
    int aw = w->view.area.w;
    int ah = w->view.area.h;
    memset(w->cells, 0, (size_t)w->cap);
    int rows = BRICK_ROWS + (w->level - 1);
    if (rows > ah - 3)
        rows = ah - 3;
    if (rows < 1)
        rows = 0;
    w->bricks_left = 0;
    for (int y = 0; y < rows && (y + 1) * aw <= w->cap; y++) {
        for (int x = 0; x < aw; x++) {
            w->cells[y * aw + x] = 1;
            w->bricks_left++;
        }
    }
}

/* Park the ball on the paddle and wait for a serve. */
static void world_serve(BreakoutWorld *w)
{
    w->bx = w->px;
    w->by = (float)world_paddle_y(w) - 1.0f; /* ball sits on the paddle */
    w->bvx = 0.0f;
    w->bvy = 0.0f;
    w->state = BREAKOUT_SERVE;
    w->serve_dir = (rand() % 2) ? 1 : -1;
}

static void world_launch(BreakoutWorld *w)
{
    float speed = world_ball_speed(w);
    w->bvx = (float)w->serve_dir * speed * 0.4f;
    w->bvy = -sqrtf(speed * speed - w->bvx * w->bvx);
    w->state = BREAKOUT_RUNNING;
}

/* Paddle reflect: the hit offset picks the launch angle, so the ball comes
 * off the side it hit (classic Breakout) and speed magnitude stays the level
 * speed. rel is the hit position across the paddle in [-1, 1]; it maps to an
 * angle of at most PADDLE_MAX_ANGLE off the vertical. The collision line is
 * the paddle's top surface (paddle_y - 1.0 at the ball center), so the ball
 * rests with its bottom edge on the paddle instead of sinking into it. */
static void reflect_paddle(BreakoutWorld *w)
{
    float speed = world_ball_speed(w);
    float rel = (w->bx - w->px) / (PADDLE_W / 2.0f);
    if (rel > 1.0f)
        rel = 1.0f;
    else if (rel < -1.0f)
        rel = -1.0f;
    float angle = rel * PADDLE_MAX_ANGLE;
    w->bvx = sinf(angle) * speed;
    w->bvy = -cosf(angle) * speed;
    w->by = (float)world_paddle_y(w) - 1.0f; /* bottom edge on the paddle */
}

static void world_level_up(BreakoutWorld *w)
{
    w->level++;
    world_build_bricks(w);
    world_serve(w);
}

/* One sub-step of the ball: move, then wall / paddle / brick / miss. The
 * brick hit reflects on the axis whose cell boundary the ball crossed, so a
 * side hit flips vx and a top/bottom hit flips vy. */
static void world_ball_move(BreakoutWorld *w, float dt)
{
    float prev_bx = w->bx;
    float prev_by = w->by;
    w->bx += w->bvx * dt;
    w->by += w->bvy * dt;

    float aw = (float)w->view.area.w;
    float ah = (float)w->view.area.h;

    if (w->by < 0.5f) {
        w->by = 0.5f;
        w->bvy = fabsf(w->bvy);
    }
    if (w->bx < 0.5f) {
        w->bx = 0.5f;
        w->bvx = fabsf(w->bvx);
    } else if (w->bx > aw - 0.5f) {
        w->bx = aw - 0.5f;
        w->bvx = -fabsf(w->bvx);
    }

    float paddle_y = (float)world_paddle_y(w);
    float p_left = w->px - PADDLE_W / 2.0f;
    float p_right = w->px + PADDLE_W / 2.0f;
    if (w->bvy > 0.0f && prev_by <= paddle_y - 1.0f &&
        w->by >= paddle_y - 1.0f && w->bx + 0.5f > p_left &&
        w->bx - 0.5f < p_right) {
        reflect_paddle(w);
        return;
    }

    int cx = (int)w->bx;
    int cy = (int)w->by;
    if (world_brick(w, cx, cy)) {
        bool crossed_x = ((int)prev_bx != cx);
        bool crossed_y = ((int)prev_by != cy);
        world_remove_brick(w, cx, cy);
        w->score += BRICK_POINTS;
        if (crossed_x && crossed_y) {
            if (fabsf(w->bvx) >= fabsf(w->bvy))
                w->bvx = -w->bvx;
            else
                w->bvy = -w->bvy;
        } else if (crossed_x) {
            w->bvx = -w->bvx;
        } else {
            w->bvy = -w->bvy;
        }
        if (w->bricks_left == 0)
            world_level_up(w);
        return;
    }

    if (w->by > ah + 0.5f) {
        w->lives--;
        if (w->lives <= 0) {
            w->state = BREAKOUT_OVER;
        } else {
            world_serve(w);
        }
    }
}

static void world_ball_step(BreakoutWorld *w, float dt)
{
    float speed = sqrtf(w->bvx * w->bvx + w->bvy * w->bvy);
    int n = (int)ceilf(speed * dt / SUBSTEP_CELLS);
    if (n < 1)
        n = 1;
    for (int i = 0; i < n && w->state == BREAKOUT_RUNNING; i++)
        world_ball_move(w, dt / (float)n);
}

static void world_update(BreakoutWorld *w, float dt)
{
    if (!w->view.valid || w->paused || w->state == BREAKOUT_OVER)
        return;

    w->held.t += dt;
    if (w->held.t > KEY_HOLD) {
        w->held.left = 0;
        w->held.right = 0;
    }
    if (w->held.left)
        w->px -= PADDLE_SPEED * dt;
    if (w->held.right)
        w->px += PADDLE_SPEED * dt;
    float min_x = PADDLE_W / 2.0f;
    float max_x = (float)w->view.area.w - PADDLE_W / 2.0f;
    if (w->px < min_x)
        w->px = min_x;
    else if (w->px > max_x)
        w->px = max_x;

    if (w->state == BREAKOUT_SERVE) {
        w->bx = w->px; /* the ball follows the paddle while waiting */
        w->by = (float)world_paddle_y(w) - 1.0f;
        return;
    }
    world_ball_step(w, dt);
}

static void world_reset(BreakoutWorld *w)
{
    w->score = 0;
    w->lives = 3;
    w->level = 1;
    w->paused = false;
    w->px = (float)w->view.area.w / 2.0f;
    w->held.left = 0;
    w->held.right = 0;
    w->held.t = 0.0f;
    world_build_bricks(w);
    world_serve(w);
}

/* Clamp the paddle and ball into the new bounds after a resize; re-serve the
 * ball if it ended up out of the playfield or inside a (rebuilt) brick. */
static void world_resize_fix(BreakoutWorld *w)
{
    float min_x = PADDLE_W / 2.0f;
    float max_x = (float)w->view.area.w - PADDLE_W / 2.0f;
    if (w->px < min_x)
        w->px = min_x;
    else if (w->px > max_x)
        w->px = max_x;

    if (w->state == BREAKOUT_OVER)
        return;

    bool ok = w->bx >= 0.5f && w->bx <= (float)w->view.area.w - 0.5f &&
              w->by >= 0.5f && w->by <= (float)w->view.area.h + 0.5f &&
              !world_brick(w, (int)w->bx, (int)w->by);
    if (ok)
        return;
    if (w->state == BREAKOUT_SERVE) {
        w->bx = w->px;
        w->by = (float)world_paddle_y(w) - 1.0f;
    } else {
        world_serve(w);
    }
}

/* Recompute the playfield layout for a new logical grid size (gw x gh grid
 * cells, the renderer decides how a canvas maps to them). Same semantics as
 * the snakes: the first valid layout spawns a fresh game, later resizes keep
 * the current game (bricks rebuilt to the new width, entities clamped) and a
 * too-small size stays inactive until the terminal grows. */
static void world_layout(BreakoutWorld *w, int gw, int gh)
{
    TGE_ViewUpdate upd = tge_view_update(&w->view, gw, gh);

    int cap = w->view.area.w * w->view.area.h;
    tge_array_resize((void **)&w->cells, &w->cap, cap, sizeof(uint8_t));

    switch (upd) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(w);
        break;
    case TGE_VIEW_RESIZED:
        world_build_bricks(w);
        world_resize_fix(w);
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static void world_init(BreakoutWorld *w)
{
    tge_view_init(&w->view, MIN_FW, MIN_FH);
}

/* ------------------------------------------------------------- renderer */

typedef struct {
    TGE_GridView view;
    TGE_GridLayout layout; /* grid size already applied to the world */
} BreakoutRenderer;

static const TGE_Sprite SPR_EMPTY = TGE_SPRITE(2, 1, "  ", NULL);
static const TGE_Sprite SPR_BRICK = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88",
                                               "##");
static const TGE_Sprite SPR_WALL = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88",
                                              "##");
static const TGE_Sprite SPR_BALL = TGE_SPRITE(2, 1, "()", NULL);
static const TGE_Sprite SPR_PADDLE = TGE_SPRITE(2, 1, "\xE2\x96\x93\xE2\x96\x93",
                                                "==");
static const TGE_Sprite SPR_SELECT = TGE_SPRITE(2, 1, "::", NULL);

static const TGE_GridTheme BREAKOUT_THEME = {
    .empty = &SPR_EMPTY,
    .default_sprite = &SPR_BRICK,
    .border = &SPR_WALL,
    .selection = &SPR_SELECT,
};

/* Brick color cycles per row (classic striped look). A function instead of a
 * TGE_Color[] because compound literals are not constant initializers. */
static TGE_Color brick_color(int row)
{
    switch (row % 4) {
    case 0:
        return TGE_COLOR_RED;
    case 1:
        return TGE_COLOR_YELLOW;
    case 2:
        return TGE_COLOR_GREEN;
    default:
        return TGE_COLOR_CYAN;
    }
}

/* Point the grid at the canvas being drawn (the app swaps its double buffers
 * every frame); the theme, cell size and HUD origin were configured once in
 * title_event and persist across attaches. */
static void renderer_bind(BreakoutRenderer *r, TGE_Canvas *canvas)
{
    tge_grid_view_attach(&r->view, canvas);
}

static void renderer_draw(BreakoutRenderer *r, TGE_Canvas *canvas,
                          const BreakoutWorld *w)
{
    int ch = tge_canvas_height(canvas);
    renderer_bind(r, canvas);

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);

    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT,
               " SCORE: %d  LVL: %d  LIVES: %d ", w->score, w->level,
               w->lives);

    tge_grid_view_draw_border(&r->view, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);

    if (!w->view.valid) {
        tge_draw_centered_text(canvas, ch / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    for (int ly = 0; ly < w->view.area.h; ly++) {
        for (int lx = 0; lx < w->view.area.w; lx++) {
            if (!w->cells[ly * w->view.area.w + lx])
                continue;
            tge_grid_view_set_cell_local(&r->view, &w->view,
                                         tge_vec2i(lx, ly), brick_color(ly),
                                         TGE_COLOR_DEFAULT);
        }
    }

    int py = world_paddle_y(w);
    int px0 = (int)(w->px - PADDLE_W / 2.0f);
    for (int i = 0; i < PADDLE_W; i++)
        tge_grid_view_put_local(&r->view, &w->view,
                                tge_vec2i(px0 + i, py), &SPR_PADDLE,
                                TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);

    if (w->state != BREAKOUT_OVER) {
        tge_grid_view_put_local(&r->view, &w->view,
                                tge_vec2i((int)w->bx, (int)w->by), &SPR_BALL,
                                TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    }

    if (w->state == BREAKOUT_OVER) {
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    } else if (w->paused) {
        const char *again = " [P] resume ";
        tge_draw_modal(canvas, " PAUSED ", again, TGE_COLOR_YELLOW);
    } else if (w->state == BREAKOUT_SERVE) {
        tge_draw_centered_text(canvas, ch / 2 + 2, " [SPACE]/[ENTER] serve ",
                               TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    }
}

/* ---------------------------------------------------------------- scenes */

/* The game as a whole: the scene glue (TGE_GameContext) + world + renderer,
 * wired together by the game callbacks. */
typedef struct {
    TGE_GameContext ctx; /* first member: scene->userdata == &game->ctx */
    BreakoutWorld world;
    BreakoutRenderer renderer;
} BreakoutGame;

/* Only the title scene still talks directly to the app. Game scenes receive
 * ctx->app through TGE_GameContext.
 */
static TGE_App *g_app = NULL;

static void game_world_layout(void *userdata, int gw, int gh)
{
    world_layout((BreakoutWorld *)userdata, gw, gh);
}

static void game_update(TGE_GameContext *ctx, float dt)
{
    BreakoutGame *g = (BreakoutGame *)tge_game_instance(ctx);
    world_update(&g->world, dt);
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    BreakoutGame *g = (BreakoutGame *)tge_game_instance(ctx);
    tge_grid_layout_sync(&g->renderer.layout, tge_canvas_width(canvas),
                         tge_canvas_height(canvas), game_world_layout,
                         &g->world);
    renderer_draw(&g->renderer, canvas, &g->world);
}

static void game_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    BreakoutGame *g = (BreakoutGame *)tge_game_instance(ctx);

    if (ev->type == TGE_EVENT_RESIZE) {
        tge_grid_layout_sync(&g->renderer.layout, ev->data.resize.w,
                             ev->data.resize.h, game_world_layout, &g->world);
        if (g->world.state != BREAKOUT_OVER)
            g->world.paused = true;
        return;
    }
    if (tge_input_pause(ev)) {
        if (g->world.state != BREAKOUT_OVER)
            g->world.paused = !g->world.paused;
        return;
    }
    if (g->world.paused && !tge_input_cancel(ev))
        return;
    TGE_Direction d = tge_input_direction(ev);
    if (d == TGE_DIR_LEFT) {
        g->world.held.left = 1;
        g->world.held.t = 0.0f;
        return;
    }
    if (d == TGE_DIR_RIGHT) {
        g->world.held.right = 1;
        g->world.held.t = 0.0f;
        return;
    }
    if (tge_input_quit(ev)) {
        if (g->world.state == BREAKOUT_OVER)
            TGE_Quit(ctx->app);
        return;
    }
    bool confirm = tge_input_confirm(ev);
    if (!confirm && ev->type == TGE_EVENT_TEXT &&
        ev->data.text.codepoint == ' ')
        confirm = true;
    if (confirm) {
        if (g->world.state == BREAKOUT_OVER && g->world.view.valid)
            world_reset(&g->world);
        else if (g->world.state == BREAKOUT_SERVE && g->world.view.valid)
            world_launch(&g->world);
        return;
    }
    if (tge_input_cancel(ev)) {
        TGE_PopScene(ctx->app);
    }
}

static void game_destroy(TGE_GameContext *ctx)
{
    BreakoutGame *g = (BreakoutGame *)tge_game_instance(ctx);
    free(g->world.cells);
}

/* The breakout's interface, handed to tge_game_create(). */
static const TGE_GameCallbacks breakout_callbacks = {
    game_update, game_draw, game_event, game_destroy,
};

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " BREAKOUT ";
    const char *subtitle = " continuous physics on a square-pixel grid ";
    const char *controls = " Left/Right or A/D move  [SPACE]/[ENTER] serve  P: pause ";
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

    TGE_GridView gv;
    tge_grid_view_init(&gv, &BREAKOUT_THEME, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&gv, canvas);
    tge_grid_set_origin(&gv.grid, 0, h / 2 + 4);
    int gw, gh;
    tge_grid_view_size_for(&gv, w, h, &gw, &gh);
    int cx = (gw - 6) / 2;
    for (int i = 0; i < 6; i++)
        tge_grid_view_set_cell(&gv, cx + i, 0, TGE_COLOR_RED, TGE_COLOR_DEFAULT);
    tge_grid_view_put(&gv, cx + 2, 3, &SPR_BALL, TGE_COLOR_WHITE,
                      TGE_COLOR_DEFAULT);
    for (int i = 0; i < 5; i++)
        tge_grid_view_put(&gv, cx + 1 + i, 5, &SPR_PADDLE, TGE_COLOR_GREEN,
                          TGE_COLOR_DEFAULT);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    if (tge_input_cancel(ev) || tge_input_quit(ev)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        BreakoutGame *g = (BreakoutGame *)tge_game_create(
            g_app, sizeof(BreakoutGame), &breakout_callbacks);
        if (!g)
            return;
        /* Configure the renderer geometry once (cell size 2x1, HUD origin);
         * renderer_bind() re-attaches the current canvas every frame. */
        tge_grid_view_init(&g->renderer.view, &BREAKOUT_THEME,
                           TGE_GRID_SCALE_2X1);
        tge_grid_set_origin(&g->renderer.view.grid, 0, 1);
        tge_grid_layout_init(&g->renderer.layout, &g->renderer.view);
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
     * terminal size when it can query it (TIOCGWINSZ). At 2x1 cells the grid
     * needs 2*(MIN_FW+2) columns and MIN_FH+3 rows. */
    TGE_App *app = TGE_Create(2 * (MIN_FW + 2), MIN_FH + 3, "TGE Breakout");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
