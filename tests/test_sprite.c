#include "tge-extra/sprite.h"

#include "tge/tge_canvas.h"
#include "tge/tge_unicode.h"
#include "tge_internal.h"
#include "tge_test.h"

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

static const char *const block_art[] = { "AB", "CD" };
static const char *const block_mask[] = { "11", "11" };
static const TGE_MaskSprite block = { block_art, block_mask, 2, 2 };

static const char *const ring_art[] = { "abc", "   ", "def" };
static const char *const ring_mask[] = { "101", "000", "101" };
static const TGE_MaskSprite ring = { ring_art, ring_mask, 3, 3 };

static const char *const wide_art[] = { "...." };
static const char *const wide_mask[] = { "1111" };
static const TGE_MaskSprite wide = { wide_art, wide_mask, 4, 1 };

static const char *const spaced_art[] = { "A B" };
static const char *const spaced_mask[] = { "101" };
static const TGE_MaskSprite spaced = { spaced_art, spaced_mask, 3, 1 };

static const char *const heart_art[] = { "\xe2\x96\x88\xe2\x96\x96" }; /* "█▖" */
static const char *const heart_mask[] = { "11" };
static const TGE_MaskSprite heart = { heart_art, heart_mask, 2, 1 };

TGE_TEST(solid_matches_mask)
{
    TGE_ASSERT(tge_sprite_solid(&block, 0, 0), "solid cell");
    TGE_ASSERT(tge_sprite_solid(&block, 1, 1), "solid cell");
    TGE_ASSERT(!tge_sprite_solid(&ring, 1, 1), "mask hole is empty");
    TGE_ASSERT(!tge_sprite_solid(&ring, 1, 0), "empty row cell");
}

TGE_TEST(solid_out_of_bounds_is_false)
{
    TGE_ASSERT(!tge_sprite_solid(&block, -1, 0), "row above");
    TGE_ASSERT(!tge_sprite_solid(&block, 0, -1), "col before");
    TGE_ASSERT(!tge_sprite_solid(&block, 2, 0), "row below");
    TGE_ASSERT(!tge_sprite_solid(&block, 0, 2), "col after");
    TGE_ASSERT(!tge_sprite_solid(&block, 100, 100), "far outside");
}

TGE_TEST(collide_disjoint_boxes)
{
    TGE_ASSERT(!tge_sprite_collide(&block, 0, 0, 2, &block, 5, 5), "far apart");
    TGE_ASSERT(!tge_sprite_collide(&block, 0, 0, 2, &block, 0, 3), "one above");
    TGE_ASSERT(!tge_sprite_collide(&block, 0, 0, 2, &block, 3, 0), "one beside");
}

TGE_TEST(collide_touching_edges_are_not_a_hit)
{
    TGE_ASSERT(!tge_sprite_collide(&block, 0, 0, 2, &block, 2, 0),
               "x edges touch, half-open");
    TGE_ASSERT(!tge_sprite_collide(&block, 0, 0, 2, &block, 0, 2),
               "y edges touch, half-open");
    TGE_ASSERT(tge_sprite_collide(&block, 0, 0, 2, &block, 1, 0),
               "one column of overlap hits");
    TGE_ASSERT(tge_sprite_collide(&block, 0, 0, 2, &block, 0, 1),
               "one row of overlap hits");
}

TGE_TEST(collide_gaps_slip_through)
{
    /* ring shifted one column right: the overlap window covers only the ring's
     * empty column, so the solid corners never meet. */
    TGE_ASSERT(!tge_sprite_collide(&ring, 0, 0, 3, &ring, 0, 1),
               "corner vs hole slips through");
    /* ring shifted two columns right: its left solid column lands on the
     * other's right solid column, so the corners meet and hit. */
    TGE_ASSERT(tge_sprite_collide(&ring, 0, 0, 3, &ring, 2, 0),
               "solid corners overlap");
}

TGE_TEST(collide_narrow_hitbox_changes_result)
{
    /* block only overlaps the far right column of the 4-wide sprite. */
    TGE_ASSERT(tge_sprite_collide(&wide, 0, 0, 4, &block, 3, 0),
               "full-width hitbox reaches it");
    TGE_ASSERT(!tge_sprite_collide(&wide, 0, 0, 2, &block, 3, 0),
               "narrow hitbox misses it");
}

TGE_TEST(draw_unicode_paints_art_and_skips_spaces)
{
    tge_unicode_set_mode(TGE_UNICODE_ON);
    TGE_Canvas *c = tge_canvas_create(10, 3);
    tge_clear(c, '.', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_sprite_draw(c, 1, 1, &spaced, TGE_COLOR_WHITE, TGE_COLOR_BLUE);

    TGE_ASSERT(cell_at(c, 1, 1)->ch == 'A', "first glyph painted");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == '.', "space left transparent");
    TGE_ASSERT(cell_at(c, 3, 1)->ch == 'B', "second glyph painted");
    TGE_ASSERT(cell_at(c, 1, 1)->fg.data.index ==
                   TGE_COLOR_WHITE.data.index, "fg color applied");
    TGE_ASSERT(cell_at(c, 1, 1)->bg.data.index ==
                   TGE_COLOR_BLUE.data.index, "bg color applied");
    tge_canvas_destroy(c);
    tge_unicode_set_mode(TGE_UNICODE_AUTO);
}

TGE_TEST(draw_unicode_advances_one_cell_per_glyph)
{
    tge_unicode_set_mode(TGE_UNICODE_ON);
    TGE_Canvas *c = tge_canvas_create(10, 3);
    tge_clear(c, '.', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_sprite_draw(c, 1, 1, &heart, TGE_COLOR_WHITE, TGE_COLOR_BLACK);

    TGE_ASSERT(cell_at(c, 1, 1)->ch == 0x2588u, "first glyph at col 1");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == 0x2596u,
               "second glyph at col 2, one cell per codepoint");
    TGE_ASSERT(cell_at(c, 3, 1)->ch == '.', "nothing past the 2 cells");
    tge_canvas_destroy(c);
    tge_unicode_set_mode(TGE_UNICODE_AUTO);
}

TGE_TEST(draw_ascii_fallback_uses_mask_blocks)
{
    tge_unicode_set_mode(TGE_UNICODE_OFF);
    TGE_Canvas *c = tge_canvas_create(10, 3);
    tge_clear(c, '.', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
    tge_sprite_draw(c, 1, 1, &spaced, TGE_COLOR_WHITE, TGE_COLOR_BLACK);

    TGE_ASSERT(cell_at(c, 1, 1)->ch == '#', "solid mask cell -> block");
    TGE_ASSERT(cell_at(c, 2, 1)->ch == '.', "empty mask cell untouched");
    TGE_ASSERT(cell_at(c, 3, 1)->ch == '#', "solid mask cell -> block");
    tge_canvas_destroy(c);
    tge_unicode_set_mode(TGE_UNICODE_AUTO);
}

int main(void)
{
    test_solid_matches_mask();
    test_solid_out_of_bounds_is_false();
    test_collide_disjoint_boxes();
    test_collide_touching_edges_are_not_a_hit();
    test_collide_gaps_slip_through();
    test_collide_narrow_hitbox_changes_result();
    test_draw_unicode_paints_art_and_skips_spaces();
    test_draw_unicode_advances_one_cell_per_glyph();
    test_draw_ascii_fallback_uses_mask_blocks();
    return tge_test_report();
}
