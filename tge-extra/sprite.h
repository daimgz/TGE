#ifndef TGE_EXTRA_SPRITE_H_
#define TGE_EXTRA_SPRITE_H_

#include <stdbool.h>

#include "tge/tge_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A hand-drawn sprite: box-art plus a parallel collision mask, both fixed
 * grids of cells (termrex's SpriteAsset has the same pair, ANSI_ART and
 * COLLISION_MASK). The world reads `mask` ('1' = solid) and the renderer
 * draws `art` (UTF-8 box-drawing glyphs, one cell per glyph); the ASCII
 * fallback draws the mask as solid blocks, so the shape survives terminals
 * without UTF-8. Keeping the two grids together is the point: art is what a
 * viewer sees, mask is what the game collides against, and they must stay in
 * lock-step. Validated by 08_dino, which only asks for this: it keeps pose
 * picking, animation timing and its narrow Chrome-style hitbox to itself. */
typedef struct {
    const char *const *art;   /* h rows, each exactly w cells wide */
    const char *const *mask;  /* h rows of w '1'/'0' cells */
    int w, h;
} TGE_MaskSprite;

/* True when the cell is inside the sprite and its mask is solid ('1'). The
 * bounds check makes the caller's overlap loops safe without extra care. */
bool tge_sprite_solid(const TGE_MaskSprite *sprite, int row, int col);

/* Mask collision, the termrex SpriteInstance::collide algorithm: reject the
 * bounding boxes (half-open, so touching edges do not collide), then check
 * every overlapping cell against both masks. Only where both sprites have a
 * solid cell does a hit happen, so a ducked dino with empty cells where the
 * standing one was solid slips under a pterodactyl. `aw` is a's hit width,
 * which may be narrower than its art (Chrome-style): the game decides the
 * hitbox, the mask only describes geometry. */
bool tge_sprite_collide(const TGE_MaskSprite *a, int ax, int ay, int aw,
                        const TGE_MaskSprite *b, int bx, int by);

/* Draw the sprite with its top-left at (x, y). Unicode mode paints each art
 * glyph, skipping spaces so the background shows through; otherwise the mask
 * is drawn as solid '#' blocks (the mask IS the ASCII fallback). Only reads
 * the public canvas API, no allocation, safe for the render path. */
void tge_sprite_draw(TGE_Canvas *canvas, int x, int y,
                     const TGE_MaskSprite *sprite, TGE_Color fg, TGE_Color bg);

#ifdef __cplusplus
}
#endif

#endif
