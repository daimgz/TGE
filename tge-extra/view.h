#ifndef TGE_EXTRA_VIEW_H_
#define TGE_EXTRA_VIEW_H_

#include <stdbool.h>

#include "tge/tge_math.h"

#include "tge-extra/vec2i.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Playfield computation for games that adapt to the terminal: given a surface
 * size it subtracts an inset margin, validates the resulting area against a
 * minimum and tracks the first valid layout.
 *
 * The surface is whatever the caller feeds to tge_view_update(): canvas cells
 * for a plain game, logical grid cells for a grid game. The view does not
 * know about canvases or grids; it only does rect math in the caller's
 * coordinate system.
 *
 * Typical use:
 *   TGE_View view;
 *   tge_view_init(&view, 10, 6);
 *   // per frame / on resize:
 *   switch (tge_view_update(&view, w, h)) {
 *   case TGE_VIEW_FIRST_VALID: (re)start_game(); break;
 *   case TGE_VIEW_RESIZED:     clamp_entities(view.area); break;
 *   case TGE_VIEW_INVALID:     break;  (too small: paused)
 *   }
 *   // the view owns the local space the game logic reasons in:
 *   food = tge_view_random_point(&view);          // spawn inside
 *   if (!tge_view_contains(&view, next_head)) ... // wall collision
 *   draw_at(tge_view_translate(&view, cell));     // local -> surface
 *
 * The struct is small and fields are plain configuration (like TGE_Rect or
 * TGE_Color), so they are public: set view.margin directly, no setters.
 * Operations over the local space (translate, contains, random_point,
 * local_bounds) live here as functions; plain field reads are done on the
 * struct directly, there are no getters (see ADR-007). */
typedef struct {
    TGE_Rect area;    /* playfield rect, inset `margin` from the surface edge */
    int margin;       /* inset in surface cells, default 1 */
    int min_w;        /* minimum playfield width; smaller => valid == false */
    int min_h;        /* minimum playfield height; smaller => valid == false */
    bool valid;       /* false when area is smaller than the minimum */
    bool first;       /* true until the first VALID layout was received (not
                       * "first frame": a too-small start keeps first == true
                       * until the surface grows enough) */
} TGE_View;

/* Result of tge_view_update(). Lets the caller react to what the new layout
 * means instead of re-deriving it:
 *
 *   TGE_VIEW_FIRST_VALID  the first VALID layout was received: the playfield
 *                         became playable for the first time, so (re)start.
 *   TGE_VIEW_RESIZED      a valid layout was recomputed and it is not the
 *                         first one: keep the game state and clamp it into
 *                         the new bounds. Callers normally call update() only
 *                         when the surface may have changed, so this is the
 *                         "size changed" signal.
 *   TGE_VIEW_INVALID      the area is below the minimum: not playable. `first`
 *                         stays pending until the surface grows enough. */
typedef enum {
    TGE_VIEW_INVALID,
    TGE_VIEW_RESIZED,
    TGE_VIEW_FIRST_VALID
} TGE_ViewUpdate;

/* Initialize with the given minimum playfield size and margin 1. */
void tge_view_init(TGE_View *view, int min_width, int min_height);

/* Recompute area for a surface of `width` x `height` logical cells:
 *   area = rect(margin, margin, width - 2*margin, height - 2*margin)
 * clamped to zero, and valid = (area.w >= min_w && area.h >= min_h).
 * The first call that yields a valid area consumes `first` and returns
 * TGE_VIEW_FIRST_VALID; later valid layouts return TGE_VIEW_RESIZED and
 * layouts below the minimum return TGE_VIEW_INVALID. */
TGE_ViewUpdate tge_view_update(TGE_View *view, int width, int height);

/* Map a playfield-local coordinate to the outer coordinate system by adding
 * the playfield origin: result = local + (area.x, area.y). The outer system
 * is whatever surface the view was updated with (canvas cells for a plain
 * game, grid cells for a grid game). Draw callers pass local playfield
 * coordinates and the origin offsets them into the surface. */
TGE_Vec2i tge_view_translate(const TGE_View *view, TGE_Vec2i local);

/* True when `p` lies inside the playfield interior, in local coordinates
 * (x in [0, area.w), y in [0, area.h)). Wall checks (a cell about to move
 * out of bounds) and out-of-field checks (food no longer fitting after a
 * resize) use this instead of re-deriving the bounds. */
bool tge_view_contains(const TGE_View *view, TGE_Vec2i p);

/* Uniformly random local coordinate inside the playfield interior (one of
 * the area.w * area.h cells, using rand()). A NULL view or a degenerate
 * area yields (0, 0). */
TGE_Vec2i tge_view_random_point(const TGE_View *view);

/* The playfield interior expressed in local coordinates: rect(0, 0, w, h),
 * the bounds the game logic reasons about. Utility ops that take a
 * rectangle (e.g. tge_vec2i_clamp_rect) get the local space from here. */
TGE_Rect tge_view_local_bounds(const TGE_View *view);

#ifdef __cplusplus
}
#endif

#endif
