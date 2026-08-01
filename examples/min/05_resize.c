#include "tge/tge.h"
#include <stdio.h>
#include <string.h>

static char info[128] = "resize the terminal window...";

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 1, "resize - TGE_EVENT_RESIZE",
                  TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 3, info, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    snprintf(info, sizeof(info), "current size: %dx%d", w, h);
    tge_draw_text(canvas, 2, h - 2, "[ESC] quit", TGE_COLOR_MAGENTA,
                  TGE_COLOR_BLACK);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    if (ev->type == TGE_EVENT_RESIZE) {
        snprintf(info, sizeof(info), "RESIZE -> %dx%d", ev->data.resize.w,
                 ev->data.resize.h);
    } else if (ev->type == TGE_EVENT_KEYDOWN &&
               ev->data.key.keycode == TGE_KEY_ESC) {
        TGE_Quit(app);
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 10, "TGE resize");
    if (!app)
        return 1;
    TGE_Run(app, NULL, NULL, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
