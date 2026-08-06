#include "tge-extra/grid_view.h"

void tge_grid_view_init(TGE_GridView *view, TGE_Canvas *canvas,
                        const TGE_GridTheme *theme, TGE_GridScale scale)
{
    if (!view)
        return;
    tge_grid_init(&view->grid, canvas);
    view->grid.theme = theme ? theme : &TGE_GRID_THEME_BLOCKS;
    if (scale == TGE_GRID_SCALE_2X1)
        tge_grid_square_pixels(&view->grid);
    else
        tge_grid_set_cell_size(&view->grid, 1, 1);
}

int tge_grid_view_width(const TGE_GridView *view)
{
    if (!view)
        return 0;
    return tge_grid_width(&view->grid);
}

int tge_grid_view_height(const TGE_GridView *view)
{
    if (!view)
        return 0;
    return tge_grid_height(&view->grid);
}

void tge_grid_view_size_for(const TGE_GridView *view, int w, int h, int *gw,
                            int *gh)
{
    if (!view)
        return;
    tge_grid_size_for(&view->grid, w, h, gw, gh);
}

void tge_grid_view_draw_border(TGE_GridView *view, TGE_Color fg, TGE_Color bg)
{
    if (!view)
        return;
    tge_grid_draw_border(&view->grid, 0, 0, tge_grid_width(&view->grid),
                         tge_grid_height(&view->grid), fg, bg);
}

void tge_grid_view_set_cell(TGE_GridView *view, int lx, int ly, TGE_Color fg,
                            TGE_Color bg)
{
    if (!view)
        return;
    tge_grid_set_cell(&view->grid, lx, ly, fg, bg);
}

void tge_grid_view_put(TGE_GridView *view, int lx, int ly,
                       const TGE_Sprite *sprite, TGE_Color fg, TGE_Color bg)
{
    if (!view)
        return;
    tge_grid_put(&view->grid, lx, ly, sprite, fg, bg);
}

void tge_grid_view_put_attr(TGE_GridView *view, int lx, int ly,
                            const TGE_Sprite *sprite, TGE_Color fg,
                            TGE_Color bg, uint8_t attr)
{
    if (!view)
        return;
    tge_grid_put_attr(&view->grid, lx, ly, sprite, fg, bg, attr);
}
