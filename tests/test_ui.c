#include "tge-extra/ui.h"

#include "tge/tge_canvas.h"
#include "tge_internal.h"
#include "tge_test.h"

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

static bool row_has_fg(const TGE_Canvas *c, int y, TGE_Color color)
{
    for (int x = 0; x < c->width; x++)
        if (cell_at(c, x, y)->fg.data.index == color.data.index)
            return true;
    return false;
}

static bool row_has_char(const TGE_Canvas *c, int y, uint32_t ch)
{
    for (int x = 0; x < c->width; x++)
        if (cell_at(c, x, y)->ch == ch)
            return true;
    return false;
}

TGE_TEST(modal_draws_bar_title_and_subtitle)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    tge_draw_modal(c, " GAME OVER ", " restart ", TGE_COLOR_RED);

    int title_row = 20 / 2 - 1;
    int gap_row = 20 / 2;
    int subtitle_row = 20 / 2 + 1;

    TGE_ASSERT(row_has_fg(c, title_row, TGE_COLOR_RED),
               "title in red on its row");
    TGE_ASSERT(row_has_char(c, title_row, 'G'), "title text present");
    TGE_ASSERT(row_has_fg(c, subtitle_row, TGE_COLOR_WHITE),
               "subtitle in white on its row");
    TGE_ASSERT(row_has_char(c, subtitle_row, 'r'), "subtitle text present");

    for (int x = 1; x < c->width - 1; x++) {
        const TGE_Cell *cell = cell_at(c, x, gap_row);
        TGE_ASSERT(cell->ch == ' ' &&
                       cell->bg.data.index == TGE_COLOR_BLACK.data.index,
                   "bar row blank on black");
    }
    tge_canvas_destroy(c);
}

TGE_TEST(modal_title_color_follows_argument)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    tge_draw_modal(c, " PAUSED ", " resume ", TGE_COLOR_YELLOW);
    int title_row = 20 / 2 - 1;
    TGE_ASSERT(row_has_fg(c, title_row, TGE_COLOR_YELLOW),
               "title honors the passed color");
    TGE_ASSERT(!row_has_fg(c, title_row, TGE_COLOR_RED),
               "no red on a yellow title");
    tge_canvas_destroy(c);
}

TGE_TEST(modal_leaves_edges_untouched)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    tge_draw_modal(c, " GAME OVER ", " restart ", TGE_COLOR_RED);
    for (int y = 0; y < c->height; y++) {
        TGE_ASSERT(cell_at(c, 0, y)->ch == 0, "left edge untouched");
        TGE_ASSERT(cell_at(c, c->width - 1, y)->ch == 0,
                   "right edge untouched");
    }
    tge_canvas_destroy(c);
}

TGE_TEST(modal_tiny_canvas_is_safe)
{
    TGE_Canvas *c = tge_canvas_create(12, 3);
    tge_draw_modal(c, " GAME OVER ", " restart ", TGE_COLOR_RED);
    tge_canvas_destroy(c);
}

int main(void)
{
    test_modal_draws_bar_title_and_subtitle();
    test_modal_title_color_follows_argument();
    test_modal_leaves_edges_untouched();
    test_modal_tiny_canvas_is_safe();
    return tge_test_report();
}
