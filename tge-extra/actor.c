#include "actor.h"

void tge_actor_draw(TGE_GridView *view, const TGE_View *layout,
                    const TGE_Actor *actor)
{
    if (!view || !layout || !actor || !actor->sprite)
        return;
    tge_grid_view_put_local(view, layout, actor->position, actor->sprite,
                            actor->fg, actor->bg);
}
