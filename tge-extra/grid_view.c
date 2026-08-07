#include "tge-extra/grid_view.h"

#include <stdbool.h>
#include <stddef.h>

void tge_grid_view_init(TGE_GridView *view, const TGE_GridTheme *theme,
                        TGE_GridScale scale)
{
    if (!view)
        return;
    tge_grid_init(&view->grid, NULL);
    view->grid.theme = theme ? theme : &TGE_GRID_THEME_BLOCKS;
    if (scale == TGE_GRID_SCALE_2X1)
        tge_grid_square_pixels(&view->grid);
    else
        tge_grid_set_cell_size(&view->grid, 1, 1);
}

void tge_grid_view_attach(TGE_GridView *view, TGE_Canvas *canvas)
{
    if (!view)
        return;
    tge_grid_attach(&view->grid, canvas);
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

void tge_grid_layout_init(TGE_GridLayout *layout, TGE_GridView *view)
{
    if (!layout)
        return;
    layout->view = view;
    layout->cached_width = 0;
    layout->cached_height = 0;
}

bool tge_grid_layout_sync(TGE_GridLayout *layout, int surface_w,
                          int surface_h, tge_grid_layout_resize_fn on_resize,
                          void *userdata)
{
    if (!layout || !layout->view)
        return false;
    int grid_w, grid_h;
    tge_grid_view_size_for(layout->view, surface_w, surface_h, &grid_w,
                           &grid_h);
    if (grid_w == layout->cached_width && grid_h == layout->cached_height)
        return false;
    layout->cached_width = grid_w;
    layout->cached_height = grid_h;
    if (on_resize)
        on_resize(userdata, grid_w, grid_h);
    return true;
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

void tge_grid_view_set_cell_local(TGE_GridView *view, const TGE_View *layout,
                                  TGE_Vec2i local, TGE_Color fg, TGE_Color bg)
{
    if (!view || !layout)
        return;
    TGE_Vec2i p = tge_view_translate(layout, local);
    tge_grid_view_set_cell(view, p.x, p.y, fg, bg);
}

void tge_grid_view_put_local(TGE_GridView *view, const TGE_View *layout,
                             TGE_Vec2i local, const TGE_Sprite *sprite,
                             TGE_Color fg, TGE_Color bg)
{
    if (!view || !layout)
        return;
    TGE_Vec2i p = tge_view_translate(layout, local);
    tge_grid_view_put(view, p.x, p.y, sprite, fg, bg);
}

void tge_grid_view_put_attr_local(TGE_GridView *view, const TGE_View *layout,
                                  TGE_Vec2i local, const TGE_Sprite *sprite,
                                  TGE_Color fg, TGE_Color bg, uint8_t attr)
{
    if (!view || !layout)
        return;
    TGE_Vec2i p = tge_view_translate(layout, local);
    tge_grid_view_put_attr(view, p.x, p.y, sprite, fg, bg, attr);
}
