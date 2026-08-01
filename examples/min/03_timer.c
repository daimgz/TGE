#include "tge/tge.h"
#include <stdio.h>
#include <string.h>

#define EVENT_TICK  100
#define EVENT_ALERT 101

static char info[160];
static int ticks = 0;

static void init(TGE_App *app)
{
    TGE_Runtime *rt = TGE_GetRuntime(app);
    tge_runtime_call_every(rt, 0.5, EVENT_TICK, TGE_TIMER_NORMAL);
    tge_runtime_call_later(rt, 3.0, EVENT_ALERT, TGE_TIMER_HIGH);
    snprintf(info, sizeof(info), "tick every 0.5s, alert in 3.0s");
}

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 1, "timers - call_every / call_later",
                  TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 3, info, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 5, "one-shot alert fires once and stops",
                  TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, h - 2, "[ESC] quit", TGE_COLOR_MAGENTA,
                  TGE_COLOR_BLACK);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    if (ev->type == TGE_EVENT_TIMER) {
        if (ev->data.timer.id == EVENT_TICK) {
            ticks++;
            snprintf(info, sizeof(info), "tick #%d", ticks);
        } else if (ev->data.timer.id == EVENT_ALERT) {
            snprintf(info, sizeof(info), "ALERT (one-shot, priority %d)",
                     ev->data.timer.priority);
        }
    } else if (ev->type == TGE_EVENT_KEYDOWN &&
               ev->data.key.keycode == TGE_KEY_ESC) {
        TGE_Quit(app);
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 10, "TGE timers");
    if (!app)
        return 1;
    TGE_Run(app, init, NULL, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
