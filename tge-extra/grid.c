#include "grid.h"

#include "tge/tge_utf8.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const TGE_Sprite block_empty = { 2, 1, "  " };
static const TGE_Sprite block_default = { 2, 1, "\xE2\x96\x88\xE2\x96\x88" };
static const TGE_Sprite block_border = { 2, 1, "\xE2\x96\x88\xE2\x96\x88" };
static const TGE_Sprite block_select = { 2, 1, "\xE2\x96\x93\xE2\x96\x93" };

static const TGE_Sprite ascii_empty = { 2, 1, "  " };
static const TGE_Sprite ascii_default = { 2, 1, "[]" };
static const TGE_Sprite ascii_border = { 2, 1, "##" };
static const TGE_Sprite ascii_select = { 2, 1, "<>" };

static const TGE_Sprite dots_empty = { 2, 1, "\xC2\xB7 " }; /* U+00B7 + space */
static const TGE_Sprite dots_default = { 2, 1, "oo" };
static const TGE_Sprite dots_border = { 2, 1, "##" };
static const TGE_Sprite dots_select = { 2, 1, "()" };

const TGE_GridTheme TGE_GRID_THEME_BLOCKS = { &block_empty, &block_default,
                                              &block_border, &block_select };
const TGE_GridTheme TGE_GRID_THEME_ASCII = { &ascii_empty, &ascii_default,
                                             &ascii_border, &ascii_select };
const TGE_GridTheme TGE_GRID_THEME_DOTS = { &dots_empty, &dots_default,
                                            &dots_border, &dots_select };

static void normalize_cell_size(TGE_Grid *g)
{
    if (g->cell_w < 1)
        g->cell_w = 1;
    if (g->cell_h < 1)
        g->cell_h = 1;
}

void tge_grid_init(TGE_Grid *g, TGE_Canvas *canvas)
{
    if (!g)
        return;
    g->canvas = canvas;
    g->ox = 0;
    g->oy = 0;
    g->cell_w = 1;
    g->cell_h = 1;
    g->theme = &TGE_GRID_THEME_BLOCKS;
}

void tge_grid_set_origin(TGE_Grid *g, int ox, int oy)
{
    if (!g)
        return;
    g->ox = ox;
    g->oy = oy;
}

void tge_grid_set_cell_size(TGE_Grid *g, int cell_w, int cell_h)
{
    if (!g)
        return;
    g->cell_w = cell_w;
    g->cell_h = cell_h;
    normalize_cell_size(g);
}

int tge_grid_width(const TGE_Grid *g)
{
    if (!g || !g->canvas)
        return 0;
    int w = tge_canvas_width(g->canvas) - g->ox;
    if (w < 0)
        w = 0;
    return w / g->cell_w;
}

int tge_grid_height(const TGE_Grid *g)
{
    if (!g || !g->canvas)
        return 0;
    int h = tge_canvas_height(g->canvas) - g->oy;
    if (h < 0)
        h = 0;
    return h / g->cell_h;
}

void tge_grid_size_for(const TGE_Grid *g, int w, int h, int *gw, int *gh)
{
    if (!g || !gw || !gh)
        return;
    int lw = w - g->ox;
    int lh = h - g->oy;
    if (lw < 0)
        lw = 0;
    if (lh < 0)
        lh = 0;
    *gw = lw / g->cell_w;
    *gh = lh / g->cell_h;
}

/* Decode `sprite` into `buf` (capacity `cap`). Returns the number of glyphs on
 * success, -1 on error; a message is printed to stderr and the sprite is
 * ignored so misconfigurations are not silent. */
static int sprite_glyphs(const TGE_Sprite *sprite, uint32_t *buf, int cap)
{
    if (!sprite || !sprite->utf8)
        return -1;
    if (sprite->width < 1 || sprite->height < 1) {
        fprintf(stderr, "tge_grid: invalid sprite dimensions %dx%d\n",
                sprite->width, sprite->height);
        return -1;
    }
    int want = sprite->width * sprite->height;
    if (want > cap) {
        fprintf(stderr, "tge_grid: sprite has more than %d glyphs\n", cap);
        return -1;
    }
    int count = 0;
    const char *p = sprite->utf8;
    int remaining = (int)strlen(sprite->utf8);
    while (remaining > 0 && count < cap) {
        uint32_t glyph;
        int consumed = tge_utf8_decode(p, remaining, &glyph);
        if (consumed < 0) {
            fprintf(stderr, "tge_grid: invalid UTF-8 in sprite\n");
            return -1;
        }
        buf[count++] = glyph;
        p += consumed;
        remaining -= consumed;
    }
    if (count != want || remaining != 0) {
        fprintf(stderr,
                "tge_grid: sprite has %d glyphs, expected %dx%d\n", count,
                sprite->width, sprite->height);
        return -1;
    }
    return count;
}

/* Map a tile role to its theme sprite. Returns NULL (with a message on
 * stderr) for an invalid tile or a theme that has no sprite for it. */
static const TGE_Sprite *resolve_tile(const TGE_Grid *g, TGE_GridTile tile)
{
    if (!g || !g->theme)
        return NULL;
    const TGE_Sprite *sprite = NULL;
    switch (tile) {
    case TGE_TILE_EMPTY:
        sprite = g->theme->empty;
        break;
    case TGE_TILE_DEFAULT:
        sprite = g->theme->default_sprite;
        break;
    case TGE_TILE_BORDER:
        sprite = g->theme->border;
        break;
    case TGE_TILE_SELECTION:
        sprite = g->theme->selection;
        break;
    default:
        fprintf(stderr, "tge_grid: invalid tile %d\n", (int)tile);
        return NULL;
    }
    if (!sprite)
        fprintf(stderr, "tge_grid: theme has no sprite for tile %d\n",
                (int)tile);
    return sprite;
}

/* Tile the sprite across the logical cell's cell_w x cell_h block. */
static void draw_cell_sprite(TGE_Grid *g, int lx, int ly,
                             const TGE_Sprite *sprite, TGE_Color fg,
                             TGE_Color bg)
{
    if (!g || !g->canvas || !sprite)
        return;
    uint32_t glyphs[TGE_GRID_MAX_CELL_GLYPHS];
    if (sprite_glyphs(sprite, glyphs, TGE_GRID_MAX_CELL_GLYPHS) < 0)
        return;
    int pw = sprite->width;
    int ph = sprite->height;
    TGE_Canvas *c = g->canvas;
    int cw = tge_canvas_width(c);
    int chh = tge_canvas_height(c);
    int px = g->ox + lx * g->cell_w;
    int py = g->oy + ly * g->cell_h;
    for (int y = 0; y < g->cell_h; y++) {
        int phy = py + y;
        if (phy < 0 || phy >= chh)
            continue;
        for (int x = 0; x < g->cell_w; x++) {
            int phx = px + x;
            if (phx < 0 || phx >= cw)
                continue;
            tge_set_cell(c, phx, phy, glyphs[(y % ph) * pw + (x % pw)], fg,
                         bg);
        }
    }
}

void tge_grid_put_tile(TGE_Grid *g, int lx, int ly, TGE_GridTile tile,
                       TGE_Color fg, TGE_Color bg)
{
    draw_cell_sprite(g, lx, ly, resolve_tile(g, tile), fg, bg);
}

void tge_grid_fill(TGE_Grid *g, int lx, int ly, int lw, int lh,
                   TGE_GridTile tile, TGE_Color fg, TGE_Color bg)
{
    if (!g || lw <= 0 || lh <= 0)
        return;
    for (int row = ly; row < ly + lh; row++) {
        for (int col = lx; col < lx + lw; col++)
            tge_grid_put_tile(g, col, row, tile, fg, bg);
    }
}

void tge_grid_set_cell(TGE_Grid *g, int lx, int ly, TGE_Color fg, TGE_Color bg)
{
    tge_grid_put_tile(g, lx, ly, TGE_TILE_DEFAULT, fg, bg);
}

void tge_grid_erase(TGE_Grid *g, int lx, int ly, TGE_Color fg, TGE_Color bg)
{
    tge_grid_put_tile(g, lx, ly, TGE_TILE_EMPTY, fg, bg);
}

void tge_grid_draw_border(TGE_Grid *g, int lx, int ly, int lw, int lh,
                          TGE_Color fg, TGE_Color bg)
{
    if (!g || lw <= 0 || lh <= 0)
        return;
    for (int i = 0; i < lw; i++) {
        tge_grid_put_tile(g, lx + i, ly, TGE_TILE_BORDER, fg, bg);
        tge_grid_put_tile(g, lx + i, ly + lh - 1, TGE_TILE_BORDER, fg, bg);
    }
    for (int i = 0; i < lh; i++) {
        tge_grid_put_tile(g, lx, ly + i, TGE_TILE_BORDER, fg, bg);
        tge_grid_put_tile(g, lx + lw - 1, ly + i, TGE_TILE_BORDER, fg, bg);
    }
}

void tge_grid_clear(TGE_Grid *g, TGE_Color fg, TGE_Color bg)
{
    if (!g || !g->canvas)
        return;
    tge_grid_fill(g, 0, 0, tge_grid_width(g), tge_grid_height(g),
                  TGE_TILE_EMPTY, fg, bg);
}

void tge_grid_draw_frame(TGE_Grid *g, int lx, int ly, int lw, int lh,
                         TGE_Color fg, TGE_Color bg)
{
    if (!g || !g->canvas || lw <= 0 || lh <= 0)
        return;
    TGE_Canvas *c = g->canvas;
    int x = g->ox + lx * g->cell_w;
    int y = g->oy + ly * g->cell_h;
    int x2 = x + lw * g->cell_w - 1;
    int y2 = y + lh * g->cell_h - 1;
    for (int i = x + 1; i < x2; i++) {
        tge_set_cell(c, i, y, 0x2500, fg, bg);
        tge_set_cell(c, i, y2, 0x2500, fg, bg);
    }
    for (int i = y + 1; i < y2; i++) {
        tge_set_cell(c, x, i, 0x2502, fg, bg);
        tge_set_cell(c, x2, i, 0x2502, fg, bg);
    }
    tge_set_cell(c, x, y, 0x250C, fg, bg);
    tge_set_cell(c, x2, y, 0x2510, fg, bg);
    tge_set_cell(c, x, y2, 0x2514, fg, bg);
    tge_set_cell(c, x2, y2, 0x2518, fg, bg);
}

void tge_grid_draw_line(TGE_Grid *g, int x1, int y1, int x2, int y2,
                        TGE_Color fg, TGE_Color bg)
{
    if (!g)
        return;
    int dx = x2 > x1 ? x2 - x1 : x1 - x2;
    int dy = y2 > y1 ? y2 - y1 : y1 - y2;
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        tge_grid_set_cell(g, x1, y1, fg, bg);
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

void tge_grid_draw_circle(TGE_Grid *g, int cx, int cy, int r, TGE_Color fg,
                          TGE_Color bg)
{
    if (!g || r < 0)
        return;
    for (int ly = cy - r; ly <= cy + r; ly++) {
        for (int lx = cx - r; lx <= cx + r; lx++) {
            float d = sqrtf((float)(lx - cx) * (float)(lx - cx) +
                            (float)(ly - cy) * (float)(ly - cy));
            if (fabsf(d - (float)r) < 0.5f)
                tge_grid_set_cell(g, lx, ly, fg, bg);
        }
    }
}

void tge_grid_draw_text(TGE_Grid *g, int lx, int ly, const char *text,
                        TGE_Color fg, TGE_Color bg)
{
    if (!g || !g->canvas || !text)
        return;
    tge_draw_text(g->canvas, g->ox + lx * g->cell_w, g->oy + ly * g->cell_h,
                  text, fg, bg);
}

static int count_glyphs(const char *s, int len)
{
    int count = 0;
    while (len > 0) {
        uint32_t glyph;
        int consumed = tge_utf8_decode(s, len, &glyph);
        if (consumed < 0)
            return -1;
        s += consumed;
        len -= consumed;
        count++;
    }
    return count;
}

void tge_grid_put(TGE_Grid *g, int lx, int ly, const TGE_Sprite *sprite,
                  TGE_Color fg, TGE_Color bg)
{
    if (!g || !g->canvas || !sprite || !sprite->utf8)
        return;
    if (sprite->width < 1 || sprite->height < 1) {
        fprintf(stderr, "tge_grid_put: invalid dimensions %dx%d\n",
                sprite->width, sprite->height);
        return;
    }

    int n = sprite->width * sprite->height;
    int len = (int)strlen(sprite->utf8);
    int count = count_glyphs(sprite->utf8, len);
    if (count < 0) {
        fprintf(stderr, "tge_grid_put: invalid UTF-8 in sprite\n");
        return;
    }
    if (count != n) {
        fprintf(stderr,
                "tge_grid_put: sprite has %d glyphs, expected %dx%d\n", count,
                sprite->width, sprite->height);
        return;
    }

    const char *p = sprite->utf8;
    int remaining = len;
    for (int i = 0; i < n; i++) {
        uint32_t glyph;
        int consumed = tge_utf8_decode(p, remaining, &glyph);
        p += consumed;
        remaining -= consumed;

        int x = g->ox + lx * g->cell_w + i % sprite->width;
        int y = g->oy + ly * g->cell_h + i / sprite->width;
        tge_set_cell(g->canvas, x, y, glyph, fg, bg);
    }
}
