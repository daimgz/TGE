#include "tge_internal.h"
#include <stdlib.h>

/* Two colors are equal when their modes match and their payloads match. The
 * union stores the payload per mode: an RGB color compared through the
 * `index` member would only see the red byte, so each mode must read its own
 * field. Default colors carry no payload and always compare equal. */
static bool color_equal(const TGE_Color *a, const TGE_Color *b)
{
    if (a->mode != b->mode)
        return false;
    if (a->mode == TGE_COLOR_MODE_INDEXED)
        return a->data.index == b->data.index;
    if (a->mode == TGE_COLOR_MODE_DEFAULT)
        return true;
    return a->data.rgb.r == b->data.rgb.r &&
           a->data.rgb.g == b->data.rgb.g &&
           a->data.rgb.b == b->data.rgb.b;
}

static bool cell_equal(const TGE_Cell *a, const TGE_Cell *b)
{
    return a->ch == b->ch && a->attr == b->attr &&
           color_equal(&a->fg, &b->fg) && color_equal(&a->bg, &b->bg);
}

void tge_renderer_diff(const TGE_Canvas *current, const TGE_Canvas *previous,
                       struct TGE_Diff *diff)
{
    diff->count = 0;
    if (!current || !previous || !diff->spans)
        return;
    if (current->width != previous->width || current->height != previous->height)
        return;

    int w = current->width;
    int h = current->height;
    const TGE_Cell *cur = current->cells;
    const TGE_Cell *prev = previous->cells;

    for (int y = 0; y < h; y++) {
        const TGE_Cell *row_cur = cur + (size_t)y * (size_t)w;
        const TGE_Cell *row_prev = prev + (size_t)y * (size_t)w;
        int x = 0;
        while (x < w) {
            if (cell_equal(&row_cur[x], &row_prev[x])) {
                x++;
                continue;
            }
            int start = x;
            while (x < w && !cell_equal(&row_cur[x], &row_prev[x]))
                x++;
            if (diff->count >= diff->capacity) {
                int ncap = diff->capacity ? diff->capacity * 2 : 64;
                TGE_DirtySpan *ns = (TGE_DirtySpan *)realloc(
                    diff->spans, (size_t)ncap * sizeof(TGE_DirtySpan));
                if (!ns) {
                    diff->count = 0;
                    return;
                }
                diff->spans = ns;
                diff->capacity = ncap;
            }
            diff->spans[diff->count].y = y;
            diff->spans[diff->count].x_start = start;
            diff->spans[diff->count].x_end = x;
            diff->count++;
        }
    }
}

void tge_diff_init(TGE_Diff *diff, int capacity)
{
    diff->spans = (TGE_DirtySpan *)malloc((size_t)capacity * sizeof(TGE_DirtySpan));
    diff->count = 0;
    diff->capacity = diff->spans ? capacity : 0;
}

void tge_diff_free(TGE_Diff *diff)
{
    free(diff->spans);
    diff->spans = NULL;
    diff->capacity = 0;
    diff->count = 0;
}
