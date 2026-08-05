#ifndef TGE_EXTRA_VEC2I_H_
#define TGE_EXTRA_VEC2I_H_

#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif

#endif
