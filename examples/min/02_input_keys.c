#include "tge/tge.h"
#include <stdio.h>
#include <string.h>

static char info[160];

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 1, "input - press keys and arrows",
                  TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 3, info, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, h - 2, "[ESC] quit", TGE_COLOR_MAGENTA,
                  TGE_COLOR_BLACK);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    switch (ev->type) {
    case TGE_EVENT_KEYDOWN:
        snprintf(info, sizeof(info), "KEYDOWN keycode=%d mod=%d",
                 ev->data.key.keycode, ev->data.key.mod);
        if (ev->data.key.keycode == TGE_KEY_ESC)
            TGE_Quit(app);
        break;
    case TGE_EVENT_KEYUP:
        snprintf(info, sizeof(info), "KEYUP keycode=%d", ev->data.key.keycode);
        break;
    case TGE_EVENT_TEXT:
        if (ev->data.text.codepoint < 0x80)
            snprintf(info, sizeof(info), "TEXT codepoint=%u ('%c')",
                     ev->data.text.codepoint,
                     (int)ev->data.text.codepoint);
        else
            snprintf(info, sizeof(info), "TEXT codepoint=%u", ev->data.text.codepoint);
        break;
    default:
        snprintf(info, sizeof(info), "EVENT type=%d", (int)ev->type);
        break;
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 10, "TGE input");
    if (!app)
        return 1;
    snprintf(info, sizeof(info), "waiting for input...");
    TGE_Run(app, NULL, NULL, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
