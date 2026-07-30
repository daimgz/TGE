#include "tge/tge_canvas.h"
#include "tge_internal.h"
#include <stdlib.h>
#include <string.h>

int tge_canvas_width(const TGE_Canvas *canvas)
{
    return canvas ? canvas->width : 0;
}

int tge_canvas_height(const TGE_Canvas *canvas)
{
    return canvas ? canvas->height : 0;
}

void tge_clear(TGE_Canvas *canvas, uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    if (!canvas || !canvas->cells) return;
    int n = canvas->width * canvas->height;
    for (int i = 0; i < n; i++) {
        canvas->cells[i].ch = ch;
        canvas->cells[i].fg = fg;
        canvas->cells[i].bg = bg;
        canvas->cells[i].attr = 0;
    }
}

void tge_set_cell(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                  TGE_Color fg, TGE_Color bg)
{
    if (!canvas || !canvas->cells) return;
    if (x < 0 || x >= canvas->width || y < 0 || y >= canvas->height) return;
    int idx = y * canvas->width + x;
    canvas->cells[idx].ch = ch;
    canvas->cells[idx].fg = fg;
    canvas->cells[idx].bg = bg;
    canvas->cells[idx].attr = 0;
}

void tge_draw_text(TGE_Canvas *canvas, int x, int y, const char *text,
                   TGE_Color fg, TGE_Color bg)
{
    if (!canvas || !text) return;
    if (y < 0 || y >= canvas->height) return;
    for (int i = 0; text[i] != '\0' && (x + i) < canvas->width; i++) {
        if (x + i < 0) continue;
        int idx = y * canvas->width + x + i;
        canvas->cells[idx].ch = (unsigned char)text[i];
        canvas->cells[idx].fg = fg;
        canvas->cells[idx].bg = bg;
        canvas->cells[idx].attr = 0;
    }
}

void tge_draw_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   TGE_Color fg, TGE_Color bg)
{
    (void)canvas; (void)x; (void)y; (void)w; (void)h; (void)fg; (void)bg;
}

void tge_draw_frame(TGE_Canvas *canvas, int x, int y, int w, int h,
                    TGE_Color fg, TGE_Color bg)
{
    (void)canvas; (void)x; (void)y; (void)w; (void)h; (void)fg; (void)bg;
}

void tge_fill_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    (void)canvas; (void)x; (void)y; (void)w; (void)h;
    (void)ch; (void)fg; (void)bg;
}

void tge_draw_line(TGE_Canvas *canvas, int x1, int y1, int x2, int y2,
                   uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    (void)canvas; (void)x1; (void)y1; (void)x2; (void)y2;
    (void)ch; (void)fg; (void)bg;
}

void tge_draw_circle(TGE_Canvas *canvas, int cx, int cy, int r,
                     uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    (void)canvas; (void)cx; (void)cy; (void)r;
    (void)ch; (void)fg; (void)bg;
}
