#include "tge/tge_canvas.h"
#include "tge_internal.h"
#include "tge_test.h"

static TGE_Canvas *make_canvas(int w, int h)
{
    return tge_canvas_create(w, h);
}

TGE_TEST(no_changes_empty_diff)
{
    TGE_Canvas *a = make_canvas(5, 3);
    TGE_Canvas *b = make_canvas(5, 3);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 0, "identical canvases produce no spans");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(single_cell_single_span)
{
    TGE_Canvas *a = make_canvas(5, 3);
    TGE_Canvas *b = make_canvas(5, 3);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(a, 2, 1, 'X', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 1, "one span");
    TGE_ASSERT(d.spans[0].y == 1, "row 1");
    TGE_ASSERT(d.spans[0].x_start == 2 && d.spans[0].x_end == 3,
               "half-open [2,3)");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(consecutive_cells_one_span)
{
    TGE_Canvas *a = make_canvas(8, 1);
    TGE_Canvas *b = make_canvas(8, 1);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_text(a, 2, 0, "Hola", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 1, "one span for contiguous run");
    TGE_ASSERT(d.spans[0].x_start == 2 && d.spans[0].x_end == 6,
               "span covers 4 cells");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(nonconsecutive_multiple_spans)
{
    TGE_Canvas *a = make_canvas(8, 1);
    TGE_Canvas *b = make_canvas(8, 1);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(a, 1, 0, 'X', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_set_cell(a, 5, 0, 'Y', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 2, "two spans");
    TGE_ASSERT(d.spans[0].x_start == 1 && d.spans[0].x_end == 2, "span 0");
    TGE_ASSERT(d.spans[1].x_start == 5 && d.spans[1].x_end == 6, "span 1");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(multiple_rows)
{
    TGE_Canvas *a = make_canvas(4, 4);
    TGE_Canvas *b = make_canvas(4, 4);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(a, 0, 0, 'A', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_set_cell(a, 3, 3, 'B', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 2, "two rows changed");
    TGE_ASSERT(d.spans[0].y == 0, "first span row 0");
    TGE_ASSERT(d.spans[1].y == 3, "second span row 3");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(full_refresh_one_span_per_row)
{
    TGE_Canvas *a = make_canvas(6, 3);
    TGE_Canvas *b = make_canvas(6, 3);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, '#', TGE_COLOR_RED, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 32);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 3, "one span per row");
    for (int i = 0; i < 3; i++) {
        TGE_ASSERT(d.spans[i].y == i, "row index");
        TGE_ASSERT(d.spans[i].x_start == 0 && d.spans[i].x_end == 6,
                   "full row");
    }
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(color_change_detected)
{
    TGE_Canvas *a = make_canvas(3, 1);
    TGE_Canvas *b = make_canvas(3, 1);
    tge_clear(a, 'X', TGE_COLOR_RED, TGE_COLOR_BLACK);
    tge_clear(b, 'X', TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 8);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 1, "color change produces diff");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(diff_grows_capacity)
{
    TGE_Canvas *a = make_canvas(20, 4);
    TGE_Canvas *b = make_canvas(20, 4);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 20; x++)
            if ((x + y) % 2 == 0)
                tge_set_cell(a, x, y, 'Z', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 4);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count > 4, "capacity grown for many spans");
    TGE_ASSERT(d.spans != 0, "spans valid");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(diff_size_mismatch)
{
    TGE_Canvas *a = make_canvas(5, 3);
    TGE_Canvas *b = make_canvas(5, 4);
    tge_clear(a, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_clear(b, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(a, b, &d);
    TGE_ASSERT(d.count == 0, "mismatched sizes produce empty diff");
    tge_diff_free(&d);
    tge_canvas_destroy(a);
    tge_canvas_destroy(b);
}

TGE_TEST(null_arguments_safe)
{
    TGE_Diff d;
    tge_diff_init(&d, 16);
    tge_renderer_diff(NULL, NULL, &d);
    TGE_ASSERT(d.count == 0, "null canvases safe");
    tge_diff_free(&d);
}

int main(void)
{
    test_no_changes_empty_diff();
    test_single_cell_single_span();
    test_consecutive_cells_one_span();
    test_nonconsecutive_multiple_spans();
    test_multiple_rows();
    test_full_refresh_one_span_per_row();
    test_color_change_detected();
    test_diff_grows_capacity();
    test_diff_size_mismatch();
    test_null_arguments_safe();
    return tge_test_report();
}
