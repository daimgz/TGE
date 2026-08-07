#include "tge/tge.h"
#include <string.h>

static const char *names[8] = {
    "BLACK", "RED", "GREEN", "YELLOW", "BLUE", "MAGENTA", "CYAN", "WHITE"
};

static TGE_Color color_of(int i)
{
    switch (i) {
    case 0:  return TGE_COLOR_BLACK;
    case 1:  return TGE_COLOR_RED;
    case 2:  return TGE_COLOR_GREEN;
    case 3:  return TGE_COLOR_YELLOW;
    case 4:  return TGE_COLOR_BLUE;
    case 5:  return TGE_COLOR_MAGENTA;
    case 6:  return TGE_COLOR_CYAN;
    default: return TGE_COLOR_WHITE;
    }
}

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    tge_draw_text(canvas, 2, 1, "colors - 8 indexed colors",
                  TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    for (int i = 0; i < 8; i++) {
        int y = 3 + i;
        tge_fill_rect(canvas, 2, y, 16, 1, ' ', TGE_COLOR_WHITE, color_of(i));
        tge_draw_text(canvas, 20, y, names[i], TGE_COLOR_WHITE,
                      TGE_COLOR_DEFAULT);
    }
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
    TGE_App *app = TGE_Create(60, 14, "TGE colors");
    if (!app)
        return 1;
    TGE_Run(app, NULL, NULL, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
