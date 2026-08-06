#include "tge-extra/view.h"

void tge_view_init(TGE_View *view, int min_width, int min_height)
{
    if (!view)
        return;
    view->area = (TGE_Rect){ 0, 0, 0, 0 };
    view->margin = 1;
    view->min_w = min_width;
    view->min_h = min_height;
    view->valid = false;
    view->first = true;
}

TGE_ViewUpdate tge_view_update(TGE_View *view, int width, int height)
{
    if (!view)
        return TGE_VIEW_INVALID;
    int m = view->margin;
    int w = width - 2 * m;
    int h = height - 2 * m;
    if (w < 0)
        w = 0;
    if (h < 0)
        h = 0;
    view->area = tge_rect(m, m, w, h);
    view->valid = (view->area.w >= view->min_w && view->area.h >= view->min_h);
    if (!view->valid)
        return TGE_VIEW_INVALID;
    if (view->first) {
        view->first = false;
        return TGE_VIEW_FIRST_VALID;
    }
    return TGE_VIEW_RESIZED;
}

TGE_Vec2i tge_view_translate(const TGE_View *view, TGE_Vec2i local)
{
    if (!view)
        return tge_vec2i_zero();
    return tge_rect_translate_point(view->area, local);
}

bool tge_view_contains(const TGE_View *view, TGE_Vec2i p)
{
    if (!view)
        return false;
    return p.x >= 0 && p.x < view->area.w && p.y >= 0 && p.y < view->area.h;
}

TGE_Vec2i tge_view_random_point(const TGE_View *view)
{
    if (!view)
        return tge_vec2i_zero();
    return tge_rect_random_point(tge_view_local_bounds(view));
}

TGE_Rect tge_view_local_bounds(const TGE_View *view)
{
    if (!view)
        return tge_rect(0, 0, 0, 0);
    return tge_rect(0, 0, view->area.w, view->area.h);
}
