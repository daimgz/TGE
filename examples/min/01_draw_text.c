#include "tge/tge.h"
#include <string.h>

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 1, "draw_text - Canvas + Renderer + Runtime",
                  TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 3, "Latin: Caf, andal el nino",
                  TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 4, "UTF-8: caf\xc3\xa9 a\xc3\xb1o \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
                  TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 6, "Wide chars (CJK) take 2 columns",
                  TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, h - 2, "[ESC] quit", TGE_COLOR_MAGENTA,
                  TGE_COLOR_DEFAULT);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    if (ev->type == TGE_EVENT_KEYDOWN &&
        ev->data.key.keycode == TGE_KEY_ESC) {
        TGE_Quit(app);
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 14, "TGE draw_text");
    if (!app)
        return 1;
    TGE_Run(app, NULL, NULL, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
