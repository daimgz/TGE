#include "tge-extra/direction.h"

#include "tge_test.h"

TGE_TEST(dx_dy_axes)
{
    TGE_ASSERT(tge_direction_dx(TGE_DIR_LEFT) == -1, "left dx");
    TGE_ASSERT(tge_direction_dx(TGE_DIR_RIGHT) == 1, "right dx");
    TGE_ASSERT(tge_direction_dx(TGE_DIR_UP) == 0, "up dx");
    TGE_ASSERT(tge_direction_dx(TGE_DIR_DOWN) == 0, "down dx");
    TGE_ASSERT(tge_direction_dy(TGE_DIR_UP) == -1, "up dy");
    TGE_ASSERT(tge_direction_dy(TGE_DIR_DOWN) == 1, "down dy");
    TGE_ASSERT(tge_direction_dy(TGE_DIR_LEFT) == 0, "left dy");
    TGE_ASSERT(tge_direction_dy(TGE_DIR_RIGHT) == 0, "right dy");
    TGE_ASSERT(tge_direction_dx(TGE_DIR_NONE) == 0 && tge_direction_dy(TGE_DIR_NONE) == 0,
               "none is neutral");
}

TGE_TEST(vec_steps)
{
    TGE_Vec2i u = tge_direction_vec(TGE_DIR_UP);
    TGE_ASSERT(u.x == 0 && u.y == -1, "up vec");
    TGE_Vec2i d = tge_direction_vec(TGE_DIR_DOWN);
    TGE_ASSERT(d.x == 0 && d.y == 1, "down vec");
    TGE_Vec2i l = tge_direction_vec(TGE_DIR_LEFT);
    TGE_ASSERT(l.x == -1 && l.y == 0, "left vec");
    TGE_Vec2i r = tge_direction_vec(TGE_DIR_RIGHT);
    TGE_ASSERT(r.x == 1 && r.y == 0, "right vec");
    TGE_Vec2i n = tge_direction_vec(TGE_DIR_NONE);
    TGE_ASSERT(n.x == 0 && n.y == 0, "none vec");
}

TGE_TEST(opposite_pairs)
{
    TGE_ASSERT(tge_direction_opposite(TGE_DIR_UP) == TGE_DIR_DOWN, "up<->down");
    TGE_ASSERT(tge_direction_opposite(TGE_DIR_DOWN) == TGE_DIR_UP, "down<->up");
    TGE_ASSERT(tge_direction_opposite(TGE_DIR_LEFT) == TGE_DIR_RIGHT, "left<->right");
    TGE_ASSERT(tge_direction_opposite(TGE_DIR_RIGHT) == TGE_DIR_LEFT, "right<->left");
    TGE_ASSERT(tge_direction_opposite(TGE_DIR_NONE) == TGE_DIR_NONE, "none");
}

TGE_TEST(opposite_composes_with_vec)
{
    for (int d = TGE_DIR_UP; d <= TGE_DIR_RIGHT; d++) {
        TGE_Vec2i a = tge_direction_vec((TGE_Direction)d);
        TGE_Vec2i b = tge_direction_vec(
            tge_direction_opposite((TGE_Direction)d));
        TGE_Vec2i sum = tge_vec2i_add(a, b);
        TGE_ASSERT(sum.x == 0 && sum.y == 0, "opposite cancels");
    }
}

int main(void)
{
    test_dx_dy_axes();
    test_vec_steps();
    test_opposite_pairs();
    test_opposite_composes_with_vec();
    return tge_test_report();
}
