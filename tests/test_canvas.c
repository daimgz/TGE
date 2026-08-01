#include "tge/tge_canvas.h"
#include "tge/tge_utf8.h"
#include "tge_internal.h"
#include "tge_test.h"

#include <string.h>

static TGE_Canvas *make_canvas(int w, int h)
{
    return tge_canvas_create(w, h);
}

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

TGE_TEST(create_destroy)
{
    TGE_Canvas *c = make_canvas(10, 5);
    TGE_ASSERT(c != 0, "canvas created");
    tge_canvas_destroy(c);
}

TGE_TEST(create_invalid_size)
{
    TGE_ASSERT(make_canvas(0, 5) == 0, "zero width rejected");
    TGE_ASSERT(make_canvas(5, -1) == 0, "negative height rejected");
}

TGE_TEST(width_height)
{
    TGE_Canvas *c = make_canvas(20, 8);
    TGE_ASSERT(tge_canvas_width(c) == 20, "width");
    TGE_ASSERT(tge_canvas_height(c) == 8, "height");
    tge_canvas_destroy(c);
}

TGE_TEST(clear_fills_all)
{
    TGE_Canvas *c = make_canvas(4, 3);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 4; x++) {
            const TGE_Cell *cell = cell_at(c, x, y);
            TGE_ASSERT(cell->ch == ' ', "clear char");
            TGE_ASSERT(cell->fg.data.index == 0 && cell->bg.data.index == 0,
                       "clear colors");
            TGE_ASSERT(cell->attr == 0, "clear attr");
        }
    }
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_basic)
{
    TGE_Canvas *c = make_canvas(5, 5);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(c, 2, 3, 'X', TGE_COLOR_RED, TGE_COLOR_BLUE);
    const TGE_Cell *cell = cell_at(c, 2, 3);
    TGE_ASSERT(cell->ch == 'X', "char set");
    TGE_ASSERT(cell->fg.data.index == 1, "fg red");
    TGE_ASSERT(cell->bg.data.index == 4, "bg blue");
    TGE_ASSERT(cell_at(c, 1, 3)->ch == ' ', "neighbor untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_clipping)
{
    TGE_Canvas *c = make_canvas(5, 5);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(c, -1, 0, 'A', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_set_cell(c, 5, 0, 'B', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_set_cell(c, 0, -1, 'C', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_set_cell(c, 0, 5, 'D', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_set_cell(c, 0, 0, 'E', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 'E', "in-bounds set");
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            if (!(x == 0 && y == 0))
                TGE_ASSERT(cell_at(c, x, y)->ch == ' ', "no write outside");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_wide_continuation)
{
    TGE_Canvas *c = make_canvas(6, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(c, 2, 0, 0x4E2D, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 2, 0)->ch == 0x4E2D, "wide base");
    TGE_ASSERT(cell_at(c, 3, 0)->ch == 0, "continuation marker");
    TGE_ASSERT(cell_at(c, 1, 0)->ch == ' ', "before untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_wide_at_edge_clips)
{
    TGE_Canvas *c = make_canvas(5, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_set_cell(c, 4, 0, 0x4E2D, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 4, 0)->ch == 0x4E2D, "base at last column");
    TGE_ASSERT(cell_at(c, 3, 0)->ch == ' ', "no neighbor");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_text_ascii)
{
    TGE_Canvas *c = make_canvas(10, 2);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_text(c, 1, 0, "Hola", TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 1, 0)->ch == 'H', "H");
    TGE_ASSERT(cell_at(c, 2, 0)->ch == 'o', "o");
    TGE_ASSERT(cell_at(c, 3, 0)->ch == 'l', "l");
    TGE_ASSERT(cell_at(c, 4, 0)->ch == 'a', "a");
    TGE_ASSERT(cell_at(c, 0, 0)->ch == ' ', "before");
    TGE_ASSERT(cell_at(c, 1, 0)->fg.data.index == 3, "fg yellow");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_text_right_clip)
{
    TGE_Canvas *c = make_canvas(5, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_text(c, 3, 0, "ABCDEF", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 3, 0)->ch == 'A', "A at 3");
    TGE_ASSERT(cell_at(c, 4, 0)->ch == 'B', "B at 4");
    TGE_ASSERT(cell_at(c, 0, 0)->ch == ' ' && cell_at(c, 2, 0)->ch == ' ',
               "no spill");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_text_left_clip)
{
    TGE_Canvas *c = make_canvas(5, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_text(c, -3, 0, "ABCDEF", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 'D', "D first visible");
    TGE_ASSERT(cell_at(c, 1, 0)->ch == 'E', "E");
    TGE_ASSERT(cell_at(c, 2, 0)->ch == 'F', "F");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_text_utf8_wide)
{
    TGE_Canvas *c = make_canvas(8, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_text(c, 1, 0, "a中b", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 1, 0)->ch == 'a', "a");
    TGE_ASSERT(cell_at(c, 2, 0)->ch == 0x4E2D, "wide base");
    TGE_ASSERT(cell_at(c, 3, 0)->ch == 0, "continuation");
    TGE_ASSERT(cell_at(c, 4, 0)->ch == 'b', "b after wide");
    TGE_ASSERT(cell_at(c, 5, 0)->ch == ' ', "rest empty");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_text_wide_at_edge)
{
    TGE_Canvas *c = make_canvas(4, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_text(c, 3, 0, "中", TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 3, 0)->ch == ' ', "wide clipped at edge");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_rect_outline)
{
    TGE_Canvas *c = make_canvas(8, 6);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_rect(c, 1, 1, 4, 3, TGE_COLOR_RED, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0x2588, "top-left corner");
    TGE_ASSERT(cell_at(c, 4, 1)->ch == 0x2588, "top-right corner");
    TGE_ASSERT(cell_at(c, 1, 3)->ch == 0x2588, "bottom-left");
    TGE_ASSERT(cell_at(c, 4, 3)->ch == 0x2588, "bottom-right");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 0x2588, "top edge");
    TGE_ASSERT(cell_at(c, 2, 2)->ch == ' ', "inside empty");
    TGE_ASSERT(cell_at(c, 0, 0)->ch == ' ', "outside empty");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_frame_boxes)
{
    TGE_Canvas *c = make_canvas(8, 6);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_frame(c, 1, 1, 4, 3, TGE_COLOR_CYAN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0x250C, "TL corner");
    TGE_ASSERT(cell_at(c, 4, 1)->ch == 0x2510, "TR corner");
    TGE_ASSERT(cell_at(c, 1, 3)->ch == 0x2514, "BL corner");
    TGE_ASSERT(cell_at(c, 4, 3)->ch == 0x2518, "BR corner");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 0x2500, "top edge");
    TGE_ASSERT(cell_at(c, 1, 2)->ch == 0x2502, "left edge");
    tge_canvas_destroy(c);
}

TGE_TEST(fill_rect_clipped)
{
    TGE_Canvas *c = make_canvas(6, 4);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_fill_rect(c, -2, -1, 10, 6, '#', TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 6; x++)
            TGE_ASSERT(cell_at(c, x, y)->ch == '#', "fully covered");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_line_horizontal)
{
    TGE_Canvas *c = make_canvas(6, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_line(c, 1, 0, 4, 0, '-', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 1, 0)->ch == '-' && cell_at(c, 4, 0)->ch == '-',
               "endpoints");
    TGE_ASSERT(cell_at(c, 2, 0)->ch == '-', "mid");
    TGE_ASSERT(cell_at(c, 0, 0)->ch == ' ', "outside");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_line_diagonal)
{
    TGE_Canvas *c = make_canvas(5, 5);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_line(c, 0, 0, 4, 4, '*', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    for (int i = 0; i < 5; i++)
        TGE_ASSERT(cell_at(c, i, i)->ch == '*', "diagonal");
    TGE_ASSERT(cell_at(c, 1, 0)->ch == ' ', "off-diagonal");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_line_offscreen)
{
    TGE_Canvas *c = make_canvas(4, 4);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_line(c, -5, -5, 10, 10, '#', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            if (x == y)
                TGE_ASSERT(cell_at(c, x, y)->ch == '#', "visible diag");
            else
                TGE_ASSERT(cell_at(c, x, y)->ch == ' ', "clipped");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_circle_basic)
{
    TGE_Canvas *c = make_canvas(9, 7);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_circle(c, 4, 3, 2, 'o', TGE_COLOR_RED, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 4, 3)->ch == 'o', "center not drawn (r>=1 skip)");
    TGE_ASSERT(cell_at(c, 4, 1)->ch == 'o', "top point");
    TGE_ASSERT(cell_at(c, 4, 5)->ch == 'o', "bottom point");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_circle_zero_radius)
{
    TGE_Canvas *c = make_canvas(3, 3);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_circle(c, 1, 1, 0, 'o', TGE_COLOR_RED, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 1, 1)->ch == 'o', "r=0 draws center");
    tge_canvas_destroy(c);
}

TGE_TEST(canvas_resize)
{
    TGE_Canvas *c = make_canvas(4, 2);
    tge_clear(c, 'A', TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    tge_canvas_resize(c, 6, 3);
    TGE_ASSERT(tge_canvas_width(c) == 6 && tge_canvas_height(c) == 3,
               "new size");
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0, "cells zeroed after resize");
    tge_canvas_resize(c, 6, 3);
    TGE_ASSERT(tge_canvas_width(c) == 6, "same size no-op");
    tge_canvas_destroy(c);
}

int main(void)
{
    test_create_destroy();
    test_create_invalid_size();
    test_width_height();
    test_clear_fills_all();
    test_set_cell_basic();
    test_set_cell_clipping();
    test_set_cell_wide_continuation();
    test_set_cell_wide_at_edge_clips();
    test_draw_text_ascii();
    test_draw_text_right_clip();
    test_draw_text_left_clip();
    test_draw_text_utf8_wide();
    test_draw_text_wide_at_edge();
    test_draw_rect_outline();
    test_draw_frame_boxes();
    test_fill_rect_clipped();
    test_draw_line_horizontal();
    test_draw_line_diagonal();
    test_draw_line_offscreen();
    test_draw_circle_basic();
    test_draw_circle_zero_radius();
    test_canvas_resize();
    return tge_test_report();
}
