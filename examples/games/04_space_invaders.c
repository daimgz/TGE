#include "tge/tge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 40
#define WIN_H 24

#define GRID_ROWS 5
#define GRID_COLS 8
#define MAX_EBULLETS 4

#define PLAYER_Y (WIN_H - 3)
#define PLAYER_W 3

#define TMR_MOVE 201

typedef enum { SI_PLAYING = 0, SI_WAVE, SI_OVER } SiState;

typedef struct {
    int alive[GRID_ROWS][GRID_COLS];
    int inv_x, inv_y;
    int dir;
    float move_acc;
    float move_interval;
    int count;

    int px;
    int lives;
    int score;
    int level;

    int pb_active, pb_x, pb_y;
    int eb_x[MAX_EBULLETS], eb_y[MAX_EBULLETS];
    int eb_active[MAX_EBULLETS];
    float shot_acc;
    float shot_interval;
    float bullet_acc;
    float wave_timer;
    float hit_flash;
    SiState state;
} Invaders;

static TGE_App *g_app = NULL;
static TGE_Scene g_menu;
static TGE_Scene g_game;
static Invaders g_inv;

static int score_of_row(int row)
{
    return row == 0 ? 30 : (row <= 2 ? 20 : 10);
}

static uint32_t char_of_row(int row)
{
    return row == 0 ? 0x25B2 : (row <= 2 ? 0x25AC : 0x2584);
}

static void spawn_wave(Invaders *t)
{
    for (int y = 0; y < GRID_ROWS; y++)
        for (int x = 0; x < GRID_COLS; x++)
            t->alive[y][x] = 1;
    t->count = GRID_ROWS * GRID_COLS;
    t->inv_x = 1;
    t->inv_y = 1;
    t->dir = 1;
    float iv = 0.45f * 0.85f * (float)(t->level - 1);
    if (iv > 0.45f)
        iv = 0.45f;
    if (iv < 0.08f)
        iv = 0.08f;
    t->move_interval = iv;
    for (int i = 0; i < MAX_EBULLETS; i++)
        t->eb_active[i] = 0;
    t->pb_active = 0;
    t->state = SI_PLAYING;
}

static void reset(Invaders *t)
{
    t->score = 0;
    t->level = 1;
    t->lives = 3;
    t->px = WIN_W / 2;
    t->hit_flash = 0.0f;
    t->shot_acc = 0.0f;
    t->bullet_acc = 0.0f;
    t->shot_interval = 0.8f;
    spawn_wave(t);
}

static void game_over(Invaders *t)
{
    t->state = SI_OVER;
    t->pb_active = 0;
    for (int i = 0; i < MAX_EBULLETS; i++)
        t->eb_active[i] = 0;
}

static void start_wave(Invaders *t)
{
    t->level++;
    spawn_wave(t);
}

static void clear_all_bullets(Invaders *t)
{
    t->pb_active = 0;
    for (int i = 0; i < MAX_EBULLETS; i++)
        t->eb_active[i] = 0;
}

static void hit_player(Invaders *t)
{
    t->lives--;
    clear_all_bullets(t);
    if (t->lives <= 0) {
        game_over(t);
    } else {
        t->hit_flash = 0.9f;
        t->px = WIN_W / 2;
    }
}

static void step_invaders(Invaders *t)
{
    if (t->count <= 0)
        return;
    int minx = 99, maxx = -1, maxy = -1;
    for (int y = 0; y < GRID_ROWS; y++)
        for (int x = 0; x < GRID_COLS; x++)
            if (t->alive[y][x]) {
                if (x * 2 < minx)
                    minx = x * 2;
                if (x * 2 + 1 > maxx)
                    maxx = x * 2 + 1;
                if (y > maxy)
                    maxy = y;
            }
    int left = t->inv_x + minx;
    int right = t->inv_x + maxx;
    if (t->dir > 0 && right >= WIN_W - 2) {
        t->dir = -1;
        t->inv_y++;
    } else if (t->dir < 0 && left <= 1) {
        t->dir = 1;
        t->inv_y++;
    } else {
        t->inv_x += t->dir;
    }
    if (t->inv_y + maxy >= PLAYER_Y)
        game_over(t);
}

static void step_player_bullet(Invaders *t)
{
    if (!t->pb_active)
        return;
    t->pb_y--;
    if (t->pb_y < 1) {
        t->pb_active = 0;
        return;
    }
    for (int y = 0; y < GRID_ROWS && t->pb_active; y++)
        for (int x = 0; x < GRID_COLS && t->pb_active; x++)
            if (t->alive[y][x] && t->pb_y == t->inv_y + y &&
                (t->pb_x == t->inv_x + x * 2 ||
                 t->pb_x == t->inv_x + x * 2 + 1)) {
                t->alive[y][x] = 0;
                t->count--;
                t->score += score_of_row(y);
                t->pb_active = 0;
                if (t->count <= 0) {
                    t->state = SI_WAVE;
                    t->wave_timer = 1.2f;
                }
            }
}

static void step_enemy_bullets(Invaders *t)
{
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!t->eb_active[i])
            continue;
        t->eb_y[i]++;
        if (t->eb_y[i] > PLAYER_Y) {
            t->eb_active[i] = 0;
            continue;
        }
        if (t->eb_y[i] == PLAYER_Y && t->eb_x[i] >= t->px - 1 &&
            t->eb_x[i] <= t->px + 1) {
            t->eb_active[i] = 0;
            hit_player(t);
            continue;
        }
        if (t->pb_active && t->eb_y[i] == t->pb_y &&
            t->eb_x[i] == t->pb_x) {
            t->pb_active = 0;
            t->eb_active[i] = 0;
        }
    }
}

static void enemy_shoot(Invaders *t)
{
    if (t->count <= 0)
        return;
    int active_bullets = 0;
    for (int i = 0; i < MAX_EBULLETS; i++)
        if (t->eb_active[i])
            active_bullets++;
    if (active_bullets >= MAX_EBULLETS)
        return;
    int col = rand() % GRID_COLS;
    for (int tries = 0; tries < GRID_COLS; tries++) {
        int alive_in_col = 0;
        for (int y = 0; y < GRID_ROWS; y++)
            if (t->alive[y][col]) {
                alive_in_col = 1;
                break;
            }
        if (alive_in_col)
            break;
        col = (col + 1) % GRID_COLS;
    }
    int row = -1;
    for (int y = 0; y < GRID_ROWS; y++)
        if (t->alive[y][col])
            row = y;
    if (row < 0)
        return;
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!t->eb_active[i]) {
            t->eb_active[i] = 1;
            t->eb_x[i] = t->inv_x + col * 2 + (rand() % 2);
            t->eb_y[i] = t->inv_y + row;
            break;
        }
    }
    t->shot_interval = 0.3f + (float)(rand() % 70) / 100.0f +
                       (float)(t->level - 1) * 0.05f;
    t->shot_acc = 0.0f;
}

static void si_init(TGE_Scene *scene)
{
    reset((Invaders *)scene->userdata);
}

static void si_update(TGE_Scene *scene, float dt)
{
    Invaders *t = (Invaders *)scene->userdata;

    if (t->hit_flash > 0.0f) {
        t->hit_flash -= dt;
        if (t->hit_flash < 0.0f)
            t->hit_flash = 0.0f;
    }

    if (t->state == SI_WAVE) {
        t->wave_timer -= dt;
        if (t->wave_timer <= 0.0f)
            start_wave(t);
        return;
    }
    if (t->state != SI_PLAYING)
        return;

    t->move_acc += dt;
    while (t->move_acc >= t->move_interval) {
        t->move_acc -= t->move_interval;
        step_invaders(t);
        if (t->state != SI_PLAYING)
            break;
    }

    t->shot_acc += dt;
    while (t->shot_acc >= t->shot_interval) {
        t->shot_acc -= t->shot_interval;
        enemy_shoot(t);
        if (t->state != SI_PLAYING)
            break;
    }

    t->bullet_acc += dt;
    while (t->bullet_acc >= 0.03f) {
        t->bullet_acc -= 0.03f;
        step_player_bullet(t);
        step_enemy_bullets(t);
        if (t->state != SI_PLAYING)
            break;
    }
}

static void draw_hud(TGE_Canvas *canvas, const Invaders *t)
{
    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK,
               " SCORE %06d ", t->score);
    tge_printf(canvas, 14, 0, TGE_COLOR_CYAN, TGE_COLOR_BLACK, " LV %d ",
               t->level);
    tge_printf(canvas, 22, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK, " LIVES %d ",
               t->lives);
    tge_printf(canvas, 33, 0, TGE_COLOR_WHITE, TGE_COLOR_BLACK, " LEF %02d ",
               t->count);
}

static void si_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    Invaders *t = (Invaders *)scene->userdata;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    draw_hud(canvas, t);

    for (int y = 0; y < GRID_ROWS; y++)
        for (int x = 0; x < GRID_COLS; x++)
            if (t->alive[y][x]) {
                uint32_t ch = char_of_row(y);
                TGE_Color col = y == 0 ? TGE_COLOR_MAGENTA
                                : (y <= 2 ? TGE_COLOR_YELLOW
                                          : TGE_COLOR_GREEN);
                tge_set_cell(canvas, t->inv_x + x * 2, t->inv_y + y, ch, col,
                             TGE_COLOR_BLACK);
                tge_set_cell(canvas, t->inv_x + x * 2 + 1, t->inv_y + y, ch,
                             col, TGE_COLOR_BLACK);
            }

    if (t->hit_flash <= 0.0f) {
        tge_set_cell(canvas, t->px - 1, PLAYER_Y, '\\', TGE_COLOR_GREEN,
                     TGE_COLOR_BLACK);
        tge_set_cell(canvas, t->px, PLAYER_Y, '^', TGE_COLOR_GREEN,
                     TGE_COLOR_BLACK);
        tge_set_cell(canvas, t->px + 1, PLAYER_Y, '/', TGE_COLOR_GREEN,
                     TGE_COLOR_BLACK);
    }

    if (t->pb_active)
        tge_set_cell(canvas, t->pb_x, t->pb_y, '|', TGE_COLOR_WHITE,
                     TGE_COLOR_BLACK);
    for (int i = 0; i < MAX_EBULLETS; i++)
        if (t->eb_active[i])
            tge_set_cell(canvas, t->eb_x[i], t->eb_y[i], '|', TGE_COLOR_RED,
                         TGE_COLOR_BLACK);

    if (t->state == SI_WAVE) {
        char buf[24];
        snprintf(buf, sizeof(buf), " WAVE %d ", t->level + 1);
        tge_draw_text(canvas, (w - (int)strlen(buf)) / 2, h / 2, buf,
                      TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    }

    if (t->state == SI_OVER) {
        const char *msg = " GAME OVER ";
        const char *again = " [ENTER] retry  [ESC] menu  [Q] quit ";
        tge_draw_text(canvas, (w - (int)strlen(msg)) / 2, h / 2 - 2, msg,
                      TGE_COLOR_RED, TGE_COLOR_BLACK);
        tge_draw_text(canvas, (w - (int)strlen(again)) / 2, h / 2, again,
                      TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    }
}

static void si_event(TGE_Scene *scene, TGE_Event *ev)
{
    Invaders *t = (Invaders *)scene->userdata;

    if (ev->type == TGE_EVENT_TEXT) {
        switch (ev->data.text.codepoint) {
        case 'a': case 'A':
            if (t->state == SI_PLAYING && t->px > 2)
                t->px--;
            break;
        case 'd': case 'D':
            if (t->state == SI_PLAYING && t->px < WIN_W - 3)
                t->px++;
            break;
        case 'w': case 'W':
        case ' ':
            if (t->state == SI_PLAYING && !t->pb_active) {
                t->pb_active = 1;
                t->pb_x = t->px;
                t->pb_y = PLAYER_Y - 1;
            }
            break;
        case 'q': case 'Q':
            if (t->state == SI_OVER)
                TGE_Quit(g_app);
            break;
        case 13:
            if (t->state == SI_OVER)
                reset(t);
            break;
        default:
            break;
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
        case TGE_KEY_LEFT:
            if (t->state == SI_PLAYING && t->px > 2)
                t->px--;
            break;
        case TGE_KEY_RIGHT:
            if (t->state == SI_PLAYING && t->px < WIN_W - 3)
                t->px++;
            break;
        case TGE_KEY_UP:
        case TGE_KEY_SPACE:
            if (t->state == SI_PLAYING && !t->pb_active) {
                t->pb_active = 1;
                t->pb_x = t->px;
                t->pb_y = PLAYER_Y - 1;
            }
            break;
        case TGE_KEY_ENTER:
            if (t->state == SI_OVER)
                reset(t);
            break;
        case TGE_KEY_ESC:
            TGE_PopScene(g_app);
            break;
        default:
            break;
        }
    }
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " SPACE INVADERS ";
    const char *controls = " Arrows/WASD move  Space/Up shoot ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(title)) / 2, h / 2 - 2, title,
                  TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(controls)) / 2, h / 2, controls,
                  TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, (w - (int)strlen(start)) / 2, h / 2 + 2, start,
                  TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    bool enter = false;
    if (ev->type == TGE_EVENT_TEXT) {
        if (ev->data.text.codepoint == 13) {
            enter = true;
        } else if (ev->data.text.codepoint == 'q' ||
                   ev->data.text.codepoint == 'Q') {
            TGE_Quit(g_app);
            return;
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN &&
               ev->data.key.keycode == TGE_KEY_ENTER) {
        enter = true;
    }
    if (ev->type == TGE_EVENT_KEYDOWN &&
        ev->data.key.keycode == TGE_KEY_ESC) {
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
    g_game.userdata = &g_inv;
    g_game.init = si_init;
    g_game.update = si_update;
    g_game.draw = si_draw;
    g_game.event = si_event;

    TGE_PushScene(app, &g_menu);
}

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Space Invaders");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
