#ifndef TGE_EXTRA_PLAYFIELD_H_
#define TGE_EXTRA_PLAYFIELD_H_

#include <stdbool.h>

#include "tge-extra/grid_view.h"

#include "tge-extra/view.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The playfield composition that Snake and Breakout each built by hand:
 *
 *   TGE_View view;        logical playfield, adapts to the terminal
 *   TGE_GridView grid;    the drawing surface (theme, cell size, origin)
 *   TGE_GridLayout layout; remembers which logical grid size was applied
 *
 * bundled into one struct. This module exists ONLY because that exact
 * composition (init/attach/sync/border + local-space draws) appeared in two
 * games; it carries no camera, no entities, no game rules. The game still
 * decides how to react to a resize (TGE_ViewUpdate) and owns all its logic.
 *
 * Usage mirrors the games it replaces:
 *   TGE_Playfield pf;
 *   tge_playfield_init(&pf, &THEME, TGE_GRID_SCALE_2X1, 10, 6);
 *   // per draw / on resize:
 *   if (tge_playfield_sync(&pf, w, h, world_resize_cb, &world))
 *       ;   // world_resize_cb updated pf.view and clamped the world
 *   tge_playfield_attach(&pf, canvas);   // per draw, before drawing
 *   tge_playfield_draw_border(&pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
 *   tge_grid_view_put_local(&pf.grid_view, &pf.view, pos, sprite, fg, bg);
 */
typedef struct {
    TGE_View view;          /* logical playfield (adapts to the terminal) */
    TGE_GridView grid_view; /* the drawing surface */
    TGE_GridLayout layout;  /* logical grid size already applied to the view */
} TGE_Playfield;

/* Configure once: the view's minimum playfield size and the grid's theme and
 * cell scale (origin defaults to (0, 0)). The canvas is attached separately
 * per draw. The layout cache starts empty, so the first sync reports a change. */
void tge_playfield_init(TGE_Playfield *pf, const TGE_GridTheme *theme,
                        TGE_GridScale scale, int min_w, int min_h);

/* Point the grid at the canvas it draws into. Call it at the start of every
 * draw: the app swaps its double buffers each frame. */
void tge_playfield_attach(TGE_Playfield *pf, TGE_Canvas *canvas);

/* Recompute the logical grid size for a surface of surface_w x surface_h and,
 * when it changed, update the layout cache and call `on_resize` with the new
 * grid size and `userdata` (returns true on a change). The callback is
 * responsible for updating pf->view (tge_view_update) and adapting the game
 * world to the new logical size, exactly like the games this module replaces.
 * A resize never mutates game state by itself: the callback is the only
 * effect. */
bool tge_playfield_sync(TGE_Playfield *pf, int surface_w, int surface_h,
                        tge_grid_layout_resize_fn on_resize, void *userdata);

/* Border around the whole logical grid view (the theme `border` sprite). */
void tge_playfield_draw_border(TGE_Playfield *pf, TGE_Color fg, TGE_Color bg);

#ifdef __cplusplus
}
#endif

#endif
