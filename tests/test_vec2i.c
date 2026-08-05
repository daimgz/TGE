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

int main(void)
{
    test_construct_and_zero();
    test_add_sub_scale();
    test_eq();
    return tge_test_report();
}
