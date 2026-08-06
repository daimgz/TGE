#include "tge_internal.h"
#include <stdlib.h>

static bool cell_equal(const TGE_Cell *a, const TGE_Cell *b)
{
    if (a->ch != b->ch || a->attr != b->attr)
        return false;
    if (a->fg.mode != b->fg.mode || a->bg.mode != b->bg.mode)
        return false;
    if (a->fg.mode == TGE_COLOR_MODE_INDEXED) {
        return a->fg.data.index == b->fg.data.index &&
               a->bg.data.index == b->bg.data.index;
    }
    /* Default colors carry no payload: both cells already have mode DEFAULT,
     * so they are always equal regardless of any stale union data. */
    if (a->fg.mode == TGE_COLOR_MODE_DEFAULT)
        return true;
    return a->fg.data.rgb.r == b->fg.data.rgb.r &&
           a->fg.data.rgb.g == b->fg.data.rgb.g &&
           a->fg.data.rgb.b == b->fg.data.rgb.b &&
           a->bg.data.rgb.r == b->bg.data.rgb.r &&
           a->bg.data.rgb.g == b->bg.data.rgb.g &&
           a->bg.data.rgb.b == b->bg.data.rgb.b;
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
