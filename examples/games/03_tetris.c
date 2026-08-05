#include "tge/tge.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#define WIN_W 40
#define WIN_H 24

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

static const Tetromino kPieces[7] = {
    { 4, { {0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0} }, 0, 6 },
    { 2, { {1,1,0,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, 1, 3 },
    { 3, { {0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 2, 5 },
    { 3, { {0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0} }, 3, 2 },
    { 3, { {1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 4, 1 },
    { 3, { {1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 5, 4 },
    { 3, { {0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0} }, 6, 7 },
};

typedef struct {
    int board[ROWS][COLS];
    Tetromino cur;
    int cur_x, cur_y;
    int next;
    int score;
    int level;
    int lines;
    int gravity_timer;
    TetrisState state;
    float now;
    float last_rotate;
} Tetris;

static TGE_App *g_app = NULL;
static TGE_Scene g_menu;
static TGE_Scene g_game;
static Tetris g_tetris;

static void rotate_cw(Tetromino *p)
{
    uint8_t tmp[4][4];
    for (int y = 0; y < p->n; y++)
        for (int x = 0; x < p->n; x++)
            tmp[x][p->n - 1 - y] = p->cells[y][x];
    memcpy(p->cells, tmp, sizeof(p->cells));
}

static int fits_shape(const Tetromino *p, const Tetris *t, int px, int py)
{
    for (int y = 0; y < p->n; y++)
        for (int x = 0; x < p->n; x++)
            if (p->cells[y][x]) {
                int bx = px + x, by = py + y;
                if (bx < 0 || bx >= COLS || by < 0 || by >= ROWS)
                    return 0;
                if (t->board[by][bx])
                    return 0;
            }
    return 1;
}

static int fits(Tetris *t, int px, int py)
{
    return fits_shape(&t->cur, t, px, py);
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
    t->cur_x = (COLS - t->cur.n) / 2;
    t->cur_y = 0;
    if (!fits(t, t->cur_x, t->cur_y))
        game_over(t);
}

static void lock(Tetris *t)
{
    for (int y = 0; y < t->cur.n; y++)
        for (int x = 0; x < t->cur.n; x++)
            if (t->cur.cells[y][x])
                t->board[t->cur_y + y][t->cur_x + x] = t->cur.type + 1;
    clear_lines(t);
    if (t->state == STATE_PLAYING)
        spawn(t);
}

static void reset(Tetris *t)
{
    memset(t->board, 0, sizeof(t->board));
    t->score = 0;
    t->level = 1;
    t->lines = 0;
    t->state = STATE_PLAYING;
    t->now = 0.0f;
    t->last_rotate = -ROTATE_DEBOUNCE;
    t->next = rand() % 7;
    set_gravity(t);
    spawn(t);
}

static void move_cur(Tetris *t, int dx)
{
    if (t->state != STATE_PLAYING)
        return;
    if (fits(t, t->cur_x + dx, t->cur_y))
        t->cur_x += dx;
}

static void try_rotate(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    Tetromino rot = t->cur;
    rotate_cw(&rot);
    static const int kicks[5] = { 0, -1, 1, -2, 2 };
    for (int i = 0; i < 5; i++)
        if (fits_shape(&rot, t, t->cur_x + kicks[i], t->cur_y)) {
            t->cur = rot;
            t->cur_x += kicks[i];
            return;
        }
}

static void rotate_pressed(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    if (t->now - t->last_rotate < ROTATE_DEBOUNCE) {
        t->last_rotate = t->now;
        return;
    }
    t->last_rotate = t->now;
    try_rotate(t);
}

static void hard_drop(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    int d = 0;
    while (fits(t, t->cur_x, t->cur_y + d + 1))
        d++;
    t->cur_y += d;
    t->score += 2 * d;
    lock(t);
}

static void soft_drop(Tetris *t)
{
    if (fits(t, t->cur_x, t->cur_y + 1)) {
        t->cur_y++;
        t->score++;
    }
}

static void gravity_step(Tetris *t)
{
    if (t->state != STATE_PLAYING)
        return;
    if (fits(t, t->cur_x, t->cur_y + 1))
        t->cur_y++;
    else
        lock(t);
}

static void game_init(TGE_Scene *scene)
{
    Tetris *t = (Tetris *)scene->userdata;
    t->gravity_timer = -1;
    reset(t);
}

static void game_update(TGE_Scene *scene, float dt)
{
    Tetris *t = (Tetris *)scene->userdata;
    t->now += dt;
}

static void draw_block(TGE_Canvas *c, int x, int y, TGE_Color col)
{
    tge_set_cell(c, x, y, 0x2588, col, TGE_COLOR_BLACK);
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    Tetris *t = (Tetris *)scene->userdata;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    tge_draw_frame(canvas, OX - 1, OY - 1, COLS + 2, ROWS + 2, TGE_COLOR_CYAN,
                   TGE_COLOR_BLACK);

    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            if (t->board[y][x])
                draw_block(canvas, OX + x, OY + y,
                           tge_color_indexed(kPieces[t->board[y][x] - 1].color));

    TGE_Color col = tge_color_indexed(t->cur.color);
    for (int y = 0; y < t->cur.n; y++)
        for (int x = 0; x < t->cur.n; x++)
            if (t->cur.cells[y][x])
                draw_block(canvas, OX + t->cur_x + x, OY + t->cur_y + y, col);

    tge_draw_text(canvas, 17, 1, " NEXT ", TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_draw_frame(canvas, 16, 2, 10, 5, TGE_COLOR_BLUE, TGE_COLOR_BLACK);
    const Tetromino *nx = &kPieces[t->next];
    int px = 16 + (10 - nx->n) / 2;
    int py = 2 + (5 - nx->n) / 2;
    for (int y = 0; y < nx->n; y++)
        for (int x = 0; x < nx->n; x++)
            if (nx->cells[y][x])
                draw_block(canvas, px + x, py + y, tge_color_indexed(nx->color));

    tge_draw_text(canvas, 17, 9, " SCORE ", TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_printf(canvas, 17, 10, TGE_COLOR_WHITE, TGE_COLOR_BLACK, "%6d",
               t->score);
    tge_draw_text(canvas, 17, 12, " LEVEL ", TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_printf(canvas, 17, 13, TGE_COLOR_WHITE, TGE_COLOR_BLACK, "%6d",
               t->level);
    tge_draw_text(canvas, 17, 15, " LINES ", TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_printf(canvas, 17, 16, TGE_COLOR_WHITE, TGE_COLOR_BLACK, "%6d",
               t->lines);

    const char *controls = " <-> move  W/Up rot  Space drop  ESC ";
    tge_draw_text(canvas, 1, h - 1, controls, TGE_COLOR_GREEN, TGE_COLOR_BLACK);

    if (t->state == STATE_OVER) {
        const char *msg = " GAME OVER ";
        const char *again = " [ENTER] retry  [ESC] menu ";
        tge_fill_rect(canvas, 0, OY + ROWS / 2 - 1, w, 3, ' ', TGE_COLOR_BLACK,
                      TGE_COLOR_BLACK);
        tge_draw_text(canvas, (w - (int)strlen(msg)) / 2, OY + ROWS / 2 - 1,
                      msg, TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
        tge_draw_text(canvas, (w - (int)strlen(again)) / 2, OY + ROWS / 2 + 1,
                      again, TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    }
}

static void game_event(TGE_Scene *scene, TGE_Event *ev)
{
    Tetris *t = (Tetris *)scene->userdata;

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
        gravity_step(t);
    }
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

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(title)) / 2, h / 2 - 3, title,
                  TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(c1)) / 2, h / 2, c1,
                  TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(c2)) / 2, h / 2 + 1, c2,
                  TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(start)) / 2, h / 2 + 3, start,
                  TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
}

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
    if (enter)
        TGE_PushScene(g_app, &g_game);
}

static void init_app(TGE_App *app)
{
    g_app = app;

    memset(&g_menu, 0, sizeof(g_menu));
    g_menu.opaque = false;
    g_menu.draw = title_draw;
    g_menu.event = title_event;

    memset(&g_game, 0, sizeof(g_game));
    g_game.opaque = true;
    g_game.userdata = &g_tetris;
    g_game.init = game_init;
    g_game.update = game_update;
    g_game.draw = game_draw;
    g_game.event = game_event;

    TGE_PushScene(app, &g_menu);
}

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Tetris");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
