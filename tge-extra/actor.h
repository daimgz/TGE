#ifndef TGE_EXTRA_ACTOR_H_
#define TGE_EXTRA_ACTOR_H_

#include "tge-extra/grid_view.h"

#include "tge-extra/vec2i.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A positioned sprite on a grid: the minimal shared shape of movable
 * characters in grid games (Pac-Man and its ghosts, a player, a unit). It is
 * deliberately NOT an entity or ECS: it is plain data (position +
 * representation) plus one draw helper. Games embed a TGE_Actor in their own
 * state structs and layer their game-specific fields on top (direction, mode,
 * targets, animation frames), and animate by swapping `sprite`.
 *
 * The position is a playfield-local coordinate; tge_actor_draw() translates
 * it through a TGE_View (the same local space tge_grid_view_put_local uses).
 */
typedef struct {
    TGE_Vec2i position; /* local playfield cell */
    const TGE_Sprite *sprite;
    TGE_Color fg;
    TGE_Color bg;
} TGE_Actor;

/* Draw the actor's sprite at its position through the view's layout. No-op on
 * NULL view/layout/actor or a NULL sprite. Equivalent to
 * tge_grid_view_put_local(view, layout, actor->position, ...). */
void tge_actor_draw(TGE_GridView *view, const TGE_View *layout,
                    const TGE_Actor *actor);

#ifdef __cplusplus
}
#endif

#endif
