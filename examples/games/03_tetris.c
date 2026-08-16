/* 03_tetris - Tetris on a grid with scheduler-driven gravity.
 *
 * The Grid renderer plus three tge-extra modules, each demonstrating why it
 * exists:
 *
 *   - Grid   two TGE_Grids (board 10x20 + NEXT preview) with 2x1 cells.
 *   - Timer  the rotation debounce: a gameplay cooldown (accumulator timer).
 *            "Can I rotate again yet?" is game state, not a motor event.
 *   - View   minimum playfield gate: "too small", wait for a valid layout
 *            (TGE_VIEW_FIRST_VALID) and keep/restart on resize.
 *   - Vec2i  the active piece position as a vector (movement with
 *            tge_vec2i_add) instead of loose (x, y) fields.
 *
 * The distinction the example is built to teach:
 *
 *   - Core scheduler (tge_runtime_call_every) -> periodic motor events:
 *     gravity fires every interval and is rescheduled on level up.
 *   - tge-extra TGE_Timer (accumulator fed per frame) -> gameplay cooldowns:
 *     rotation is a classic cooldown of ROTATE_DEBOUNCE, not a motor event.
 *
 * Two concepts, two mechanisms, one example each: gravity stays on the
 * scheduler, rotation uses a timer.
 */
#include "tge/tge.h"

#include "tge-extra/grid.h"
#include "tge-extra/timer.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"
#include "tge-extra/game.h"
#include "tge-extra/ui.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

/* Interior (inside the view margin) the fixed composition needs: board frame
 * 1..22, NEXT box right edge at column 33, controls row at the bottom. The
 * requested window is the minimum + the 1-cell margin on each side. */
#define MIN_FW 34
#define MIN_FH 22

#define COLS 10
#define ROWS 20
#define OX   2
#define OY   2

#define TMR_GRAVITY 200

#define ROTATE_DEBOUNCE 0.20f

#define POINTS_L1 100
#define POINTS_L2 300
#define POINTS_L3 500
#define POINTS_L4 800

typedef enum { STATE_PLAYING = 0, STATE_OVER } TetrisState;

typedef struct {
    int n;
    uint8_t cells[4][4];
    int type;
    uint8_t color;
} Tetromino;

typedef struct {
    int board[ROWS][COLS];
    Tetromino cur;
    TGE_Vec2i pos; /* active piece position in board cells */
    int next;
    int score;
    int level;
    int lines;
    int gravity_timer;
    TetrisState state;
    bool paused;
    TGE_Timer rot; /* rotation cooldown (classic debounce) */
    TGE_View view;
    int last_w, last_h; /* last surface the view was computed for (-1 = none) */
} Tetris;

/* The board and the NEXT preview are two separate coordinate spaces, so the
 * renderer owns one TGE_Grid per region instead of drawing both through the
 * board grid: the board is a logical 10x20 area at (OX, OY) and the preview
 * its own logical 5x5 area at the NEXT box. Cell size 2x1 and the origins are
 * permanent properties of the renderer; only the canvas changes per frame
 * (the app swaps its double buffers), so renderer_attach() just re-attaches. */
typedef struct {
    TGE_Grid board;   /* origin (OX, OY) */
    TGE_Grid preview; /* origin (24, 2), NEXT box */
} TetrisRenderer;

/* The game as a whole: the scene glue (TGE_GameContext) + world + renderer,
 * wired together by the game adapter callbacks. ctx MUST be the first member
 * (offset 0): scene->userdata == &game->ctx. */
typedef struct {
    TGE_GameContext ctx; /* first member: scene->userdata == &game->ctx */
    Tetris world;
    TetrisRenderer renderer;
} TetrisGame;

static const Tetromino kPieces[7] = {
    { 4, { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} }, 0, 6 },
    { 2, { {1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, 1, 3 },
    { 3, { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 2, 5 },
    { 3, { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, 3, 2 },
    { 3, { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 4, 1 },
    { 3, { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 5, 4 },
    { 3, { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 6, 7 },
};

static TGE_App *g_app = NULL;

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *ev);
static void game_update(TGE_GameContext *ctx, float dt);
static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_event(TGE_GameContext *ctx, TGE_Event *ev);
static void renderer_init(TetrisRenderer *r);
static void renderer_attach(TetrisRenderer *r, TGE_Canvas *canvas);
static void renderer_draw_board(TetrisRenderer *r, const Tetris *t);
static void renderer_draw_piece(TetrisRenderer *r, const Tetris *t);
static void renderer_draw_next(TetrisRenderer *r, const Tetris *t);
static void tetris_game_init(TetrisGame *g);
static void tetris_resize(Tetris *t, int w, int h);
static void reset(Tetris *t);
static void move_cur(Tetris *t, int dx);
static void try_rotate(Tetris *t);
static void rotate_pressed(Tetris *t);
static void hard_drop(Tetris *t);
static void soft_drop(Tetris *t);
static void gravity_step(Tetris *t);
static void rotate_cw(Tetromino *p);
static int fits_shape(const Tetromino *p, const Tetris *t, TGE_Vec2i pos);
static int fits(Tetris *t, TGE_Vec2i pos);
static float gravity_interval(int level);
static void set_gravity(Tetris *t);
static void game_over(Tetris *t);
static void clear_lines(Tetris *t);
static void spawn(Tetris *t);
static void lock(Tetris *t);

int main(void)
{
    /* Requested size is the minimum/fallback: the core starts with the real
     * terminal size when it can query it (TIOCGWINSZ). */
    TGE_App *app = TGE_Create(MIN_FW + 2, MIN_FH + 2, "TGE Tetris");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *menu = NULL;
    tge_scene_create(&menu, 0, NULL, title_draw, title_event, NULL);
    menu->opaque = false;
    TGE_PushScene(app, menu);
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " TETRIS ";
    const char *c1 = " Move: Left/Right   Rotate: W/Up ";
    const char *c2 = " Down: soft drop    Space: hard drop ";
    const char *start = " [ENTER] to start  [Q] to quit ";

    tge_draw_region(canvas, (TGE_Rect){0, 0, w, h}, NULL, TGE_COLOR_CYAN);
    tge_draw_centered_text(canvas, h / 2 - 3, title, TGE_COLOR_GREEN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2, c1, TGE_COLOR_WHITE,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 1, c2, TGE_COLOR_WHITE,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 3, start, TGE_COLOR_YELLOW,
                           TGE_COLOR_DEFAULT);
}

/* The game's interface, handed to tge_game_create(). */
static const TGE_GameCallbacks tetris_callbacks = {
    game_update,
    game_draw,
    game_event,
    NULL,
};

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    bool enter = false;
    bool quit = false;
    if (ev->type == TGE_EVENT_TEXT) {
        if (ev->data.text.codepoint == 13) {
            enter = true;
        } else if (ev->data.text.codepoint == 'q' ||
                   ev->data.text.codepoint == 'Q') {
            quit = true;
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN) {
        if (ev->data.key.keycode == TGE_KEY_ENTER) {
            enter = true;
        } else if (ev->data.key.keycode == TGE_KEY_ESC) {
            quit = true;
        }
    }
    if (quit) {
        TGE_Quit(g_app);
        return;
    }
    if (enter) {
        /* tge_game_create wires the trampolines and pushes the game scene in one
         * call; g points at the instance (ctx at offset 0). */
        TetrisGame *g = (TetrisGame *)tge_game_create(
            g_app, sizeof(TetrisGame), &tetris_callbacks);
        tetris_game_init(g);
    }
}

static void game_update(TGE_GameContext *ctx, float dt)
{
    TetrisGame *g = (TetrisGame *)tge_game_instance(ctx);
    if (g->world.paused)
        return;
    tge_timer_update(&g->world.rot, dt);
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    TetrisGame *g = (TetrisGame *)tge_game_instance(ctx);
    Tetris *t = &g->world;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);

    /* Layout is computed once here (the initial size never fires a RESIZE
     * event) and afterwards only on TGE_EVENT_RESIZE. */
    if (t->last_w < 0)
        tetris_resize(t, w, h);

    if (!t->view.valid) {
        tge_draw_centered_text(canvas, h / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    renderer_attach(&g->renderer, canvas);

    tge_draw_region(canvas, (TGE_Rect){OX - 1, OY - 1, COLS * 2 + 2, ROWS + 2},
                   NULL, TGE_COLOR_CYAN);

    renderer_draw_board(&g->renderer, &g->world);
    renderer_draw_piece(&g->renderer, &g->world);
    renderer_draw_next(&g->renderer, &g->world);

    tge_draw_region(canvas, (TGE_Rect){24, 2, 10, 5}, " NEXT ",
                    TGE_COLOR_YELLOW);

    tge_draw_text(canvas, 25, 9, " SCORE ", TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_printf(canvas, 25, 10, TGE_COLOR_WHITE, TGE_COLOR_DEFAULT, "%6d",
               g->world.score);
    tge_draw_text(canvas, 25, 12, " LEVEL ", TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_printf(canvas, 25, 13, TGE_COLOR_WHITE, TGE_COLOR_DEFAULT, "%6d",
               g->world.level);
    tge_draw_text(canvas, 25, 15, " LINES ", TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_printf(canvas, 25, 16, TGE_COLOR_WHITE, TGE_COLOR_DEFAULT, "%6d",
               g->world.lines);

    const char *controls = " <-> move  W/Up rot  Space drop  P pause  ESC ";
    tge_draw_text(canvas, 1, h - 1, controls, TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);

    if (g->world.state == STATE_OVER) {
        const char *msg = " GAME OVER ";
        const char *again = " [ENTER] retry  [ESC] menu ";
        tge_fill_rect(canvas, 0, OY + ROWS / 2 - 1, w, 3, ' ', TGE_COLOR_DEFAULT,
                      TGE_COLOR_DEFAULT);
        tge_draw_centered_text(canvas, OY + ROWS / 2 - 1, msg,
                               TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
        tge_draw_centered_text(canvas, OY + ROWS / 2 + 1, again,
                               TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    } else if (g->world.paused) {
        const char *again = " [P] resume ";
        tge_fill_rect(canvas, 0, OY + ROWS / 2 - 1, w, 3, ' ', TGE_COLOR_DEFAULT,
                      TGE_COLOR_DEFAULT);
        tge_draw_centered_text(canvas, OY + ROWS / 2 - 1, " PAUSED ",
                               TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
        tge_draw_centered_text(canvas, OY + ROWS / 2 + 1, again,
                               TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    }
}

static void game_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    TetrisGame *g = (TetrisGame *)tge_game_instance(ctx);
    Tetris *t = &g->world;

    if (ev->type == TGE_EVENT_RESIZE) {
        tetris_resize(t, ev->data.resize.w, ev->data.resize.h);
        if (t->state != STATE_OVER)
            t->paused = true;
        return;
    }
    if (ev->type == TGE_EVENT_TEXT &&
        (ev->data.text.codepoint == 'p' || ev->data.text.codepoint == 'P')) {
        if (t->state != STATE_OVER)
            t->paused = !t->paused;
        return;
    }
    if (t->paused &&
        !(ev->type == TGE_EVENT_KEYDOWN &&
          ev->data.key.keycode == TGE_KEY_ESC))
        return;
    if (ev->type == TGE_EVENT_TEXT) {
        uint32_t cp = ev->data.text.codepoint;
        if (cp == 'w' || cp == 'W') {
            rotate_pressed(t);
        } else if (cp == ' ') {
            if (t->state == STATE_PLAYING)
                hard_drop(t);
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
        case TGE_KEY_LEFT:
            move_cur(t, -1);
            break;
        case TGE_KEY_RIGHT:
            move_cur(t, 1);
            break;
        case TGE_KEY_DOWN:
            if (t->state == STATE_PLAYING)
                soft_drop(t);
            break;
        case TGE_KEY_UP:
            rotate_pressed(t);
            break;
        case TGE_KEY_SPACE:
            if (t->state == STATE_PLAYING)
                hard_drop(t);
            break;
        case TGE_KEY_ENTER:
            if (t->state == STATE_OVER)
                reset(t);
            break;
        case TGE_KEY_ESC:
            if (t->gravity_timer >= 0)
                tge_runtime_cancel_scheduled(TGE_GetRuntime(g_app),
                                             t->gravity_timer);
            t->gravity_timer = -1;
            TGE_PopScene(g_app);
            break;
        default:
            break;
        }
    } else if (ev->type == TGE_EVENT_TIMER &&
               ev->data.timer.id == TMR_GRAVITY) {
        if (t->view.valid && !t->paused)
            gravity_step(t);
    }
}

static void renderer_init(TetrisRenderer *r)
{
    tge_grid_init(&r->board, NULL);
    tge_grid_set_cell_size(&r->board, 2, 1);
    tge_grid_set_origin(&r->board, OX, OY);

    tge_grid_init(&r->preview, NULL);
    tge_grid_set_cell_size(&r->preview, 2, 1);
    tge_grid_set_origin(&r->preview, 24, 2);
}

/* Sync the renderer with the canvas currently being drawn (the app swaps its
 * double buffers). Nothing is reconfigured: geometry and theme persist. */
static void renderer_attach(TetrisRenderer *r, TGE_Canvas *canvas)
{
    tge_grid_attach(&r->board, canvas);
    tge_grid_attach(&r->preview, canvas);
}

static void renderer_draw_board(TetrisRenderer *r, const Tetris *t)
{
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (t->board[y][x])
                tge_grid_set_cell(&r->board, x, y,
                                  tge_color_indexed(
                                      kPieces[t->board[y][x] - 1].color),
                                  TGE_COLOR_DEFAULT);
}

static void renderer_draw_piece(TetrisRenderer *r, const Tetris *t)
{
    TGE_Color col = tge_color_indexed(t->cur.color);
    for (int y = 0; y < t->cur.n; y++)
        for (int x = 0; x < t->cur.n; x++)
            if (t->cur.cells[y][x])
                tge_grid_set_cell(&r->board, t->pos.x + x, t->pos.y + y, col,
                                  TGE_COLOR_DEFAULT);
}

static void renderer_draw_next(TetrisRenderer *r, const Tetris *t)
{
    const Tetromino *nx = &kPieces[t->next];
    int npx = (5 - nx->n) / 2;
    int npy = (5 - nx->n) / 2;
    for (int y = 0; y < nx->n; y++)
        for (int x = 0; x < nx->n; x++)
            if (nx->cells[y][x])
                tge_grid_set_cell(&r->preview, npx + x, npy + y,
                                  tge_color_indexed(nx->color),
                                  TGE_COLOR_DEFAULT);
}

/* One-shot constructor for a fresh game: permanent renderer geometry plus a
 * reset world. Kept together so future game/renderer state has a single
 * setup point. */
static void tetris_game_init(TetrisGame *g)
{
    renderer_init(&g->renderer);
    g->world.gravity_timer = -1;
    g->world.last_w = -1;
    g->world.last_h = -1;
    tge_view_init(&g->world.view, MIN_FW, MIN_FH);
    reset(&g->world);
}

/* Recompute the playfield gate for a new surface size. The board is a fixed
 * 10x20 so there is nothing to clamp on resize: FIRST_VALID starts a fresh
 * game, RESIZED keeps it running, INVALID shows "too small". */
static void tetris_resize(Tetris *t, int w, int h)
{
    t->last_w = w;
    t->last_h = h;
    switch (tge_view_update(&t->view, w, h)) {
    case TGE_VIEW_FIRST_VALID:
        reset(t);
        break;
    case TGE_VIEW_RESIZED:
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static void reset(Tetris *t)
{
    memset(t->board, 0, sizeof(t->board));
    t->score = 0;
    t->level = 1;
    t->lines = 0;
    t->state = STATE_PLAYING;
    t->paused = false;
    t->next = rand() % 7;
    tge_timer_init(&t->rot, ROTATE_DEBOUNCE);
    /* Preload the cooldown so the very first rotation after a reset is
     * immediate (the old `last_rotate = -ROTATE_DEBOUNCE` did the same). */
    tge_timer_update(&t->rot, ROTATE_DEBOUNCE);
    set_gravity(t);
    spawn(t);
}

static void move_cur(Tetris *t, int dx)
{
    if (t->state != STATE_PLAYING)
        return;
    TGE_Vec2i np = tge_vec2i_add(t->pos, tge_vec2i(dx, 0));
    if (fits(t, np))
        t->pos = np;
}

static void try_rotate(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    Tetromino rot = t->cur;
    rotate_cw(&rot);
    static const int kicks[5] = { 0, -1, 1, -2, 2 };
    for (int i = 0; i < 5; i++) {
        TGE_Vec2i np = tge_vec2i_add(t->pos, tge_vec2i(kicks[i], 0));
        if (fits_shape(&rot, t, np)) {
            t->cur = rot;
            t->pos = np;
            return;
        }
    }
}

/* Classic cooldown: at most one rotation every ROTATE_DEBOUNCE seconds no
 * matter how fast the key repeats. The previous version used a sliding
 * window where each press restarted the wait; the cooldown is simpler and
 * is what a player expects. */
static void rotate_pressed(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    if (!tge_timer_tick(&t->rot))
        return;
    tge_timer_reset(&t->rot);
    try_rotate(t);
}

static void hard_drop(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    TGE_Vec2i np = t->pos;
    while (fits(t, tge_vec2i_add(np, tge_vec2i(0, 1))))
        np = tge_vec2i_add(np, tge_vec2i(0, 1));
    int d = np.y - t->pos.y;
    t->pos = np;
    t->score += 2 * d;
    lock(t);
}

static void soft_drop(Tetris *t)
{
    if (fits(t, tge_vec2i_add(t->pos, tge_vec2i(0, 1)))) {
        t->pos.y++;
        t->score++;
    }
}

static void gravity_step(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    if (fits(t, tge_vec2i_add(t->pos, tge_vec2i(0, 1))))
        t->pos.y++;
    else
        lock(t);
}

static void rotate_cw(Tetromino *p)
{
    uint8_t tmp[4][4];
    for (int y = 0; y < p->n; y++)
        for (int x = 0; x < p->n; x++)
            tmp[x][p->n - 1 - y] = p->cells[y][x];
    memcpy(p->cells, tmp, sizeof(p->cells));
}

static int fits_shape(const Tetromino *p, const Tetris *t, TGE_Vec2i pos)
{
    for (int y = 0; y < p->n; y++)
        for (int x = 0; x < p->n; x++)
            if (p->cells[y][x]) {
                int bx = pos.x + x, by = pos.y + y;
                if (bx < 0 || bx >= COLS || by < 0 || by >= ROWS)
                    return 0;
                if (t->board[by][bx])
                    return 0;
            }
    return 1;
}

static int fits(Tetris *t, TGE_Vec2i pos)
{
    return fits_shape(&t->cur, t, pos);
}

static float gravity_interval(int level)
{
    float s = 0.9f * powf(0.8f, (float)level);
    if (s < 0.08f)
        s = 0.08f;
    return s;
}

static void set_gravity(Tetris *t)
{
    TGE_Runtime *rt = TGE_GetRuntime(g_app);
    if (t->gravity_timer >= 0)
        tge_runtime_cancel_scheduled(rt, t->gravity_timer);
    t->gravity_timer = tge_runtime_call_every(rt, gravity_interval(t->level),
                                              TMR_GRAVITY, TGE_TIMER_NORMAL);
}

static void game_over(Tetris *t)
{
    t->state = STATE_OVER;
    if (t->gravity_timer >= 0) {
        tge_runtime_cancel_scheduled(TGE_GetRuntime(g_app), t->gravity_timer);
        t->gravity_timer = -1;
    }
}

static void clear_lines(Tetris *t)
{
    int n = 0;
    for (int y = ROWS - 1; y >= 0; y--) {
        int full = 1;
        for (int x = 0; x < COLS; x++)
            if (!t->board[y][x]) {
                full = 0;
                break;
            }
        if (!full)
            continue;
        n++;
        for (int yy = y; yy > 0; yy--)
            memcpy(t->board[yy], t->board[yy - 1], sizeof(t->board[0]));
        memset(t->board[0], 0, sizeof(t->board[0]));
        y++;
    }
    if (!n)
        return;
    static const int pts[5] = { 0, POINTS_L1, POINTS_L2, POINTS_L3, POINTS_L4 };
    t->score += pts[n] * t->level;
    t->lines += n;
    int lvl = t->lines / 10 + 1;
    if (lvl != t->level) {
        t->level = lvl;
        set_gravity(t);
    }
}

static void spawn(Tetris *t)
{
    t->cur = kPieces[t->next];
    t->next = rand() % 7;
    t->pos = tge_vec2i((COLS - t->cur.n) / 2, 0);
    if (!fits(t, t->pos))
        game_over(t);
}

static void lock(Tetris *t)
{
    for (int y = 0; y < t->cur.n; y++)
        for (int x = 0; x < t->cur.n; x++)
            if (t->cur.cells[y][x])
                t->board[t->pos.y + y][t->pos.x + x] = t->cur.type + 1;
    clear_lines(t);
    if (t->state == STATE_PLAYING)
        spawn(t);
}
