/* 08_grid_canvas - terminal cells are not square (roughly 1:2), so drawing
 * at 1x1 makes circles look like ellipses and horizontal motion look slower
 * than vertical. TGE_Grid maps each logical cell to a cell_w x cell_h block
 * of characters; with tge_grid_square_pixels (2x1) a logical cell renders as
 * two characters and looks visually square.
 *
 *  - Top: the same circle drawn on a raw canvas (stretched) and on a 2x1
 *    grid (square).
 *  - Middle: the grid themes. The same draw calls (set_cell for the default
 *    tile, put_tile for the selection tile) render three different looks
 *    just by swapping grid.theme: BLOCKS, DOTS and ASCII.
 *  - Bottom: a ball bouncing in a logical 2x1 box, with equal speed on both
 *    axes, so the path is a real square.
 */
#include "tge/tge.h"

#include "tge-extra/grid.h"

#include <math.h>
#include <stdio.h>

static float g_t;
static float g_bx = 5.0f;
static float g_by = 3.0f;
static float g_vx = 3.5f;
static float g_vy = 2.2f;

static void init(TGE_App *app)
{
    (void)app;
}

static void update(TGE_App *app, float dt)
{
    (void)app;
    g_t += dt;
    g_bx += g_vx * dt;
    g_by += g_vy * dt;
    if (g_bx < 1.0f) { g_bx = 1.0f; g_vx = -g_vx; }
    if (g_bx > 27.0f) { g_bx = 27.0f; g_vx = -g_vx; }
    if (g_by < 1.0f) { g_by = 1.0f; g_vy = -g_vy; }
    if (g_by > 4.0f) { g_by = 4.0f; g_vy = -g_vy; }
}

static void draw(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    tge_draw_text(canvas, 2, 0,
                  "same circle: raw 1x1 (left) vs grid 2x1 (right)",
                  TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 1, "1x1", TGE_COLOR_MAGENTA, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 33, 1, "2x1", TGE_COLOR_GREEN, TGE_COLOR_BLACK);

    tge_draw_circle(canvas, 14, 7, 5, 0x2588, TGE_COLOR_MAGENTA,
                    TGE_COLOR_BLACK);

    TGE_Grid grid;
    tge_grid_init(&grid, canvas);
    tge_grid_square_pixels(&grid);
    tge_grid_set_origin(&grid, 31, 2);
    tge_grid_draw_circle(&grid, 7, 5, 4, TGE_COLOR_GREEN, TGE_COLOR_BLACK);

    TGE_Grid demo;
    tge_grid_init(&demo, canvas);
    tge_grid_square_pixels(&demo);
    tge_grid_set_origin(&demo, 2, 15);
    demo.theme = &TGE_GRID_THEME_BLOCKS;
    tge_grid_set_cell(&demo, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_put_tile(&demo, 1, 0, TGE_TILE_SELECTION, TGE_COLOR_YELLOW,
                      TGE_COLOR_BLACK);
    demo.theme = &TGE_GRID_THEME_DOTS;
    tge_grid_set_cell(&demo, 4, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_put_tile(&demo, 5, 0, TGE_TILE_SELECTION, TGE_COLOR_YELLOW,
                      TGE_COLOR_BLACK);
    demo.theme = &TGE_GRID_THEME_ASCII;
    tge_grid_set_cell(&demo, 8, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_put_tile(&demo, 9, 0, TGE_TILE_SELECTION, TGE_COLOR_YELLOW,
                      TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 14,
                  "theme swap: BLOCKS | DOTS | ASCII (default + selection)",
                  TGE_COLOR_CYAN, TGE_COLOR_BLACK);

    TGE_Grid bounce;
    tge_grid_init(&bounce, canvas);
    tge_grid_square_pixels(&bounce);
    tge_grid_set_origin(&bounce, 1, 17);
    tge_grid_draw_border(&bounce, 0, 0, 29, 6, TGE_COLOR_WHITE,
                         TGE_COLOR_BLACK);
    tge_grid_set_cell(&bounce, (int)lroundf(g_bx), (int)lroundf(g_by),
                      TGE_COLOR_YELLOW, TGE_COLOR_BLACK);

    tge_draw_text(canvas, 2, 16, "bouncing ball at 2x1 (equal vx, vy)",
                  TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    tge_draw_text(canvas, 2, 23, "[ESC] quit", TGE_COLOR_WHITE,
                  TGE_COLOR_BLACK);
}

static void on_event(TGE_App *app, TGE_Event *ev)
{
    if (ev->type == TGE_EVENT_KEYDOWN &&
        ev->data.key.keycode == TGE_KEY_ESC) {
        TGE_Quit(app);
    }
}

int main(void)
{
    TGE_App *app = TGE_Create(60, 24, "TGE grid");
    if (!app)
        return 1;
    TGE_Run(app, init, update, draw, on_event);
    TGE_Destroy(app);
    return 0;
}
