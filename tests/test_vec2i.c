#include "tge-extra/vec2i.h"

#include "tge_test.h"

TGE_TEST(construct_and_zero)
{
    TGE_Vec2i v = tge_vec2i(3, -4);
    TGE_ASSERT(v.x == 3 && v.y == -4, "fields set");
    TGE_Vec2i z = tge_vec2i_zero();
    TGE_ASSERT(z.x == 0 && z.y == 0, "zero is (0,0)");
}
TGE_TEST(add_sub_scale)
{
    TGE_Vec2i a = tge_vec2i(2, 3);
    TGE_Vec2i b = tge_vec2i(1, -1);
    TGE_Vec2i s = tge_vec2i_add(a, b);
    TGE_ASSERT(s.x == 3 && s.y == 2, "add");
    TGE_Vec2i d = tge_vec2i_sub(a, b);
    TGE_ASSERT(d.x == 1 && d.y == 4, "sub");
    TGE_Vec2i m = tge_vec2i_scale(a, 3);
    TGE_ASSERT(m.x == 6 && m.y == 9, "scale");
    TGE_Vec2i z = tge_vec2i_scale(a, 0);
    TGE_ASSERT(z.x == 0 && z.y == 0, "scale by zero");
    TGE_Vec2i neg = tge_vec2i_scale(a, -1);
    TGE_ASSERT(neg.x == -2 && neg.y == -3, "scale negative");
}

TGE_TEST(eq)
{
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i(1, 2), tge_vec2i(1, 2)), "equal");
    TGE_ASSERT(!tge_vec2i_eq(tge_vec2i(1, 2), tge_vec2i(2, 2)), "x differs");
    TGE_ASSERT(!tge_vec2i_eq(tge_vec2i(1, 2), tge_vec2i(1, 3)), "y differs");
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_zero(), tge_vec2i(0, 0)), "zero equals");
}

TGE_TEST(clamp_rect_bounds)
{
    TGE_Rect r = tge_rect(5, 10, 4, 3);
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(0, 0), r),
                            tge_vec2i(5, 10)),
               "below-left clamps to origin");
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(99, 99), r),
                            tge_vec2i(8, 12)),
               "beyond-right clamps to r.x+w-1/r.y+h-1");
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(6, 11), r),
                            tge_vec2i(6, 11)),
               "inside stays");
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(3, 11), r),
                            tge_vec2i(5, 11)),
               "x clamped alone");
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(6, 20), r),
                            tge_vec2i(6, 12)),
               "y clamped alone");
}

TGE_TEST(clamp_rect_degenerate)
{
    TGE_Rect r = tge_rect(2, 3, 0, 4);
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(9, 9), r),
                            tge_vec2i(2, 3)),
               "zero width yields top-left");
    TGE_Rect r2 = tge_rect(2, 3, 4, -1);
    TGE_ASSERT(tge_vec2i_eq(tge_vec2i_clamp_rect(tge_vec2i(9, 9), r2),
                            tge_vec2i(2, 3)),
               "zero height yields top-left");
}

TGE_TEST(random_point_stays_in_rect)
{
    TGE_Rect r = tge_rect(3, 4, 5, 6);
    for (int i = 0; i < 200; i++) {
        TGE_Vec2i p = tge_rect_random_point(r);
        TGE_ASSERT(p.x >= 3 && p.x <= 7 && p.y >= 4 && p.y <= 9,
                   "point within inclusive bounds");
    }
    TGE_Rect d = tge_rect(1, 2, 0, 0);
    TGE_ASSERT(tge_vec2i_eq(tge_rect_random_point(d), tge_vec2i(1, 2)),
               "degenerate rect yields top-left");
}

TGE_TEST(translate_point_adds_origin)
{
    TGE_Rect r = tge_rect(10, 20, 5, 5);
    TGE_ASSERT(tge_vec2i_eq(tge_rect_translate_point(r, tge_vec2i(0, 0)),
                            tge_vec2i(10, 20)),
               "origin maps local zero");
    TGE_ASSERT(tge_vec2i_eq(tge_rect_translate_point(r, tge_vec2i(3, -2)),
                            tge_vec2i(13, 18)),
               "local offsets added");
    TGE_Rect z = tge_rect(0, 0, 4, 4);
    TGE_ASSERT(tge_vec2i_eq(tge_rect_translate_point(z, tge_vec2i(2, 2)),
                            tge_vec2i(2, 2)),
               "identity when origin is zero");
}

int main(void)
{
    test_construct_and_zero();
    test_add_sub_scale();
    test_eq();
    test_clamp_rect_bounds();
    test_clamp_rect_degenerate();
    test_random_point_stays_in_rect();
    test_translate_point_adds_origin();
    return tge_test_report();
}
