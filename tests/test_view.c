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
    return tge_test_report();
}
