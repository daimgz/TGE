#ifndef TGE_EXTRA_GRID_H_
#define TGE_EXTRA_GRID_H_

#include <stdint.h>
#include "tge/tge_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* General glyph sprite: exactly `width` x `height` single-width UTF-8 glyphs
 * in row-major order (width columns per row). Sprites are independent of the
 * grid cell size: tge_grid_put() renders them at natural terminal size,
 * anchored at a grid position, so a 3x3 sprite occupies 3 columns and 3 rows
 * no matter how big a grid cell is. */
typedef struct {
    int width;
    int height;
    const char *utf8;
} TGE_Sprite;

/* Visual theme of a grid: one borrowed sprite per semantic tile role. Every
 * tile draw is resolved through the theme, so swapping the theme changes the
 * look of all the primitives without touching the game logic:
 *
 *   empty          cells that hold nothing (clear, erase)
 *   default_sprite the ordinary map cell: used when no other tile is given
 *                  (set_cell, fill, line, circle)
 *   border         walls and outlines (draw_border)
 *   selection      highlighted cells (put_tile with TGE_TILE_SELECTION)
 *
 * A NULL field makes the draws that use it skip with a message on stderr, so
 * misconfigurations are not silent. The built-in themes
 * (TGE_GRID_THEME_BLOCKS/ASCII/DOTS) always fill every field. */
typedef struct {
    const TGE_Sprite *empty;
    const TGE_Sprite *default_sprite;
    const TGE_Sprite *border;
    const TGE_Sprite *selection;
} TGE_GridTheme;

/* Semantic tile roles. A tile is resolved through the grid theme, so the game
 * code depends on the role (an empty cell, a wall, a normal cell) instead of
 * on the concrete glyphs. */
typedef enum {
    TGE_TILE_EMPTY,
    TGE_TILE_DEFAULT,
    TGE_TILE_BORDER,
    TGE_TILE_SELECTION,
    TGE_GRID_TILE_COUNT /* sentinel for validation, not a tile */
} TGE_GridTile;

/* Grid: a drawing layer for grid-based games (snake, sokoban, tetris,
 * roguelikes). It maps a logical coordinate space over a TGE_Canvas where
 * each logical cell is a cell_w x cell_h block of physical cells. Terminal
 * cells are not square (roughly 1:2 wide:tall), so at cell size 2x1 a logical
 * cell renders as two adjacent characters and looks visually square. Games
 * write in logical coordinates; the cell size decides the physical
 * representation, so the same game logic runs at any terminal resolution
 * without changes (e.g. grid 20x19 at 1x1/2x1/4x2 becomes 20x19/40x19/80x38).
 *
 * Grid coordinates are logical cells. draw_text() and draw_frame() are the
 * exception: they intentionally bypass cell scaling and keep glyphs at
 * natural terminal width, so HUD labels and UI panels keep their natural
 * size and are not stretched like the map cells.
 *
 * Scope: TGE_Grid is deliberately NOT a 2D engine. No tilemaps, cameras,
 * layers, entities, animations or z-order here: those belong in separate
 * modules. The grid only knows an origin, a cell size, a theme and the
 * drawing helpers above.
 *
 * The struct is small and the view is borrowed (no allocation): the caller
 * keeps owning the canvas, and the theme (with its sprites) is a borrowed
 * pointer that must outlive the grid.
 */
#define TGE_GRID_MAX_CELL_GLYPHS 32 /* max glyphs of a theme cell sprite */

typedef struct {
    TGE_Canvas *canvas;
    int ox;      /* physical origin column of the grid view */
    int oy;      /* physical origin row of the grid view */
    int cell_w;  /* logical cell width in characters */
    int cell_h;  /* logical cell height in rows */
    const TGE_GridTheme *theme; /* sprite set used by the tile draws */
} TGE_Grid;

/* Ready-made themes:
 *
 *   TGE_GRID_THEME_BLOCKS  empty "  ", default "██", border "██", selection "▓▓"
 *                          the classic game look (Unicode blocks).
 *   TGE_GRID_THEME_ASCII   empty "  ", default "[]", border "##", selection "<>"
 *                          plain ASCII, for terminals without Unicode.
 *   TGE_GRID_THEME_DOTS    empty "· ", default "oo", border "##", selection "()"
 *                          shows that a sprite need not be a solid block.
 *
 * They are designed for square pixels (2x1 cells). */
extern const TGE_GridTheme TGE_GRID_THEME_BLOCKS;
extern const TGE_GridTheme TGE_GRID_THEME_ASCII;
extern const TGE_GridTheme TGE_GRID_THEME_DOTS;

/* Attach a grid view to `canvas`. Cell size defaults to 1x1, origin to
 * (0, 0) and the theme to TGE_GRID_THEME_BLOCKS. */
void tge_grid_init(TGE_Grid *g, TGE_Canvas *canvas);

/* Move the physical origin of the grid view. */
void tge_grid_set_origin(TGE_Grid *g, int ox, int oy);

/* Change the size of one logical cell in physical cells. Sizes <= 0 default
 * to 1. */
void tge_grid_set_cell_size(TGE_Grid *g, int cell_w, int cell_h);

/* Shorthand for tge_grid_set_cell_size(g, 2, 1): cells render as two
 * characters, the size that looks square on a typical terminal. */
#define tge_grid_square_pixels(g) tge_grid_set_cell_size((g), 2, 1)

/* Logical size of the view (floor of the physical space / cell size). */
int tge_grid_width(const TGE_Grid *g);
int tge_grid_height(const TGE_Grid *g);

/* Logical size for an arbitrary physical surface of w x h, without touching
 * a canvas. Same math as tge_grid_width/height (origin + cell size applied to
 * the given w/h), for callers that have raw dimensions instead of a canvas
 * (e.g. a resize event before the canvas is updated). */
void tge_grid_size_for(const TGE_Grid *g, int w, int h, int *gw, int *gh);

/* Write one logical cell with the `tile` sprite of the grid theme. An invalid
 * tile or a theme with no sprite for it is skipped with a message on stderr. */
void tge_grid_put_tile(TGE_Grid *g, int lx, int ly, TGE_GridTile tile,
                       TGE_Color fg, TGE_Color bg);

/* Draw a general sprite anchored at a logical position. The sprite is drawn
 * at natural terminal size regardless of the cell size: it is NOT stretched
 * or tiled, its top-left glyph lands on the logical cell's top-left physical
 * cell, so a 2x1 sprite on a 4x2 cell still renders 2 columns by 1 row.
 * `sprite` must contain exactly width*height single-width glyphs; on a
 * mismatch the draw is skipped and a message is printed to stderr, so
 * misconfigurations are not silent. */
void tge_grid_put(TGE_Grid *g, int lx, int ly, const TGE_Sprite *sprite,
                  TGE_Color fg, TGE_Color bg);

/* Fill a logical rectangle of cells with the `tile` sprite of the theme. */
void tge_grid_fill(TGE_Grid *g, int lx, int ly, int lw, int lh,
                   TGE_GridTile tile, TGE_Color fg, TGE_Color bg);

/* Write one logical cell with the theme `default_sprite` (the ordinary map
 * cell). Shorthand for tge_grid_put_tile(g, x, y, TGE_TILE_DEFAULT, ...). */
void tge_grid_set_cell(TGE_Grid *g, int lx, int ly, TGE_Color fg,
                       TGE_Color bg);

/* Write one logical cell with the theme `empty` sprite. */
void tge_grid_erase(TGE_Grid *g, int lx, int ly, TGE_Color fg, TGE_Color bg);

/* Border of a logical rectangle, one logical cell thick, drawn with the theme
 * `border` sprite. */
void tge_grid_draw_border(TGE_Grid *g, int lx, int ly, int lw, int lh,
                          TGE_Color fg, TGE_Color bg);

/* Fill the whole logical view with the theme `empty` sprite. */
void tge_grid_clear(TGE_Grid *g, TGE_Color fg, TGE_Color bg);

/* Box-drawing frame (corner glyphs) around the physical rectangle spanned by
 * the logical rect, at natural terminal width (unscaled): the grid-level
 * counterpart of tge_draw_frame. Use it to delimit HUD panels or regions
 * without leaving logical coordinates. */
void tge_grid_draw_frame(TGE_Grid *g, int lx, int ly, int lw, int lh,
                         TGE_Color fg, TGE_Color bg);

/* Bresenham line between two logical cells, drawn with the `default_sprite`. */
void tge_grid_draw_line(TGE_Grid *g, int x1, int y1, int x2, int y2,
                        TGE_Color fg, TGE_Color bg);

/* Outline circle of logical radius r centered on (cx, cy), drawn with the
 * `default_sprite`. */
void tge_grid_draw_circle(TGE_Grid *g, int cx, int cy, int r, TGE_Color fg,
                          TGE_Color bg);

/* Text at a logical position, glyphs at natural terminal width (unscaled),
 * which suits HUD labels that should not be stretched. */
void tge_grid_draw_text(TGE_Grid *g, int lx, int ly, const char *text,
                        TGE_Color fg, TGE_Color bg);

#ifdef __cplusplus
}
#endif

#endif
