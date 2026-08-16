#pragma once

#include "tge/tge_canvas.h"
#include "tge-extra/grid.h"
#include "tge/canvas.hpp"
#include "tge/color.hpp"

namespace tge {

/* Fixed-size drawing layer over a TGE_Canvas. Wraps TGE_Grid and exposes the
 * primitives a grid game needs (snake, sokoban, tetris, roguelikes). Unlike
 * tge::Playfield, a Grid does NOT resize to the window: its logical size is
 * fixed by set_cell_size + set_origin, so a 10x20 Tetris board stays 10x20 at
 * any terminal size. The caller keeps owning the canvas (re-attached each
 * frame) and the theme (borrowed pointer, must outlive the grid).
 *
 * Consumer: Tetris (Fase 0) needed a fixed board; the same Grid is what the
 * earlier grid probes (snake/sokoban/minesweeper/pacman) used raw, so it is a
 * generalizable C++ surface, not a Tetris-specific one. */
struct Grid {
    TGE_Grid raw;

    void init(const TGE_GridTheme *theme = &TGE_GRID_THEME_BLOCKS) {
        tge_grid_init(&raw, nullptr);
        raw.theme = theme;
    }

    /* Re-point at the canvas drawn into this frame (the app swaps its double
     * buffers, so the pointer changes every present). */
    void attach(Canvas canvas) { tge_grid_attach(&raw, canvas.raw); }

    void set_origin(int x, int y) { tge_grid_set_origin(&raw, x, y); }
    void set_cell_size(int w, int h) { tge_grid_set_cell_size(&raw, w, h); }
    void square_pixels() { tge_grid_set_cell_size(&raw, 2, 1); }

    /* Logical size of the attached view. */
    int width() const { return tge_grid_width(&raw); }
    int height() const { return tge_grid_height(&raw); }

    /* One logical cell with the theme default sprite (the ordinary map cell). */
    void set_cell(int lx, int ly, Color fg, Color bg) {
        tge_grid_set_cell(&raw, lx, ly, fg, bg);
    }

    /* One logical cell with an explicit sprite + attributes (e.g. a dimmed
     * ghost piece: draw the blocks sprite with TGE_CELL_ATTR_DIM). */
    void put_attr(int lx, int ly, const TGE_Sprite *sprite, Color fg,
                  Color bg, uint8_t attr) {
        tge_grid_put_attr(&raw, lx, ly, sprite, fg, bg, attr);
    }

    /* One logical cell with the theme empty sprite. */
    void erase(int lx, int ly, Color fg, Color bg) {
        tge_grid_erase(&raw, lx, ly, fg, bg);
    }

    /* Border of a logical rectangle, one logical cell thick. */
    void draw_border(int lx, int ly, int lw, int lh, Color fg, Color bg) {
        tge_grid_draw_border(&raw, lx, ly, lw, lh, fg, bg);
    }
};

} // namespace tge
