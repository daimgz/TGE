#include "tge/tge.h"
#include <stdio.h>
#include <string.h>

static char info[128] = "move / click the mouse...";

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 1, "mouse - SGR mode events",
                  TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 3, info, TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, h - 2, "[ESC] quit", TGE_COLOR_MAGENTA,
                  TGE_COLOR_DEFAULT);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    switch (ev->type) {
    case TGE_EVENT_MOUSEDOWN:
        snprintf(info, sizeof(info), "MOUSEDOWN button=%d at (%d,%d)",
                 ev->data.mouse.button, ev->data.mouse.x, ev->data.mouse.y);
        break;
    case TGE_EVENT_MOUSEUP:
        snprintf(info, sizeof(info), "MOUSEUP button=%d at (%d,%d)",
                 ev->data.mouse.button, ev->data.mouse.x, ev->data.mouse.y);
        break;
    case TGE_EVENT_MOUSEMOVE:
        snprintf(info, sizeof(info), "MOUSEMOVE at (%d,%d)", ev->data.mouse.x,
                 ev->data.mouse.y);
        break;
    case TGE_EVENT_KEYDOWN:
        if (ev->data.key.keycode == TGE_KEY_ESC)
            TGE_Quit(app);
        break;
    default:
        break;
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 10, "TGE mouse");
    if (!app)
        return 1;
    TGE_Run(app, NULL, NULL, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
