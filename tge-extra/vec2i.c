#include "vec2i.h"

TGE_Vec2i tge_vec2i(int x, int y)
{
    TGE_Vec2i v = { x, y };
    return v;
}

TGE_Vec2i tge_vec2i_zero(void)
{
    TGE_Vec2i v = { 0, 0 };
    return v;
}

TGE_Vec2i tge_vec2i_add(TGE_Vec2i a, TGE_Vec2i b)
{
    TGE_Vec2i v = { a.x + b.x, a.y + b.y };
    return v;
}

TGE_Vec2i tge_vec2i_sub(TGE_Vec2i a, TGE_Vec2i b)
{
    TGE_Vec2i v = { a.x - b.x, a.y - b.y };
    return v;
}

TGE_Vec2i tge_vec2i_scale(TGE_Vec2i v, int s)
{
    TGE_Vec2i r = { v.x * s, v.y * s };
    return r;
}

bool tge_vec2i_eq(TGE_Vec2i a, TGE_Vec2i b)
{
    return a.x == b.x && a.y == b.y;
}
