#ifndef TGE_EXTRA_DIRECTION_H_
#define TGE_EXTRA_DIRECTION_H_

#include "tge-extra/vec2i.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cardinal directions on a grid. TGE_DIR_NONE is the neutral value, e.g.
 * what tge_input_direction() returns for non-directional input. */
typedef enum {
    TGE_DIR_NONE = 0,
    TGE_DIR_UP,
    TGE_DIR_DOWN,
    TGE_DIR_LEFT,
    TGE_DIR_RIGHT
} TGE_Direction;

/* The opposite direction; TGE_DIR_NONE for TGE_DIR_NONE. */
TGE_Direction tge_direction_opposite(TGE_Direction d);

/* Unit step along the direction axis; 0 for TGE_DIR_NONE. */
int tge_direction_dx(TGE_Direction d);
int tge_direction_dy(TGE_Direction d);

/* Unit step as a TGE_Vec2i, for tge_vec2i_add() style movement. */
TGE_Vec2i tge_direction_vec(TGE_Direction d);

#ifdef __cplusplus
}
#endif

#endif
