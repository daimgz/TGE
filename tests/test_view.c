#include "tge-extra/view.h"

#include "tge_test.h"

TGE_TEST(init_defaults)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    TGE_ASSERT(v.margin == 1, "default margin 1");
    TGE_ASSERT(v.min_w == 10 && v.min_h == 6, "min size stored");
    TGE_ASSERT(!v.valid, "not valid before first update");
    TGE_ASSERT(v.first, "first until first valid layout");
    TGE_ASSERT(v.area.w == 0 && v.area.h == 0, "area zero before update");
}

TGE_TEST(update_computes_inset_area)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    TGE_ASSERT(tge_view_update(&v, 12, 8) == TGE_VIEW_FIRST_VALID,
               "first valid layout");
    TGE_ASSERT(v.area.x == 1 && v.area.y == 1, "area inset by margin");
    TGE_ASSERT(v.area.w == 10 && v.area.h == 6, "area size computed");
    TGE_ASSERT(v.valid, "valid flag set");
}

TGE_TEST(custom_margin_field)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    v.margin = 3;
    TGE_ASSERT(tge_view_update(&v, 16, 12) == TGE_VIEW_FIRST_VALID,
               "first valid layout");
    TGE_ASSERT(v.area.x == 3 && v.area.y == 3, "area inset by custom margin");
    TGE_ASSERT(v.area.w == 10 && v.area.h == 6, "custom margin area size");
}

TGE_TEST(too_small_is_invalid)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    TGE_ASSERT(tge_view_update(&v, 11, 8) == TGE_VIEW_INVALID, "11 wide invalid");
    TGE_ASSERT(!v.valid, "valid false when too small");
    TGE_ASSERT(v.first, "first kept while invalid");
}

TGE_TEST(too_small_clamps_area_to_zero)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    TGE_ASSERT(tge_view_update(&v, 1, 1) == TGE_VIEW_INVALID,
               "surface smaller than margins");
    TGE_ASSERT(v.area.w == 0 && v.area.h == 0, "area clamped to zero");
}

TGE_TEST(first_consumed_on_first_valid_layout)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    TGE_ASSERT(tge_view_update(&v, 11, 8) == TGE_VIEW_INVALID, "too small");
    TGE_ASSERT(v.first, "first survives invalid layout");
    TGE_ASSERT(tge_view_update(&v, 12, 8) == TGE_VIEW_FIRST_VALID,
               "grows to first valid");
    TGE_ASSERT(!v.first, "first consumed by the first valid layout");
    TGE_ASSERT(tge_view_update(&v, 60, 24) == TGE_VIEW_RESIZED,
               "later updates resize");
    TGE_ASSERT(!v.first, "first stays consumed");
}

TGE_TEST(invalid_does_not_consume_first)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    TGE_ASSERT(tge_view_update(&v, 5, 5) == TGE_VIEW_INVALID, "tiny start");
    TGE_ASSERT(v.first, "first still pending after tiny start");
    TGE_ASSERT(tge_view_update(&v, 40, 20) == TGE_VIEW_FIRST_VALID,
               "first layout on the grown terminal");
    TGE_ASSERT(!v.first, "first done");
}

TGE_TEST(resized_returns_for_later_valid_updates)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 40, 20);
    TGE_ASSERT(v.area.w == 38 && v.area.h == 18, "40x20 -> 38x18");
    TGE_ASSERT(tge_view_update(&v, 20, 10) == TGE_VIEW_RESIZED,
               "second valid layout");
    TGE_ASSERT(v.area.w == 18 && v.area.h == 8, "20x10 -> 18x8");
}

TGE_TEST(valid_again_after_invalid_is_resized)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 40, 20); /* first valid */
    TGE_ASSERT(tge_view_update(&v, 11, 8) == TGE_VIEW_INVALID, "shrunk");
    TGE_ASSERT(tge_view_update(&v, 40, 20) == TGE_VIEW_RESIZED,
               "re-grown is not first again");
}

TGE_TEST(null_safety)
{
    tge_view_init(NULL, 10, 6);
    TGE_ASSERT(tge_view_update(NULL, 40, 20) == TGE_VIEW_INVALID,
               "NULL update is invalid");
}

TGE_TEST(translate_maps_local_to_outer)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 12, 8);
    TGE_ASSERT(tge_vec2i_eq(tge_view_translate(&v, tge_vec2i(0, 0)),
                            tge_vec2i(1, 1)),
               "origin offset by margin");
    TGE_ASSERT(tge_vec2i_eq(tge_view_translate(&v, tge_vec2i(9, 5)),
                            tge_vec2i(10, 6)),
               "far corner maps to area corner");
}

TGE_TEST(translate_uses_custom_margin)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    v.margin = 3;
    tge_view_update(&v, 16, 12);
    TGE_ASSERT(tge_vec2i_eq(tge_view_translate(&v, tge_vec2i(0, 0)),
                            tge_vec2i(3, 3)),
               "custom margin offset");
    TGE_ASSERT(tge_vec2i_eq(tge_view_translate(&v, tge_vec2i(9, 5)),
                            tge_vec2i(12, 8)),
               "custom margin far corner");
}

TGE_TEST(contains_checks_interior)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 12, 8);
    TGE_ASSERT(tge_view_contains(&v, tge_vec2i(0, 0)), "top-left inside");
    TGE_ASSERT(tge_view_contains(&v, tge_vec2i(9, 5)), "bottom-right inside");
    TGE_ASSERT(!tge_view_contains(&v, tge_vec2i(10, 5)), "x out of bounds");
    TGE_ASSERT(!tge_view_contains(&v, tge_vec2i(9, 6)), "y out of bounds");
    TGE_ASSERT(!tge_view_contains(&v, tge_vec2i(-1, 0)), "negative x");
    TGE_ASSERT(!tge_view_contains(&v, tge_vec2i(0, -1)), "negative y");
}

TGE_TEST(contains_zero_area_view_false)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 1, 1); /* area clamped to 0x0 */
    TGE_ASSERT(!tge_view_contains(&v, tge_vec2i(0, 0)),
               "nothing inside a degenerate area");
}

TGE_TEST(random_point_stays_inside)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 12, 8);
    for (int i = 0; i < 1000; i++)
        TGE_ASSERT(tge_view_contains(&v, tge_view_random_point(&v)),
                   "random point always inside the interior");
}

TGE_TEST(random_point_degenerate_yields_origin)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 1, 1); /* area clamped to 0x0 */
    TGE_ASSERT(tge_vec2i_eq(tge_view_random_point(&v), tge_vec2i(0, 0)),
               "degenerate area yields (0, 0)");
}

TGE_TEST(local_bounds_zero_origin)
{
    TGE_View v;
    tge_view_init(&v, 10, 6);
    tge_view_update(&v, 12, 8);
    TGE_Rect b = tge_view_local_bounds(&v);
    TGE_ASSERT(b.x == 0 && b.y == 0, "local origin is 0");
    TGE_ASSERT(b.w == 10 && b.h == 6, "local size equals area size");
}

TGE_TEST(local_ops_null_safety)
{
    TGE_ASSERT(tge_vec2i_eq(tge_view_translate(NULL, tge_vec2i(1, 2)),
                            tge_vec2i(0, 0)),
               "NULL translate is zero");
    TGE_ASSERT(!tge_view_contains(NULL, tge_vec2i(0, 0)),
               "NULL contains is false");
    TGE_ASSERT(tge_vec2i_eq(tge_view_random_point(NULL), tge_vec2i(0, 0)),
               "NULL random is zero");
    TGE_Rect b = tge_view_local_bounds(NULL);
    TGE_ASSERT(b.w == 0 && b.h == 0, "NULL bounds are zero");
}

int main(void)
{
    test_init_defaults();
    test_update_computes_inset_area();
    test_custom_margin_field();
    test_too_small_is_invalid();
    test_too_small_clamps_area_to_zero();
    test_first_consumed_on_first_valid_layout();
    test_invalid_does_not_consume_first();
    test_resized_returns_for_later_valid_updates();
    test_valid_again_after_invalid_is_resized();
    test_null_safety();
    test_translate_maps_local_to_outer();
    test_translate_uses_custom_margin();
    test_contains_checks_interior();
    test_contains_zero_area_view_false();
    test_random_point_stays_inside();
    test_random_point_degenerate_yields_origin();
    test_local_bounds_zero_origin();
    test_local_ops_null_safety();
    return tge_test_report();
}
