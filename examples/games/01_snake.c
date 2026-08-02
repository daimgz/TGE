#include "tge/tge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 40
#define WIN_H 20
#define MAX_LEN 256
#define MOVE_INTERVAL 0.10f
#define DIR_QUEUE 4

typedef struct { int x, y; } Point;

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

typedef struct {
    Point body[MAX_LEN];
    int len;
    int dx, dy;
    Point dir_queue[DIR_QUEUE];
    int dir_head, dir_tail;
    Point food;
    int score;
    SnakeState state;
    float acc;
} GameState;

static TGE_App *g_app = NULL;

static void push_dir(GameState *s, int dx, int dy)
{
    int next = (s->dir_tail + 1) % DIR_QUEUE;
    if (next != s->dir_head) {
        s->dir_queue[s->dir_tail].x = dx;
        s->dir_queue[s->dir_tail].y = dy;
        s->dir_tail = next;
    }
}

static bool spawn_food(GameState *s)
{
    if ((WIN_W - 2) * (WIN_H - 2) - s->len <= 0)
        return false;
    for (;;) {
        Point p;
        p.x = 1 + rand() % (WIN_W - 2);
        p.y = 1 + rand() % (WIN_H - 2);
        bool free_spot = true;
        for (int i = 0; i < s->len; i++) {
            if (s->body[i].x == p.x && s->body[i].y == p.y) {
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
    int cx = WIN_W / 2;
    int cy = WIN_H / 2;
    s->len = 3;
    s->body[0].x = cx;
    s->body[0].y = cy;
    s->body[1].x = cx - 1;
    s->body[1].y = cy;
    s->body[2].x = cx - 2;
    s->body[2].y = cy;
    s->dx = 1;
    s->dy = 0;
    s->dir_head = 0;
    s->dir_tail = 0;
    s->score = 0;
    s->state = SNAKE_RUNNING;
    s->acc = 0.0f;
    spawn_food(s);
}

static bool snake_step(GameState *s)
{
    while (s->dir_head != s->dir_tail) {
        Point d = s->dir_queue[s->dir_head];
        s->dir_head = (s->dir_head + 1) % DIR_QUEUE;
        if (d.x == -s->dx && d.y == -s->dy)
            continue;
        if (d.x == s->dx && d.y == s->dy)
            continue;
        s->dx = d.x;
        s->dy = d.y;
        break;
    }

    Point nh;
    nh.x = s->body[0].x + s->dx;
    nh.y = s->body[0].y + s->dy;

    if (nh.x <= 0 || nh.x >= WIN_W - 1 || nh.y <= 0 || nh.y >= WIN_H - 1)
        return false;

    bool ate = (nh.x == s->food.x && nh.y == s->food.y);
    int check_to = s->len - (ate ? 0 : 1);
    for (int i = 0; i < check_to; i++) {
        if (s->body[i].x == nh.x && s->body[i].y == nh.y)
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
    if (s->state != SNAKE_RUNNING)
        return;
    s->acc += dt;
    while (s->acc >= MOVE_INTERVAL) {
        s->acc -= MOVE_INTERVAL;
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

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    char hud[32];
    snprintf(hud, sizeof(hud), " SCORE: %d ", s->score);
    tge_draw_text(canvas, 2, 0, hud, TGE_COLOR_YELLOW, TGE_COLOR_BLACK);

    for (int i = 0; i < s->len; i++) {
        uint32_t ch = (i == 0) ? '@' : 'o';
        TGE_Color fg = (i == 0) ? TGE_COLOR_GREEN : TGE_COLOR_GREEN;
        tge_set_cell(canvas, s->body[i].x, s->body[i].y, ch, fg,
                     TGE_COLOR_BLACK);
    }
    tge_set_cell(canvas, s->food.x, s->food.y, '*', TGE_COLOR_RED,
                 TGE_COLOR_BLACK);

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
    if (ev->type == TGE_EVENT_TEXT) {
        switch (ev->data.text.codepoint) {
        case 'w': case 'W': push_dir(s, 0, -1); break;
        case 's': case 'S': push_dir(s, 0, 1); break;
        case 'a': case 'A': push_dir(s, -1, 0); break;
        case 'd': case 'D': push_dir(s, 1, 0); break;
        case 'q': case 'Q':
            if (s->state == SNAKE_OVER)
                TGE_Quit(g_app);
            break;
        case 13:
            if (s->state == SNAKE_OVER)
                snake_reset(s);
            break;
        default: break;
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
        case TGE_KEY_UP: push_dir(s, 0, -1); break;
        case TGE_KEY_DOWN: push_dir(s, 0, 1); break;
        case TGE_KEY_LEFT: push_dir(s, -1, 0); break;
        case TGE_KEY_RIGHT: push_dir(s, 1, 0); break;
        case TGE_KEY_ESC:
            TGE_PopScene(g_app);
            break;
        default: break;
        }
    }
}

static void game_destroy(TGE_Scene *scene)
{
    free(scene->userdata);
    free(scene);
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
    if (enter) {
        TGE_Scene *game = (TGE_Scene *)calloc(1, sizeof(TGE_Scene));
        GameState *s = (GameState *)calloc(1, sizeof(GameState));
        snake_reset(s);
        game->userdata = s;
        game->opaque = true;
        game->update = game_update;
        game->draw = game_draw;
        game->event = game_event;
        game->destroy = game_destroy;
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
    TGE_PushScene(app, title);
}

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Snake");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
