#include "tge/tge_canvas.h"
#include "tge/tge_unicode.h"
#include "tge/tge_utf8.h"
#include "tge_internal.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

TGE_Canvas *tge_canvas_create(int width, int height)
{
    if (width <= 0 || height <= 0)
        return NULL;
    TGE_Canvas *c = (TGE_Canvas *)calloc(1, sizeof(TGE_Canvas));
    if (!c)
        return NULL;
    c->cells = (TGE_Cell *)calloc((size_t)width * (size_t)height,
                                  sizeof(TGE_Cell));
    if (!c->cells) {
        free(c);
        return NULL;
    }
    c->width = width;
    c->height = height;
    return c;
}

void tge_canvas_destroy(TGE_Canvas *c)
{
    if (!c)
        return;
    free(c->cells);
    free(c);
}

void tge_canvas_resize(TGE_Canvas *c, int width, int height)
{
    if (!c || width <= 0 || height <= 0)
        return;
    if (width == c->width && height == c->height)
        return;
    TGE_Cell *ncells = (TGE_Cell *)calloc((size_t)width * (size_t)height,
                                          sizeof(TGE_Cell));
    if (!ncells)
        return;
    free(c->cells);
    c->cells = ncells;
    c->width = width;
    c->height = height;
}

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
    if (!canvas || !canvas->cells)
        return;
    int n = canvas->width * canvas->height;
    for (int i = 0; i < n; i++) {
        canvas->cells[i].ch = ch;
        canvas->cells[i].fg = fg;
        canvas->cells[i].bg = bg;
        canvas->cells[i].attr = 0;
    }
}

static void set_cell_impl(TGE_Canvas *c, int x, int y, uint32_t ch,
                          TGE_Color fg, TGE_Color bg, uint8_t attr)
{
    if (x < 0 || x >= c->width || y < 0 || y >= c->height)
        return;
    TGE_Cell *cell = &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
    cell->ch = ch;
    cell->fg = fg;
    cell->bg = bg;
    cell->attr = attr;
}

void tge_set_cell(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                  TGE_Color fg, TGE_Color bg)
{
    tge_set_cell_attr(canvas, x, y, ch, fg, bg, 0);
}

void tge_set_cell_attr(TGE_Canvas *canvas, int x, int y, uint32_t ch,
                       TGE_Color fg, TGE_Color bg, uint8_t attr)
{
    if (!canvas || !canvas->cells)
        return;
    if (tge_utf8_char_width(ch) == 2) {
        set_cell_impl(canvas, x, y, ch, fg, bg, attr);
        set_cell_impl(canvas, x + 1, y, 0, fg, bg, attr);
    } else {
        set_cell_impl(canvas, x, y, ch, fg, bg, attr);
    }
}

void tge_draw_text(TGE_Canvas *canvas, int x, int y, const char *text,
                   TGE_Color fg, TGE_Color bg)
{
    if (!canvas || !canvas->cells || !text)
        return;
    if (y < 0 || y >= canvas->height)
        return;

    int len = (int)strlen(text);
    int pos = 0;
    int cur_x = x;
    while (pos < len) {
        uint32_t cp;
        int n = tge_utf8_decode(text + pos, len - pos, &cp);
        if (n <= 0) {
            pos++;
            continue;
        }
        pos += n;
        int w = tge_utf8_char_width(cp);
        if (w == 0)
            continue;
        if (cur_x < 0) {
            cur_x += w;
            continue;
        }
        if (cur_x >= canvas->width)
            break;
        if (w == 2 && cur_x + 2 > canvas->width)
            break;
        tge_set_cell(canvas, cur_x, y, cp, fg, bg);
        cur_x += w;
    }
}

void tge_vprintf(TGE_Canvas *canvas, int x, int y, TGE_Color fg, TGE_Color bg,
                 const char *fmt, va_list ap)
{
    if (!canvas || !fmt)
        return;
    char buf[TGE_PRINTF_BUF] = { 0 };
    vsnprintf(buf, sizeof(buf), fmt, ap);
    tge_draw_text(canvas, x, y, buf, fg, bg);
}

void tge_printf(TGE_Canvas *canvas, int x, int y, TGE_Color fg, TGE_Color bg,
                const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    tge_vprintf(canvas, x, y, fg, bg, fmt, ap);
    va_end(ap);
}

static int text_width(const char *text)
{
    int len = (int)strlen(text);
    int pos = 0;
    int w = 0;
    while (pos < len) {
        uint32_t cp;
        int n = tge_utf8_decode(text + pos, len - pos, &cp);
        if (n <= 0) {
            pos++;
            continue;
        }
        pos += n;
        w += tge_utf8_char_width(cp);
    }
    return w;
}

void tge_draw_centered_text(TGE_Canvas *canvas, int y, const char *text,
                            TGE_Color fg, TGE_Color bg)
{
    if (!canvas || !canvas->cells || !text)
        return;
    int x = (canvas->width - text_width(text)) / 2;
    tge_draw_text(canvas, x, y, text, fg, bg);
}

void tge_draw_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   TGE_Color fg, TGE_Color bg)
{
    if (!canvas || w <= 0 || h <= 0)
        return;
    bool unicode = tge_unicode_supported();
    uint32_t block = unicode ? 0x2588 : '#';
    int x2 = x + w - 1;
    int y2 = y + h - 1;
    for (int i = x; i <= x2; i++) {
        tge_set_cell(canvas, i, y, block, fg, bg);
        tge_set_cell(canvas, i, y2, block, fg, bg);
    }
    for (int i = y; i <= y2; i++) {
        tge_set_cell(canvas, x, i, block, fg, bg);
        tge_set_cell(canvas, x2, i, block, fg, bg);
    }
}

void tge_draw_frame(TGE_Canvas *canvas, int x, int y, int w, int h,
                    TGE_Color fg, TGE_Color bg)
{
    if (!canvas || w <= 0 || h <= 0)
        return;
    bool unicode = tge_unicode_supported();

    uint32_t hline = unicode ? 0x2500 : '-';
    uint32_t vline = unicode ? 0x2502 : '|';
    int x2 = x + w - 1;
    int y2 = y + h - 1;
    for (int i = x + 1; i < x2; i++) {
        tge_set_cell(canvas, i, y, hline, fg, bg);
        tge_set_cell(canvas, i, y2, hline, fg, bg);
    }
    for (int i = y + 1; i < y2; i++) {
        tge_set_cell(canvas, x, i, vline, fg, bg);
        tge_set_cell(canvas, x2, i, vline, fg, bg);
    }
    uint32_t tl = unicode ? 0x250C : '+';
    uint32_t tr = unicode ? 0x2510 : '+';
    uint32_t bl = unicode ? 0x2514 : '+';
    uint32_t br = unicode ? 0x2518 : '+';
    tge_set_cell(canvas, x, y, tl, fg, bg);
    tge_set_cell(canvas, x2, y, tr, fg, bg);
    tge_set_cell(canvas, x, y2, bl, fg, bg);
    tge_set_cell(canvas, x2, y2, br, fg, bg);
}

void tge_fill_rect(TGE_Canvas *canvas, int x, int y, int w, int h,
                   uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    if (!canvas || w <= 0 || h <= 0)
        return;
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++)
            tge_set_cell(canvas, col, row, ch, fg, bg);
    }
}

void tge_draw_line(TGE_Canvas *canvas, int x1, int y1, int x2, int y2,
                   uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    if (!canvas)
        return;
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        tge_set_cell(canvas, x1, y1, ch, fg, bg);
        if (x1 == x2 && y1 == y2)
            break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void tge_draw_circle(TGE_Canvas *canvas, int cx, int cy, int r,
                     uint32_t ch, TGE_Color fg, TGE_Color bg)
{
    if (!canvas || r < 0)
        return;
    int x = r;
    int y = 0;
    int err = 1 - r;
    while (x >= y) {
        tge_set_cell(canvas, cx + x, cy + y, ch, fg, bg);
        tge_set_cell(canvas, cx + y, cy + x, ch, fg, bg);
        tge_set_cell(canvas, cx - y, cy + x, ch, fg, bg);
        tge_set_cell(canvas, cx - x, cy + y, ch, fg, bg);
        tge_set_cell(canvas, cx - x, cy - y, ch, fg, bg);
        tge_set_cell(canvas, cx - y, cy - x, ch, fg, bg);
        tge_set_cell(canvas, cx + y, cy - x, ch, fg, bg);
        tge_set_cell(canvas, cx + x, cy - y, ch, fg, bg);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
    tge_set_cell(canvas, cx, cy, ch, fg, bg);
}

TGE_Color tge_color_indexed(uint8_t index)
{
    TGE_Color c = {0};
    c.mode = TGE_COLOR_MODE_INDEXED;
    c.data.index = index;
    return c;
}

TGE_Color tge_color_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    TGE_Color c = {0};
    c.mode = TGE_COLOR_MODE_RGB;
    c.data.rgb.r = r;
    c.data.rgb.g = g;
    c.data.rgb.b = b;
    return c;
}

void tge_blit(TGE_Canvas *dst, int dx, int dy, const TGE_Canvas *src)
{
    if (!dst || !dst->cells || !src || !src->cells)
        return;
    for (int y = 0; y < src->height; y++) {
        int oy = dy + y;
        if (oy < 0 || oy >= dst->height)
            continue;
        for (int x = 0; x < src->width; x++) {
            int ox = dx + x;
            if (ox < 0 || ox >= dst->width)
                continue;
            dst->cells[(size_t)oy * (size_t)dst->width + (size_t)ox] =
                src->cells[(size_t)y * (size_t)src->width + (size_t)x];
        }
    }
}
