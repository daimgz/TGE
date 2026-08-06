#ifndef TGE_EXTRA_GRID_VIEW_H_
#define TGE_EXTRA_GRID_VIEW_H_

#include "tge-extra/grid.h"

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
 * Note: it is zero-initialized when embedded as a struct member, which gives
 * the deliberately invalid NULL canvas; there is no valid zero state, so it
 * MUST be initialized with tge_grid_view_init() before drawing.
 *
 * Typical use:
 *   TGE_GridView grid_view;
 *   tge_grid_view_init(&grid_view, canvas, TGE_GRID_THEME_BLOCKS,
 *                      TGE_GRID_SCALE_2X1);
 *   // per frame:
 *   tge_grid_view_draw_border(&grid_view, TGE_WHITE, TGE_BLACK);
 *   tge_grid_view_set_cell(&grid_view, snake[i].x, snake[i].y,
 *                          TGE_GREEN, TGE_BLACK); */
typedef struct {
    TGE_Grid grid; /* the initialized grid: theme, cell size, origin */
} TGE_GridView;

/* Initialize the view over `canvas`: theme set, cell size per `scale`
 * (1X1 -> 1x1, 2X1 -> 2x1) and origin (0, 0). */
void tge_grid_view_init(TGE_GridView *view, TGE_Canvas *canvas,
                        const TGE_GridTheme *theme, TGE_GridScale scale);

/* Logical size of the view: delegates to tge_grid_width/height, so it adapts
 * when the canvas is resized and shrinks near the bottom/right margins. */
int tge_grid_view_width(const TGE_GridView *view);
int tge_grid_view_height(const TGE_GridView *view);

/* Logical size for an arbitrary physical surface of w x h (no canvas needed),
 * e.g. the size a resize event implies before the canvas is updated.
 * Delegates to tge_grid_size_for. */
void tge_grid_view_size_for(const TGE_GridView *view, int w, int h, int *gw,
                            int *gh);

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

#ifdef __cplusplus
}
#endif

#endif
