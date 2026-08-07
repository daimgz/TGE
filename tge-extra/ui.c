#include "tge-extra/ui.h"

void tge_draw_modal(TGE_Canvas *canvas, const char *title,
                    const char *subtitle, TGE_Color title_fg)
{
    int width = tge_canvas_width(canvas);
    int height = tge_canvas_height(canvas);
    tge_fill_rect(canvas, 1, height / 2 - 1, width - 2, 3, ' ',
                  TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, height / 2 - 1, title, title_fg,
                           TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, height / 2 + 1, subtitle, TGE_COLOR_WHITE,
                           TGE_COLOR_BLACK);
}
