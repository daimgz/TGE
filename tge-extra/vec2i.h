#ifndef TGE_EXTRA_VEC2I_H_
#define TGE_EXTRA_VEC2I_H_

#include <stdbool.h>

#include "tge/tge_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Integer 2D vector, complementing the float TGE_Vector2 in the core.
 * Cell-grid games (Snake, Tetris, roguelikes) think in integer cells; this
 * type removes the per-game `Point` typedefs and gives the usual helpers. */
typedef struct {
    int x, y;
} TGE_Vec2i;

/* Constructors. */
TGE_Vec2i tge_vec2i(int x, int y);
TGE_Vec2i tge_vec2i_zero(void);

/* Vector arithmetic. */
TGE_Vec2i tge_vec2i_add(TGE_Vec2i a, TGE_Vec2i b);
TGE_Vec2i tge_vec2i_sub(TGE_Vec2i a, TGE_Vec2i b);
TGE_Vec2i tge_vec2i_scale(TGE_Vec2i v, int s);

/* Component-wise equality. */
bool tge_vec2i_eq(TGE_Vec2i a, TGE_Vec2i b);

/* Clamp `p` to the inclusive cell bounds of `r` (x in [r.x, r.x+r.w-1], y in
 * [r.y, r.y+r.h-1]). A degenerate rect (w or h <= 0) yields its top-left
 * corner. Keeps playfield/camera points inside their field after a resize. */
TGE_Vec2i tge_vec2i_clamp_rect(TGE_Vec2i p, TGE_Rect r);

/* Random cell inside `r` (uniform over its w*h cells, using rand()).
 * Degenerate rects yield the top-left corner. */
TGE_Vec2i tge_rect_random_point(TGE_Rect r);

/* Map a rect-local point to global coords by adding the rect origin:
 * result = p + (r.x, r.y). Draw callers pass local playfield coordinates and
 * the rect's origin offsets them into the canvas. */
TGE_Vec2i tge_rect_translate_point(TGE_Rect r, TGE_Vec2i local);

#ifdef __cplusplus
}
#endif

#endif
