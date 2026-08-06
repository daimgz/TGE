#include "tge-extra/grid.h"

#include "tge/tge_canvas.h"
#include "tge/tge_unicode.h"
#include "tge_internal.h"
#include "tge_test.h"

#include <string.h>

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

static bool cell_is(const TGE_Canvas *c, int x, int y, uint32_t ch)
{
    const TGE_Cell *cell = cell_at(c, x, y);
    return cell->ch == ch && cell->fg.data.index == TGE_COLOR_GREEN.data.index;
}

static TGE_GridTheme theme_of(const char *empty, const char *def,
                              const char *border, const char *selection)
{
    static TGE_Sprite s[4];
    s[0] = (TGE_Sprite)TGE_SPRITE(2, 1, empty, NULL);
    s[1] = (TGE_Sprite)TGE_SPRITE(2, 1, def, NULL);
    s[2] = (TGE_Sprite)TGE_SPRITE(2, 1, border, NULL);
    s[3] = (TGE_Sprite)TGE_SPRITE(2, 1, selection, NULL);
    TGE_GridTheme t;
    t.empty = &s[0];
    t.default_sprite = &s[1];
    t.border = &s[2];
    t.selection = &s[3];
    return t;
}

TGE_TEST(init_defaults)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_Grid g;
    tge_grid_init(&g, c);
    TGE_ASSERT(g.cell_w == 1 && g.cell_h == 1, "default cell size 1x1");
    TGE_ASSERT(g.ox == 0 && g.oy == 0, "default origin 0,0");
    TGE_ASSERT(g.theme == &TGE_GRID_THEME_BLOCKS, "default theme is BLOCKS");
    TGE_ASSERT(tge_grid_width(&g) == 40, "40 logical cols at 1x1");
    TGE_ASSERT(tge_grid_height(&g) == 20, "20 logical rows at 1x1");
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_ASSERT(tge_grid_width(&g) == 20, "40/2 = 20 logical cols");
    TGE_ASSERT(tge_grid_height(&g) == 20, "20/1 = 20 logical rows");
    tge_grid_set_cell_size(&g, 0, -3);
    TGE_ASSERT(g.cell_w == 1 && g.cell_h == 1, "invalid sizes default to 1");
    tge_canvas_destroy(c);
}

TGE_TEST(square_pixels_helper)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_square_pixels(&g);
    TGE_ASSERT(g.cell_w == 2 && g.cell_h == 1, "square pixels = 2x1");
    TGE_ASSERT(tge_grid_width(&g) == 20, "40/2 logical cols");
    tge_canvas_destroy(c);
}

TGE_TEST(origin_and_cell_size_change_size)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 10, 5);
    TGE_ASSERT(tge_grid_width(&g) == 15, "(40-10)/2");
    TGE_ASSERT(tge_grid_height(&g) == 15, "(20-5)/1");
    tge_grid_set_cell_size(&g, 1, 2);
    TGE_ASSERT(tge_grid_width(&g) == 30, "resized width");
    TGE_ASSERT(tge_grid_height(&g) == 7, "resized height floor");
    tge_canvas_destroy(c);
}

TGE_TEST(size_for_matches_width_height)
{
    TGE_Canvas *c = tge_canvas_create(40, 20);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 10, 5);
    int gw, gh;
    tge_grid_size_for(&g, 40, 20, &gw, &gh);
    TGE_ASSERT(gw == tge_grid_width(&g) && gh == tge_grid_height(&g),
               "raw size matches canvas-backed size");
    tge_grid_size_for(&g, 80, 38, &gw, &gh);
    TGE_ASSERT(gw == 35 && gh == 33, "(80-10)/2 and (38-5)/1");
    tge_canvas_destroy(c);
}

TGE_TEST(size_for_no_canvas_needed)
{
    TGE_Grid g;
    tge_grid_init(&g, NULL);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 0, 1);
    int gw, gh;
    tge_grid_size_for(&g, 80, 24, &gw, &gh);
    TGE_ASSERT(gw == 40 && gh == 23, "2x1 cells, HUD row on top");
    tge_grid_size_for(&g, 0, 0, &gw, &gh);
    TGE_ASSERT(gw == 0 && gh == 0, "degenerate size");
}

TGE_TEST(size_for_null_safety)
{
    TGE_Grid g;
    tge_grid_init(&g, NULL);
    int gw = -1, gh = -1;
    tge_grid_size_for(NULL, 80, 24, &gw, &gh);
    TGE_ASSERT(gw == -1 && gh == -1, "NULL grid leaves outputs");
    tge_grid_size_for(&g, 80, 24, NULL, &gh);
    TGE_ASSERT(gh == -1, "NULL gw leaves gh");
    tge_grid_size_for(&g, 80, 24, &gw, NULL);
    TGE_ASSERT(gw == -1, "NULL gh leaves gw");
}

TGE_TEST(set_cell_fills_block_with_fill_tile)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_cell(&g, 1, 2, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 2, 0x2588), "fill block left col");
    TGE_ASSERT(cell_is(c, 3, 2, 0x2588), "fill block right col");
    TGE_ASSERT(cell_at(c, 1, 2)->ch == 0, "neighbor untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_1x1_writes_one_physical_cell)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell(&g, 3, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 3, 1, 0x2588), "cell set");
    TGE_ASSERT(cell_at(c, 4, 1)->ch == 0, "only one physical cell");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_clips_at_canvas_edge)
{
    TGE_Canvas *c = tge_canvas_create(7, 5);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_cell(&g, 3, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 6, 0, 0x2588), "in-bounds half set");
    TGE_ASSERT(cell_at(c, 5, 0)->ch == 0, "out-of-bounds half skipped");
    tge_canvas_destroy(c);
}

TGE_TEST(set_cell_negative_clips)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 2, 0);
    tge_grid_set_cell(&g, -1, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, 0x2588), "block maps to origin col 0");
    TGE_ASSERT(cell_is(c, 1, 0, 0x2588), "block right half");
    tge_grid_set_cell(&g, -2, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0x2588, "fully negative block clipped");
    tge_canvas_destroy(c);
}

TGE_TEST(put_tile_resolves_theme_sprites)
{
    TGE_Canvas *c = tge_canvas_create(8, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_GridTheme t = theme_of("..", "##", "::", "()");
    g.theme = &t;
    tge_grid_put_tile(&g, 0, 0, TGE_TILE_EMPTY, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    tge_grid_put_tile(&g, 1, 0, TGE_TILE_DEFAULT, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    tge_grid_put_tile(&g, 2, 0, TGE_TILE_BORDER, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    tge_grid_put_tile(&g, 3, 0, TGE_TILE_SELECTION, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '.') && cell_is(c, 1, 0, '.'),
               "empty tile drawn");
    TGE_ASSERT(cell_is(c, 2, 0, '#') && cell_is(c, 3, 0, '#'),
               "fill tile drawn");
    TGE_ASSERT(cell_is(c, 4, 0, ':') && cell_is(c, 5, 0, ':'),
               "border tile drawn");
    TGE_ASSERT(cell_is(c, 6, 0, '(') && cell_is(c, 7, 0, ')'),
               "selection tile drawn");
    tge_canvas_destroy(c);
}

TGE_TEST(theme_swap_changes_look_without_touching_draw_calls)
{
    TGE_Canvas *c = tge_canvas_create(4, 2);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite e1 = TGE_SPRITE(2, 1, "..", NULL), f1 = TGE_SPRITE(2, 1, "##", NULL), b1 = TGE_SPRITE(2, 1, "::", NULL);
    TGE_Sprite s1 = TGE_SPRITE(2, 1, "()", NULL);
    TGE_Sprite e2 = TGE_SPRITE(2, 1, "xx", NULL), f2 = TGE_SPRITE(2, 1, "oo", NULL), b2 = TGE_SPRITE(2, 1, "##", NULL);
    TGE_Sprite s2 = TGE_SPRITE(2, 1, "<>", NULL);
    TGE_GridTheme blocks = { &e1, &f1, &b1, &s1 };
    TGE_GridTheme dots = { &e2, &f2, &b2, &s2 };
    g.theme = &blocks;
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '#') && cell_is(c, 1, 0, '#'),
               "blocks theme fill");
    g.theme = &dots;
    tge_grid_set_cell(&g, 1, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 0, 'o') && cell_is(c, 3, 0, 'o'),
               "dots theme fill after swap");
    tge_canvas_destroy(c);
}

TGE_TEST(theme_swap_keeps_grid_geometry)
{
    TGE_Canvas *c = tge_canvas_create(4, 2);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite e1 = TGE_SPRITE(2, 1, "..", NULL), f1 = TGE_SPRITE(2, 1, "##", NULL), b1 = TGE_SPRITE(2, 1, "::", NULL);
    TGE_Sprite s1 = TGE_SPRITE(2, 1, "()", NULL);
    TGE_Sprite e2 = TGE_SPRITE(2, 1, "xx", NULL), f2 = TGE_SPRITE(2, 1, "oo", NULL), b2 = TGE_SPRITE(2, 1, "##", NULL);
    TGE_Sprite s2 = TGE_SPRITE(2, 1, "<>", NULL);
    TGE_GridTheme blocks = { &e1, &f1, &b1, &s1 };
    TGE_GridTheme dots = { &e2, &f2, &b2, &s2 };
    g.theme = &blocks;
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    g.theme = &dots;
    tge_grid_set_cell(&g, 0, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '#') && cell_is(c, 1, 0, '#'),
               "first row blocks");
    TGE_ASSERT(cell_is(c, 0, 1, 'o') && cell_is(c, 1, 1, 'o'),
               "second row dots");
    TGE_ASSERT(tge_grid_width(&g) == 2, "grid size unaffected by theme");
    tge_canvas_destroy(c);
}

TGE_TEST(builtin_blocks_theme)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, 0x2588) && cell_is(c, 1, 0, 0x2588),
               "fill is full block");
    tge_grid_put_tile(&g, 1, 0, TGE_TILE_EMPTY, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 0, ' ') && cell_is(c, 3, 0, ' '),
               "empty is spaces");
    tge_grid_put_tile(&g, 2, 0, TGE_TILE_SELECTION, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 4, 0, 0x2593) && cell_is(c, 5, 0, 0x2593),
               "selection is dark shade");
    tge_canvas_destroy(c);
}

TGE_TEST(builtin_ascii_theme)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    g.theme = &TGE_GRID_THEME_ASCII;
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '[') && cell_is(c, 1, 0, ']'),
               "ascii fill is []");
    tge_grid_draw_border(&g, 1, 0, 2, 2, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 0, '#') && cell_is(c, 3, 0, '#'),
               "ascii border is ##");
    TGE_ASSERT(cell_is(c, 4, 0, '#') && cell_is(c, 5, 0, '#'),
               "ascii border corner");
    tge_canvas_destroy(c);
}

TGE_TEST(builtin_dots_theme)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    g.theme = &TGE_GRID_THEME_DOTS;
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, 'o') && cell_is(c, 1, 0, 'o'),
               "dots fill is oo");
    tge_grid_put_tile(&g, 1, 0, TGE_TILE_EMPTY, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 0, 0xB7) && cell_is(c, 3, 0, ' '),
               "dots empty is middle dot + space");
    tge_grid_put_tile(&g, 2, 0, TGE_TILE_SELECTION, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 4, 0, '(') && cell_is(c, 5, 0, ')'),
               "dots selection is ()");
    tge_canvas_destroy(c);
}

TGE_TEST(fill_uses_given_tile)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_fill(&g, 0, 0, 2, 2, TGE_TILE_EMPTY, TGE_COLOR_GREEN,
                  TGE_COLOR_BLACK);
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            TGE_ASSERT(cell_is(c, x, y, ' '), "empty tile filled");
    TGE_ASSERT(cell_at(c, 0, 2)->ch == 0, "below rect untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(fill_with_fill_tile)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_fill(&g, 0, 0, 2, 2, TGE_TILE_DEFAULT, TGE_COLOR_GREEN,
                  TGE_COLOR_BLACK);
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            TGE_ASSERT(cell_is(c, x, y, 0x2588), "fill tile drawn");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_border_only_uses_border_tile)
{
    TGE_Canvas *c = tge_canvas_create(10, 6);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_GridTheme t = theme_of("..", "##", "::", "()");
    g.theme = &t;
    tge_grid_draw_border(&g, 1, 1, 3, 3, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 1, ':'), "top-left corner");
    TGE_ASSERT(cell_is(c, 6, 1, ':'), "top-right corner");
    TGE_ASSERT(cell_is(c, 2, 3, ':'), "bottom-left corner");
    TGE_ASSERT(cell_is(c, 6, 3, ':'), "bottom-right corner");
    TGE_ASSERT(cell_is(c, 3, 1, ':'), "top edge");
    TGE_ASSERT(cell_is(c, 3, 3, ':'), "bottom edge");
    TGE_ASSERT(cell_is(c, 2, 2, ':') && cell_is(c, 3, 2, ':'),
               "left edge both columns");
    TGE_ASSERT(cell_is(c, 6, 2, ':') && cell_is(c, 7, 2, ':'),
               "right edge both columns");
    TGE_ASSERT(cell_at(c, 4, 2)->ch == 0 && cell_at(c, 5, 2)->ch == 0,
               "interior empty");
    TGE_ASSERT(cell_at(c, 8, 2)->ch == 0, "outside empty");
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0, "outside corner empty");
    tge_canvas_destroy(c);
}

TGE_TEST(clear_uses_empty_tile)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_GridTheme t = theme_of("..", "##", "::", "()");
    g.theme = &t;
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_clear(&g, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            TGE_ASSERT(cell_is(c, x, y, '.'), "clear fills with empty tile");
    tge_canvas_destroy(c);
}

TGE_TEST(erase_uses_empty_tile)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_GridTheme t = theme_of("..", "##", "::", "()");
    g.theme = &t;
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_erase(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '.') && cell_is(c, 1, 0, '.'),
               "erased cell is empty tile");
    TGE_ASSERT(cell_at(c, 2, 0)->ch == 0, "neighbor untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(theme_without_selection_skips_selection_draw)
{
    TGE_Canvas *c = tge_canvas_create(4, 2);
    TGE_Grid g;
    tge_grid_init(&g, c);
    TGE_Sprite e = TGE_SPRITE(2, 1, "..", NULL);
    TGE_Sprite f = TGE_SPRITE(2, 1, "##", NULL);
    TGE_Sprite b = TGE_SPRITE(2, 1, "::", NULL);
    TGE_GridTheme t = { &e, &f, &b, NULL };
    g.theme = &t;
    tge_grid_put_tile(&g, 0, 0, TGE_TILE_SELECTION, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0, "selection skipped");
    tge_grid_put_tile(&g, 0, 0, TGE_TILE_DEFAULT, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '#'), "fill still works");
    tge_canvas_destroy(c);
}

TGE_TEST(put_sprite_2x1_two_different_glyphs)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "()", NULL);
    tge_grid_put(&g, 1, 1, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 1, '('), "left column gets '('");
    TGE_ASSERT(cell_is(c, 3, 1, ')'), "right column gets ')'");
    tge_canvas_destroy(c);
}

TGE_TEST(put_sprite_2x2_four_glyphs)
{
    TGE_Canvas *c = tge_canvas_create(8, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    TGE_Sprite spr = TGE_SPRITE(2, 2, "abcd", NULL);
    tge_grid_put(&g, 2, 2, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 2, 2, 'a'), "row 0, col 0");
    TGE_ASSERT(cell_is(c, 3, 2, 'b'), "row 0, col 1");
    TGE_ASSERT(cell_is(c, 2, 3, 'c'), "row 1, col 0");
    TGE_ASSERT(cell_is(c, 3, 3, 'd'), "row 1, col 1");
    tge_canvas_destroy(c);
}

TGE_TEST(put_sprite_ignores_cell_size)
{
    TGE_Canvas *c = tge_canvas_create(8, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 4, 2);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "ab", NULL);
    tge_grid_put(&g, 0, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, 'a') && cell_is(c, 1, 0, 'b'),
               "sprite drawn at natural 2x1, not stretched to 4x2");
    TGE_ASSERT(cell_at(c, 2, 0)->ch == 0, "cell width not stretched");
    TGE_ASSERT(cell_at(c, 0, 1)->ch == 0, "cell height not stretched");
    tge_canvas_destroy(c);
}

TGE_TEST(put_sprite_too_few_glyphs_skipped)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite wrong = TGE_SPRITE(2, 1, "x", NULL);
    tge_grid_put(&g, 0, 0, &wrong, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0, "too few glyphs not drawn");
    TGE_ASSERT(cell_at(c, 1, 0)->ch == 0, "too few glyphs not drawn");
    TGE_Sprite extra = TGE_SPRITE(2, 1, "xyz", NULL);
    tge_grid_put(&g, 0, 0, &extra, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0, "extra glyphs not drawn");
    tge_canvas_destroy(c);
}

TGE_TEST(put_sprite_clips_at_edge)
{
    TGE_Canvas *c = tge_canvas_create(7, 5);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "()", NULL);
    tge_grid_put(&g, 3, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 6, 0, '('), "first glyph in bounds");
    TGE_ASSERT(cell_at(c, 5, 0)->ch == 0, "out-of-bounds second glyph skipped");
    tge_canvas_destroy(c);
}

TGE_TEST(put_sprite_utf8_glyph_decoded)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x93", NULL); /* U+2588, U+2593 */
    tge_grid_put(&g, 0, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0x2588, "block glyph decoded");
    TGE_ASSERT(cell_at(c, 1, 0)->ch == 0x2593, "shade glyph decoded");
    tge_canvas_destroy(c);
}

TGE_TEST(put_tile_invalid_tile_skipped)
{
    TGE_Canvas *c = tge_canvas_create(4, 2);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_put_tile(&g, 0, 0, (TGE_GridTile)99, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0, "invalid tile skipped");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_frame_box_drawing)
{
    TGE_Canvas *c = tge_canvas_create(20, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 2, 1);
    tge_grid_draw_frame(&g, 1, 1, 4, 3, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 4, 2)->ch == 0x250C, "top-left corner");
    TGE_ASSERT(cell_at(c, 11, 2)->ch == 0x2510, "top-right corner");
    TGE_ASSERT(cell_at(c, 4, 4)->ch == 0x2514, "bottom-left corner");
    TGE_ASSERT(cell_at(c, 11, 4)->ch == 0x2518, "bottom-right corner");
    for (int x = 5; x <= 10; x++) {
        TGE_ASSERT(cell_at(c, x, 2)->ch == 0x2500, "top edge horizontal");
        TGE_ASSERT(cell_at(c, x, 4)->ch == 0x2500, "bottom edge horizontal");
    }
    TGE_ASSERT(cell_at(c, 4, 3)->ch == 0x2502, "left edge vertical");
    TGE_ASSERT(cell_at(c, 11, 3)->ch == 0x2502, "right edge vertical");
    TGE_ASSERT(cell_at(c, 5, 3)->ch == 0 && cell_at(c, 6, 3)->ch == 0,
               "interior empty");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_line_horizontal_and_diagonal)
{
    TGE_Canvas *c = tge_canvas_create(8, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_draw_line(&g, 0, 0, 3, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    for (int x = 0; x <= 3; x++)
        TGE_ASSERT(cell_is(c, x, 0, 0x2588), "horizontal line cell");
    TGE_ASSERT(cell_at(c, 4, 0)->ch == 0, "line stops at endpoint");
    tge_grid_draw_line(&g, 0, 4, 2, 6, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 4, 0x2588) && cell_is(c, 1, 5, 0x2588) &&
               cell_is(c, 2, 6, 0x2588), "diagonal cells");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_circle_outline_radius_one)
{
    TGE_Canvas *c = tge_canvas_create(8, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_draw_circle(&g, 3, 3, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    int count = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (cell_at(c, x, y)->ch == 0x2588)
                count++;
    TGE_ASSERT(count == 8, "ring of 8 cells around center");
    TGE_ASSERT(cell_at(c, 3, 3)->ch == 0, "center empty");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_text_unscaled_at_scaled_position)
{
    TGE_Canvas *c = tge_canvas_create(20, 6);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 2, 1);
    tge_grid_draw_text(&g, 1, 1, "AB", TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 4, 2, 'A'), "first glyph at 2+1*2,1+1*1");
    TGE_ASSERT(cell_is(c, 5, 2, 'B'), "second glyph at natural width");
    tge_canvas_destroy(c);
}

TGE_TEST(attach_repaints_canvas_keeps_geometry)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Canvas *c2 = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 1, 1);
    g.theme = &TGE_GRID_THEME_DOTS;

    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 1, 1, 'o'), "draw goes to first canvas");
    TGE_ASSERT(cell_at(c2, 1, 1)->ch == 0, "second canvas untouched");

    tge_grid_attach(&g, c2);
    TGE_ASSERT(g.cell_w == 2 && g.cell_h == 1, "cell size persists");
    TGE_ASSERT(g.ox == 1 && g.oy == 1, "origin persists");
    TGE_ASSERT(g.theme == &TGE_GRID_THEME_DOTS, "theme persists");
    TGE_ASSERT(cell_at(c, 1, 1)->ch != 0, "first canvas keeps old pixels");

    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c2, 1, 1, 'o'), "draw now goes to second canvas");
    tge_canvas_destroy(c);
    tge_canvas_destroy(c2);
}

TGE_TEST(null_safety)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "()", NULL);

    tge_grid_init(NULL, NULL);
    tge_grid_set_origin(NULL, 1, 1);
    tge_grid_set_cell_size(NULL, 1, 1);
    tge_grid_square_pixels(NULL);
    tge_grid_put_tile(NULL, 0, 0, TGE_TILE_DEFAULT, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    tge_grid_put(NULL, 0, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_put(&g, 0, 0, NULL, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_fill(NULL, 0, 0, 1, 1, TGE_TILE_DEFAULT, TGE_COLOR_GREEN,
                  TGE_COLOR_BLACK);
    tge_grid_set_cell(NULL, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_erase(NULL, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_draw_border(NULL, 0, 0, 1, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_clear(NULL, TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_grid_draw_frame(NULL, 0, 0, 1, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_draw_line(NULL, 0, 0, 1, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_draw_circle(NULL, 0, 0, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_draw_text(NULL, 0, 0, "x", TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    tge_grid_attach(NULL, NULL);
    tge_grid_attach(&g, NULL);
    TGE_ASSERT(tge_grid_width(NULL) == 0, "NULL width");
    TGE_ASSERT(tge_grid_height(NULL) == 0, "NULL height");
    tge_canvas_destroy(c);
}

TGE_TEST(sprite_fallback_ascii_when_off)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");

    tge_unicode_set_mode(TGE_UNICODE_OFF);
    tge_grid_put(&g, 0, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '#') && cell_is(c, 1, 0, '#'),
               "ascii fallback drawn in OFF mode");

    tge_unicode_set_mode(TGE_UNICODE_ON);
    tge_grid_put(&g, 0, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0x2588 && cell_at(c, 1, 0)->ch == 0x2588,
               "primary glyph drawn in ON mode");

    tge_unicode_set_mode(TGE_UNICODE_AUTO);
    tge_canvas_destroy(c);
}

TGE_TEST(sprite_without_fallback_keeps_primary)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    TGE_Sprite spr = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88", NULL);

    tge_unicode_set_mode(TGE_UNICODE_OFF);
    tge_grid_put(&g, 0, 0, &spr, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0x2588 && cell_at(c, 1, 0)->ch == 0x2588,
               "no fallback keeps primary even in OFF mode");

    tge_unicode_set_mode(TGE_UNICODE_AUTO);
    tge_canvas_destroy(c);
}

TGE_TEST(tile_fallback_resolves_theme)
{
    TGE_Canvas *c = tge_canvas_create(4, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    g.theme = &TGE_GRID_THEME_BLOCKS;

    tge_unicode_set_mode(TGE_UNICODE_OFF);
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, '#') && cell_is(c, 1, 0, '#'),
               "default tile ascii fallback");
    tge_grid_erase(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 0, ' ') && cell_is(c, 1, 0, ' '),
               "empty tile ascii fallback");
    tge_grid_draw_border(&g, 0, 1, 2, 1, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 1, '#') && cell_is(c, 1, 1, '#'),
               "border tile ascii fallback");
    tge_grid_put_tile(&g, 0, 2, TGE_TILE_SELECTION, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    TGE_ASSERT(cell_is(c, 0, 2, '.') && cell_is(c, 1, 2, '.'),
               "selection tile ascii fallback");

    tge_unicode_set_mode(TGE_UNICODE_ON);
    tge_grid_set_cell(&g, 0, 0, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 0, 0)->ch == 0x2588, "primary in ON mode");

    tge_unicode_set_mode(TGE_UNICODE_AUTO);
    tge_canvas_destroy(c);
}

TGE_TEST(draw_frame_ascii_when_off)
{
    TGE_Canvas *c = tge_canvas_create(20, 8);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_grid_set_origin(&g, 2, 1);

    tge_unicode_set_mode(TGE_UNICODE_OFF);
    tge_grid_draw_frame(&g, 1, 1, 4, 3, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_ASSERT(cell_at(c, 4, 2)->ch == '+', "top-left corner");
    TGE_ASSERT(cell_at(c, 11, 2)->ch == '+', "top-right corner");
    TGE_ASSERT(cell_at(c, 4, 4)->ch == '+', "bottom-left corner");
    TGE_ASSERT(cell_at(c, 11, 4)->ch == '+', "bottom-right corner");
    for (int x = 5; x <= 10; x++)
        TGE_ASSERT(cell_at(c, x, 2)->ch == '-' && cell_at(c, x, 4)->ch == '-',
                   "horizontal edges");
    TGE_ASSERT(cell_at(c, 4, 3)->ch == '|' && cell_at(c, 11, 3)->ch == '|',
               "vertical edges");

    tge_unicode_set_mode(TGE_UNICODE_AUTO);
    tge_canvas_destroy(c);
}

int main(void)
{
    tge_unicode_set_mode(TGE_UNICODE_ON);
    test_init_defaults();
    test_square_pixels_helper();
    test_origin_and_cell_size_change_size();
    test_size_for_matches_width_height();
    test_size_for_no_canvas_needed();
    test_size_for_null_safety();
    test_set_cell_fills_block_with_fill_tile();
    test_set_cell_1x1_writes_one_physical_cell();
    test_set_cell_clips_at_canvas_edge();
    test_set_cell_negative_clips();
    test_put_tile_resolves_theme_sprites();
    test_theme_swap_changes_look_without_touching_draw_calls();
    test_theme_swap_keeps_grid_geometry();
    test_builtin_blocks_theme();
    test_builtin_ascii_theme();
    test_builtin_dots_theme();
    test_fill_uses_given_tile();
    test_fill_with_fill_tile();
    test_draw_border_only_uses_border_tile();
    test_clear_uses_empty_tile();
    test_erase_uses_empty_tile();
    test_theme_without_selection_skips_selection_draw();
    test_put_sprite_2x1_two_different_glyphs();
    test_put_sprite_2x2_four_glyphs();
    test_put_sprite_ignores_cell_size();
    test_put_sprite_too_few_glyphs_skipped();
    test_put_sprite_clips_at_edge();
    test_put_sprite_utf8_glyph_decoded();
    test_put_tile_invalid_tile_skipped();
    test_draw_frame_box_drawing();
    test_draw_line_horizontal_and_diagonal();
    test_draw_circle_outline_radius_one();
    test_draw_text_unscaled_at_scaled_position();
    test_attach_repaints_canvas_keeps_geometry();
    test_null_safety();
    test_sprite_fallback_ascii_when_off();
    test_sprite_without_fallback_keeps_primary();
    test_tile_fallback_resolves_theme();
    test_draw_frame_ascii_when_off();
    return tge_test_report();
}
