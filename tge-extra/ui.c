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

void tge_draw_region(TGE_Canvas *canvas, TGE_Rect rect, const char *title,
                     TGE_Color fg)
{
    tge_draw_frame(canvas, rect.x, rect.y, rect.w, rect.h, fg,
                   TGE_COLOR_DEFAULT);
    if (title)
        tge_draw_text(canvas, rect.x + 1, rect.y, title, fg, TGE_COLOR_DEFAULT);
}
