#include "tge/tge_math.h"
#include "tge_test.h"

#include <math.h>

TGE_TEST(vec2_basic)
{
    TGE_Vector2 v = tge_vec2(3.0f, 4.0f);
    TGE_ASSERT(fabsf(v.x - 3.0f) < 1e-6f, "x");
    TGE_ASSERT(fabsf(v.y - 4.0f) < 1e-6f, "y");
}

TGE_TEST(vec2_ops)
{
    TGE_Vector2 a = tge_vec2(1.0f, 2.0f);
    TGE_Vector2 b = tge_vec2(3.0f, 4.0f);
    TGE_Vector2 s = tge_vec2_add(a, b);
    TGE_ASSERT(fabsf(s.x - 4.0f) < 1e-6f && fabsf(s.y - 6.0f) < 1e-6f,
               "add");
    TGE_Vector2 d = tge_vec2_sub(a, b);
    TGE_ASSERT(fabsf(d.x - -2.0f) < 1e-6f && fabsf(d.y - -2.0f) < 1e-6f,
               "sub");
    TGE_Vector2 m = tge_vec2_scale(a, 2.0f);
    TGE_ASSERT(fabsf(m.x - 2.0f) < 1e-6f && fabsf(m.y - 4.0f) < 1e-6f,
               "scale");
}

TGE_TEST(vec2_length_distance)
{
    TGE_Vector2 v = tge_vec2(3.0f, 4.0f);
    TGE_ASSERT(fabsf(tge_vec2_length(v) - 5.0f) < 1e-6f, "length");
    TGE_Vector2 a = tge_vec2(0.0f, 0.0f);
    TGE_Vector2 b = tge_vec2(0.0f, 3.0f);
    TGE_ASSERT(fabsf(tge_vec2_distance(a, b) - 3.0f) < 1e-6f, "distance");
}

TGE_TEST(rect_contains)
{
    TGE_Rect r = tge_rect(2, 3, 4, 5);
    TGE_ASSERT(tge_rect_contains(r, 2, 3), "top-left included");
    TGE_ASSERT(tge_rect_contains(r, 5, 7), "bottom-right included");
    TGE_ASSERT(!tge_rect_contains(r, 1, 3), "left excluded");
    TGE_ASSERT(!tge_rect_contains(r, 6, 3), "right excluded (half-open)");
    TGE_ASSERT(!tge_rect_contains(r, 2, 8), "bottom excluded");
    TGE_ASSERT(!tge_rect_contains(r, 2, 2), "top excluded");
}

TGE_TEST(rect_intersects)
{
    TGE_Rect a = tge_rect(0, 0, 4, 4);
    TGE_Rect b = tge_rect(3, 3, 4, 4);
    TGE_ASSERT(tge_rect_intersects(a, b), "overlapping");
    TGE_Rect c = tge_rect(4, 0, 2, 2);
    TGE_ASSERT(!tge_rect_intersects(a, c), "touching edges no intersect");
    TGE_Rect d = tge_rect(10, 10, 2, 2);
    TGE_ASSERT(!tge_rect_intersects(a, d), "separated");
    TGE_Rect e = tge_rect(0, 0, 0, 4);
    TGE_ASSERT(!tge_rect_intersects(a, e), "zero width empty");
}

TGE_TEST(circle_intersects)
{
    TGE_Rect rect = tge_rect(10, 10, 10, 10);
    TGE_Vector2 center = tge_vec2(15.0f, 15.0f);
    TGE_ASSERT(tge_circle_intersects(center, 1.0f, rect), "center inside");
    TGE_ASSERT(tge_circle_intersects(tge_vec2(15.0f, 22.0f), 3.0f, rect),
               "near edge");
    TGE_ASSERT(!tge_circle_intersects(tge_vec2(15.0f, 25.0f), 1.0f, rect),
               "far away");
}

TGE_TEST(vec2_normalize)
{
    TGE_Vector2 n = tge_vec2_normalize(tge_vec2(3.0f, 4.0f));
    TGE_ASSERT(fabsf(n.x - 0.6f) < 1e-6f && fabsf(n.y - 0.8f) < 1e-6f,
               "unit length direction");
    TGE_Vector2 z = tge_vec2_normalize(tge_vec2(0.0f, 0.0f));
    TGE_ASSERT(z.x == 0.0f && z.y == 0.0f, "zero vector stays zero");
    TGE_Vector2 u = tge_vec2_normalize(tge_vec2(1.0f, 0.0f));
    TGE_ASSERT(fabsf(tge_vec2_length(u) - 1.0f) < 1e-6f, "length 1");
}

TGE_TEST(segments_intersect)
{
    TGE_Vector2 a = tge_vec2(0, 0), b = tge_vec2(10, 10);
    TGE_Vector2 c = tge_vec2(0, 10), d = tge_vec2(10, 0);
    TGE_ASSERT(tge_segments_intersect(a, b, c, d), "crossing segments");
    TGE_ASSERT(tge_segments_intersect(a, b, tge_vec2(5, 0), tge_vec2(5, 5)),
               "endpoint touch counts as intersect");
    TGE_ASSERT(tge_segments_intersect(a, b, a, c),
               "shared origin intersects");
    TGE_ASSERT(!tge_segments_intersect(a, b, tge_vec2(5, 6), tge_vec2(6, 7)),
               "non-collinear disjoint no intersect");
    TGE_ASSERT(!tge_segments_intersect(a, b, c, c),
               "degenerate point segment no intersect");
}

int main(void)
{
    test_vec2_basic();
    test_vec2_ops();
    test_vec2_length_distance();
    test_rect_contains();
    test_rect_intersects();
    test_circle_intersects();
    test_vec2_normalize();
    test_segments_intersect();
    return tge_test_report();
}
