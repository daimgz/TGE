#include "playfield.h"

void tge_playfield_init(TGE_Playfield *pf, const TGE_GridTheme *theme,
                        TGE_GridScale scale, int min_w, int min_h)
{
    if (!pf)
        return;
    tge_view_init(&pf->view, min_w, min_h);
    tge_grid_view_init(&pf->grid_view, theme, scale);
    tge_grid_layout_init(&pf->layout, &pf->grid_view);
}

void tge_playfield_attach(TGE_Playfield *pf, TGE_Canvas *canvas)
{
    if (pf)
        tge_grid_view_attach(&pf->grid_view, canvas);
}

bool tge_playfield_sync(TGE_Playfield *pf, int surface_w, int surface_h,
                        tge_grid_layout_resize_fn on_resize, void *userdata)
{
    if (!pf)
        return false;
    return tge_grid_layout_sync(&pf->layout, surface_w, surface_h, on_resize,
                                userdata);
}

void tge_playfield_draw_border(TGE_Playfield *pf, TGE_Color fg, TGE_Color bg)
{
    if (pf)
        tge_grid_view_draw_border(&pf->grid_view, fg, bg);
}
