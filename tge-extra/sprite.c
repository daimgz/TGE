#include "sprite.h"

#include "tge/tge_unicode.h"
#include "tge/tge_utf8.h"

static bool cell_solid(const TGE_MaskSprite *s, int r, int c)
{
    return r >= 0 && r < s->h && c >= 0 && c < s->w && s->mask[r][c] == '1';
}

bool tge_sprite_solid(const TGE_MaskSprite *sprite, int row, int col)
{
    return cell_solid(sprite, row, col);
}

bool tge_sprite_collide(const TGE_MaskSprite *a, int ax, int ay, int aw,
                        const TGE_MaskSprite *b, int bx, int by)
{
    if (ax + aw <= bx || bx + b->w <= ax ||
        ay + a->h <= by || by + b->h <= ay)
        return false;
    int r0 = ay > by ? ay : by;
    int r1 = (ay + a->h < by + b->h ? ay + a->h : by + b->h) - 1;
    int c0 = ax > bx ? ax : bx;
    int c1 = (ax + aw < bx + b->w ? ax + aw : bx + b->w) - 1;
    for (int r = r0; r <= r1; r++)
        for (int c = c0; c <= c1; c++)
            if (cell_solid(a, r - ay, c - ax) && cell_solid(b, r - by, c - bx))
                return true;
    return false;
}

/* Draw one row of UTF-8 box glyphs, one cell per glyph, skipping spaces (the
 * background already shows through). The advance is one cell per codepoint,
 * not per byte: the art holds multi-byte box-drawing glyphs. */
static void draw_utf8_row(TGE_Canvas *canvas, int x, int y, const char *s,
                          TGE_Color fg, TGE_Color bg)
{
    int cx = x;
    const char *p = s;
    while (*p) {
        uint32_t cp;
        int n = tge_utf8_decode(p, 8, &cp);
        if (n <= 0)
            break;
        if (cp != ' ' && cp != 0)
            tge_set_cell(canvas, cx, y, cp, fg, bg);
        cx++;
        p += n;
    }
}

void tge_sprite_draw(TGE_Canvas *canvas, int x, int y,
                     const TGE_MaskSprite *sprite, TGE_Color fg, TGE_Color bg)
{
    if (tge_unicode_supported()) {
        for (int r = 0; r < sprite->h; r++)
            draw_utf8_row(canvas, x, y + r, sprite->art[r], fg, bg);
    } else {
        for (int r = 0; r < sprite->h; r++)
            for (int c = 0; c < sprite->w; c++)
                if (sprite->mask[r][c] == '1')
                    tge_set_cell(canvas, x + c, y + r, '#', fg, bg);
    }
}
