#include "tge/tge.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_W 80
#define WIN_H 24
#define PADDLE_H 5
#define PADDLE_SPEED 7.0f
#define BALL_SPEED 5.5f
#define BALL_SPEED_MAX 10.0f
#define MAX_VY_RATIO 0.8f
#define WIN_SCORE 5
#define KEY_HOLD 0.12f

#define TMR_COUNTDOWN 100

typedef enum { STATE_COUNTDOWN = 0, STATE_RUNNING, STATE_OVER } PongState;

typedef struct {
    int up, down;
    float t;
} Held;

typedef struct {
    float p1y, p2y;
    float bx, by, bvx, bvy;
    float speed;
    int score1, score2;
    PongState state;
    int countdown;
    int serve_dir;
    int cd_timer;
    float go_flash;
    Held p1, p2;
} Pong;

static TGE_App *g_app = NULL;
static TGE_Scene g_menu;
static TGE_Scene g_game;
static Pong g_pong;

static void start_serve(Pong *g)
{
    TGE_Runtime *rt = TGE_GetRuntime(g_app);
    if (g->cd_timer >= 0)
        tge_runtime_cancel_scheduled(rt, g->cd_timer);
    g->bx = (float)WIN_W / 2.0f;
    g->by = (float)WIN_H / 2.0f;
    g->state = STATE_COUNTDOWN;
    g->countdown = 3;
    g->go_flash = 0.0f;
    g->bvx = 0.0f;
    g->bvy = 0.0f;
    g->cd_timer = tge_runtime_call_every(rt, 1.0, TMR_COUNTDOWN,
                                         TGE_TIMER_NORMAL);
}

static void restart_match(Pong *g)
{
    g->score1 = 0;
    g->score2 = 0;
    g->p1y = (float)(WIN_H - PADDLE_H) / 2.0f;
    g->p2y = (float)(WIN_H - PADDLE_H) / 2.0f;
    g->speed = BALL_SPEED;
    g->serve_dir = (rand() % 2) ? 1 : -1;
    start_serve(g);
}

static void reflect(Pong *g, int dir)
{
    float py = (dir < 0) ? g->p2y : g->p1y;
    float rel = (g->by - (py + PADDLE_H / 2.0f)) / (PADDLE_H / 2.0f);
    if (rel > 1.0f)
        rel = 1.0f;
    else if (rel < -1.0f)
        rel = -1.0f;
    float vy = rel * g->speed * MAX_VY_RATIO;
    float vx = sqrtf(g->speed * g->speed - vy * vy);
    g->bvx = (float)dir * vx;
    g->bvy = vy;
    g->speed += 0.3f;
    if (g->speed > BALL_SPEED_MAX)
        g->speed = BALL_SPEED_MAX;
}

static void game_init(TGE_Scene *scene)
{
    Pong *g = (Pong *)scene->userdata;
    g->cd_timer = -1;
    g->p1.up = g->p1.down = 0;
    g->p2.up = g->p2.down = 0;
    restart_match(g);
}

static void game_update(TGE_Scene *scene, float dt)
{
    Pong *g = (Pong *)scene->userdata;

    g->p1.t += dt;
    g->p2.t += dt;
    if (g->p1.t > KEY_HOLD) {
        g->p1.up = 0;
        g->p1.down = 0;
    }
    if (g->p2.t > KEY_HOLD) {
        g->p2.up = 0;
        g->p2.down = 0;
    }

    if (g->p1.up)
        g->p1y -= PADDLE_SPEED * dt;
    if (g->p1.down)
        g->p1y += PADDLE_SPEED * dt;
    if (g->p2.up)
        g->p2y -= PADDLE_SPEED * dt;
    if (g->p2.down)
        g->p2y += PADDLE_SPEED * dt;

    float max_y = (float)(WIN_H - 2 - PADDLE_H);
    if (g->p1y < 1.0f)
        g->p1y = 1.0f;
    else if (g->p1y > max_y)
        g->p1y = max_y;
    if (g->p2y < 1.0f)
        g->p2y = 1.0f;
    else if (g->p2y > max_y)
        g->p2y = max_y;

    if (g->state == STATE_OVER)
        return;
    if (g->state == STATE_COUNTDOWN)
        return;

    if (g->go_flash > 0.0f)
        g->go_flash -= dt;

    g->bx += g->bvx * dt;
    g->by += g->bvy * dt;

    if (g->by < 1.5f) {
        g->by = 1.5f;
        g->bvy = fabsf(g->bvy);
    } else if (g->by > (float)(WIN_H - 2) - 0.5f) {
        g->by = (float)(WIN_H - 2) - 0.5f;
        g->bvy = -fabsf(g->bvy);
    }

    if (g->bvx < 0.0f && g->bx <= 3.5f &&
        g->by > g->p1y - 0.5f && g->by < g->p1y + PADDLE_H + 0.5f) {
        reflect(g, 1);
        g->bx = 4.0f;
    } else if (g->bvx > 0.0f && g->bx >= (float)(WIN_W - 3) - 0.5f &&
               g->by > g->p2y - 0.5f && g->by < g->p2y + PADDLE_H + 0.5f) {
        reflect(g, -1);
        g->bx = (float)(WIN_W - 4);
    }

    if (g->bx < 1.0f) {
        g->score2++;
        if (g->score2 >= WIN_SCORE) {
            g->state = STATE_OVER;
            if (g->cd_timer >= 0)
                tge_runtime_cancel_scheduled(TGE_GetRuntime(g_app),
                                             g->cd_timer);
            g->cd_timer = -1;
            return;
        }
        g->serve_dir = 1;
        start_serve(g);
    } else if (g->bx > (float)(WIN_W - 2)) {
        g->score1++;
        if (g->score1 >= WIN_SCORE) {
            g->state = STATE_OVER;
            if (g->cd_timer >= 0)
                tge_runtime_cancel_scheduled(TGE_GetRuntime(g_app),
                                             g->cd_timer);
            g->cd_timer = -1;
            return;
        }
        g->serve_dir = -1;
        start_serve(g);
    }
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    Pong *g = (Pong *)scene->userdata;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    tge_fill_rect(canvas, 0, 0, w, h, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    char buf[16];
    tge_printf(canvas, 2, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK, " P1: %d ",
               g->score1);
    snprintf(buf, sizeof(buf), " %d: P2 ", g->score2);
    tge_draw_text(canvas, w - 2 - (int)strlen(buf), 0, buf, TGE_COLOR_RED,
                  TGE_COLOR_BLACK);

    for (int y = 2; y < h - 2; y += 2)
        tge_set_cell(canvas, w / 2, y, '|', TGE_COLOR_BLUE, TGE_COLOR_BLACK);

    for (int i = 0; i < PADDLE_H; i++) {
        tge_set_cell(canvas, 2, (int)g->p1y + i, 0x2588, TGE_COLOR_GREEN,
                     TGE_COLOR_BLACK);
        tge_set_cell(canvas, w - 3, (int)g->p2y + i, 0x2588, TGE_COLOR_RED,
                     TGE_COLOR_BLACK);
    }

    if (g->state == STATE_RUNNING || g->state == STATE_COUNTDOWN) {
        int bx = (int)(g->bx + 0.5f);
        int by = (int)(g->by + 0.5f);
        tge_set_cell(canvas, bx, by, 'o', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    }

    if (g->state == STATE_COUNTDOWN) {
        char n[2] = { (char)('0' + g->countdown), '\0' };
        tge_draw_text(canvas, w / 2, h / 2, n, TGE_COLOR_YELLOW,
                      TGE_COLOR_BLACK);
    } else if (g->state == STATE_RUNNING && g->go_flash > 0.0f) {
        tge_draw_centered_text(canvas, h / 2, " GO! ", TGE_COLOR_YELLOW,
                               TGE_COLOR_BLACK);
    }

    if (g->state == STATE_OVER) {
        const char *msg = (g->score1 >= WIN_SCORE) ? " PLAYER 1 WINS "
                                                   : " PLAYER 2 WINS ";
        const char *again = " [ENTER] rematch  [ESC] menu ";
        tge_fill_rect(canvas, 1, h / 2 - 1, w - 2, 3, ' ', TGE_COLOR_BLACK,
                      TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, h / 2 - 1, msg, TGE_COLOR_YELLOW,
                               TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, h / 2 + 1, again, TGE_COLOR_WHITE,
                               TGE_COLOR_BLACK);
    }
}

static void game_event(TGE_Scene *scene, TGE_Event *ev)
{
    Pong *g = (Pong *)scene->userdata;
    if (ev->type == TGE_EVENT_TEXT) {
        switch (ev->data.text.codepoint) {
        case 'w':
        case 'W':
            g->p1.up = 1;
            g->p1.t = 0.0f;
            break;
        case 's':
        case 'S':
            g->p1.down = 1;
            g->p1.t = 0.0f;
            break;
        default:
            break;
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
        case TGE_KEY_UP:
            g->p2.up = 1;
            g->p2.t = 0.0f;
            break;
        case TGE_KEY_DOWN:
            g->p2.down = 1;
            g->p2.t = 0.0f;
            break;
        case TGE_KEY_ESC:
            TGE_PopScene(g_app);
            break;
        case TGE_KEY_ENTER:
            if (g->state == STATE_OVER)
                restart_match(g);
            break;
        default:
            break;
        }
    } else if (ev->type == TGE_EVENT_TIMER &&
               ev->data.timer.id == TMR_COUNTDOWN) {
        if (g->state == STATE_COUNTDOWN && g->countdown > 0) {
            g->countdown--;
            if (g->countdown == 0) {
                if (g->cd_timer >= 0)
                    tge_runtime_cancel_scheduled(TGE_GetRuntime(g_app),
                                                 g->cd_timer);
                g->cd_timer = -1;
                g->state = STATE_RUNNING;
                g->bvx = (float)g->serve_dir * g->speed;
                g->bvy = 0.0f;
                g->go_flash = 0.5f;
            }
        }
    }
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title = " PONG ";
    const char *controls = " P1: W/S    P2: Up/Down ";
    const char *start = " [ENTER] to start  [Q] to quit ";

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
    g_game.userdata = &g_pong;
    g_game.init = game_init;
    g_game.update = game_update;
    g_game.draw = game_draw;
    g_game.event = game_event;

    TGE_PushScene(app, &g_menu);
}

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Pong");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
