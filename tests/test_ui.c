#include "tge-extra/ui.h"

#include "tge/tge_canvas.h"
#include "tge/tge_math.h"
#include "tge/tge_unicode.h"
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

/* tge_draw_modal: a centered overlay bar with title + subtitle. */
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

/* tge_draw_region: a bordered panel with an optional title on the top border.
 * The content area is left untouched: laying it out (e.g. tge_rect_inset) is
 * the caller's job, so the helper must not draw an extra content row. */
TGE_TEST(draw_region_frame_and_title)
{
    TGE_Canvas *c = tge_canvas_create(12, 8);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_region(c, (TGE_Rect){1, 1, 8, 4}, "AB", TGE_COLOR_YELLOW);

    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0x250C, "TL corner");
    TGE_ASSERT(cell_at(c, 8, 1)->ch == 0x2510, "TR corner");
    TGE_ASSERT(cell_at(c, 1, 4)->ch == 0x2514, "BL corner");
    TGE_ASSERT(cell_at(c, 8, 4)->ch == 0x2518, "BR corner");

    /* title sits on the top border, inset by one column */
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 'A', "title char A");
    TGE_ASSERT(cell_at(c, 3, 1)->ch == 'B', "title char B");
    TGE_ASSERT(cell_at(c, 4, 1)->ch == 0x2500, "top edge after title");

    /* frame and title share the single fg */
    TGE_ASSERT(cell_at(c, 2, 1)->fg.data.index == 3, "title fg yellow");
    TGE_ASSERT(cell_at(c, 1, 1)->fg.data.index == 3, "frame fg yellow");

    /* content area is untouched: inset is the caller's responsibility */
    TGE_ASSERT(cell_at(c, 2, 2)->ch == ' ', "inside top empty");
    TGE_ASSERT(cell_at(c, 7, 3)->ch == ' ', "inside bottom empty");

    tge_canvas_destroy(c);
}

/* A NULL title draws a plain frame; the top border stays an edge, never a
 * letter, and no content row is drawn. */
TGE_TEST(draw_region_null_title_is_plain_frame)
{
    TGE_Canvas *c = tge_canvas_create(12, 8);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_draw_region(c, (TGE_Rect){1, 1, 8, 4}, NULL, TGE_COLOR_CYAN);

    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0x250C, "TL corner");
    TGE_ASSERT(cell_at(c, 8, 1)->ch == 0x2510, "TR corner");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 0x2500, "top edge, no title");
    TGE_ASSERT(cell_at(c, 4, 1)->ch != 'T', "no stray title glyph");
    TGE_ASSERT(cell_at(c, 2, 2)->ch == ' ', "inside empty without title");

    tge_canvas_destroy(c);
}

int main(void)
{
    tge_unicode_set_mode(TGE_UNICODE_ON);
    test_modal_draws_bar_title_and_subtitle();
    test_modal_title_color_follows_argument();
    test_modal_leaves_edges_untouched();
    test_modal_tiny_canvas_is_safe();
    test_draw_region_frame_and_title();
    test_draw_region_null_title_is_plain_frame();
    return tge_test_report();
}
