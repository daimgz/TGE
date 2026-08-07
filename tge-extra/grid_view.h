#ifndef TGE_EXTRA_GRID_VIEW_H_
#define TGE_EXTRA_GRID_VIEW_H_

#include "tge-extra/grid.h"

#include "tge-extra/view.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Predefined physical cell sizes for a grid view. */
typedef enum {
    TGE_GRID_SCALE_1X1, /* one character per logical cell */
    TGE_GRID_SCALE_2X1, /* two characters per logical cell (square look) */
} TGE_GridScale;

/* A grid view: an initialized TGE_Grid with the boilerplate filled in. The
 * common case of a grid that covers a whole logical area from the origin
 * (snake, tetris, roguelikes) is four calls — init, theme, cell size, origin —
 * and TGE_GridView is that in one. It is a deliberate thin wrapper: there is
 * no state beyond the TGE_Grid itself, no resize handling, no dirty tracking,
 * no magic. Rendering goes straight through to the grid primitives, which
 * clamp, so rendering outside the view is safe. Everything below the boundary
 * (canvas, sizing, origin, theme, clipping) still belongs to the grid; the
 * view is only the configuration preset.
 *
 * The struct is meant to be embedded (like the TGE_App layers) and is public
 * and malleable: after tge_grid_view_init(), set view.grid.theme or call
 * tge_grid_set_origin(&view.grid, ...) freely.
 *
 * The view is configured in two phases, mirroring TGE_Grid:
 *
 *   tge_grid_view_init(&view, theme, scale);  // once: theme, cell size
 *   tge_grid_view_attach(&view, canvas);      // per frame: current canvas
 *
 * init() is the persistent configuration (theme, cell size, origin (0, 0)),
 * attach() points the view at the canvas being drawn. The app swaps its
 * double buffers every frame, so the canvas pointer changes on every draw
 * while the configuration persists; a game typically calls attach() at the
 * start of each draw and never touches init() again.
 *
 * Note: it is zero-initialized when embedded as a struct member, which gives
 * the deliberately invalid NULL canvas; there is no valid zero state, so it
 * MUST be initialized with tge_grid_view_init() before drawing.
 *
 * Typical use:
 *   TGE_GridView grid_view;
 *   tge_grid_view_init(&grid_view, TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1);
 *   // per frame:
 *   tge_grid_view_attach(&grid_view, canvas);
 *   tge_grid_view_draw_border(&grid_view, TGE_WHITE, TGE_BLACK);
 *   tge_grid_view_set_cell(&grid_view, snake[i].x, snake[i].y,
 *                          TGE_GREEN, TGE_BLACK); */
typedef struct {
    TGE_Grid grid; /* the initialized grid: theme, cell size, origin */
} TGE_GridView;

/* Configure the view once: theme set, cell size per `scale` (1X1 -> 1x1,
 * 2X1 -> 2x1) and origin (0, 0). The canvas is attached separately with
 * tge_grid_view_attach() and persists across re-attaches. */
void tge_grid_view_init(TGE_GridView *view, const TGE_GridTheme *theme,
                        TGE_GridScale scale);

/* Point the view at the canvas it draws into. Call it at the start of every
 * draw: the app swaps its double buffers each frame, so the canvas pointer
 * changes while the configuration from init() persists. */
void tge_grid_view_attach(TGE_GridView *view, TGE_Canvas *canvas);

/* Logical size of the view: delegates to tge_grid_width/height, so it adapts
 * when the canvas is resized and shrinks near the bottom/right margins. */
int tge_grid_view_width(const TGE_GridView *view);
int tge_grid_view_height(const TGE_GridView *view);

/* Logical size for an arbitrary physical surface of w x h (no canvas needed),
 * e.g. the size a resize event implies before the canvas is updated.
 * Delegates to tge_grid_size_for. */
void tge_grid_view_size_for(const TGE_GridView *view, int w, int h, int *gw,
                            int *gh);

/* Auxiliary layout state of a grid view: which logical grid size has already
 * been applied to the world, so games recompute the layout only when the
 * surface changed. The view computes how a canvas maps to grid cells; the
 * layout remembers what was applied to the world, keeping the "the world
 * never remembers how it was presented" rule:
 *
 *   TGE_GridLayout layout;
 *   tge_grid_layout_init(&layout, &grid_view);   // once, after init
 *   // per draw / on resize:
 *   tge_grid_layout_sync(&layout, w, h, world_resize_cb, &world);
 *
 * It is a grid-view concept, not a game one: the sync only compares sizes
 * and fires a resize callback; it never touches the game, the canvas or the
 * drawing state. */
typedef struct {
    TGE_GridView *view; /* the grid the sizes are computed from */
    int cached_width;   /* last logical grid width applied to the world */
    int cached_height;  /* last logical grid height applied to the world */
} TGE_GridLayout;

/* Resize notification: called when the logical grid size changed, with the
 * new grid size and the userdata passed to tge_grid_layout_sync. */
typedef void (*tge_grid_layout_resize_fn)(void *userdata, int grid_w,
                                          int grid_h);

/* Point the layout at its grid view and reset the cache (0, 0), so the first
 * sync always reports a change. Call once after tge_grid_view_init(). */
void tge_grid_layout_init(TGE_GridLayout *layout, TGE_GridView *view);

/* Recompute the logical grid size for a surface of `surface_w` x
 * `surface_h` (via tge_grid_view_size_for). When it differs from the cached
 * size, updates the cache and calls `on_resize` (non-NULL) with the new grid
 * size and `userdata`; returns true when a resize happened. A resize never
 * mutates game state: the callback is the only effect, and the caller owns
 * whatever it does with the new size. */
bool tge_grid_layout_sync(TGE_GridLayout *layout, int surface_w,
                          int surface_h, tge_grid_layout_resize_fn on_resize,
                          void *userdata);

/* Border around the whole logical view (the theme `border` sprite). */
void tge_grid_view_draw_border(TGE_GridView *view, TGE_Color fg, TGE_Color bg);

/* Write one logical cell with the theme `default_sprite`. */
void tge_grid_view_set_cell(TGE_GridView *view, int lx, int ly, TGE_Color fg,
                            TGE_Color bg);

/* Draw a general sprite anchored at a logical position (natural terminal
 * size, see tge_grid_put). */
void tge_grid_view_put(TGE_GridView *view, int lx, int ly,
                       const TGE_Sprite *sprite, TGE_Color fg, TGE_Color bg);

/* Same as tge_grid_view_put plus cell attributes (TGE_CELL_ATTR_*). */
void tge_grid_view_put_attr(TGE_GridView *view, int lx, int ly,
                            const TGE_Sprite *sprite, TGE_Color fg,
                            TGE_Color bg, uint8_t attr);

/* The _local variants take a playfield coordinate local to a TGE_View
 * (0..w-1, 0..h-1) and translate it through the layout before drawing, so
 * games write in their own logical space instead of mapping coordinates by
 * hand:
 *
 *   tge_grid_view_set_cell_local(&gv, &world->view, body[i], GREEN, BLACK);
 *
 * The three share the same shape: (grid view, layout, local point, style...),
 * differing only in what they draw. */

/* Write one logical cell with the theme `default_sprite` at the local
 * coordinate `local` of `layout`. */
void tge_grid_view_set_cell_local(TGE_GridView *view, const TGE_View *layout,
                                  TGE_Vec2i local, TGE_Color fg, TGE_Color bg);

/* Draw a general sprite at the local coordinate `local` of `layout`. */
void tge_grid_view_put_local(TGE_GridView *view, const TGE_View *layout,
                             TGE_Vec2i local, const TGE_Sprite *sprite,
                             TGE_Color fg, TGE_Color bg);

/* Same as tge_grid_view_put_local plus cell attributes. */
void tge_grid_view_put_attr_local(TGE_GridView *view, const TGE_View *layout,
                                  TGE_Vec2i local, const TGE_Sprite *sprite,
                                  TGE_Color fg, TGE_Color bg, uint8_t attr);

#ifdef __cplusplus
}
#endif

#endif
