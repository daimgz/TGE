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
