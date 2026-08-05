#include "vec2i.h"

#include "tge/tge_math.h"

#include <stdlib.h>

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

TGE_Vec2i tge_vec2i_clamp_rect(TGE_Vec2i p, TGE_Rect r)
{
    if (r.w <= 0 || r.h <= 0)
        return tge_vec2i(r.x, r.y);
    TGE_Vec2i out;
    int lo_x = r.x;
    int hi_x = r.x + r.w - 1;
    int lo_y = r.y;
    int hi_y = r.y + r.h - 1;
    out.x = p.x < lo_x ? lo_x : (p.x > hi_x ? hi_x : p.x);
    out.y = p.y < lo_y ? lo_y : (p.y > hi_y ? hi_y : p.y);
    return out;
}

TGE_Vec2i tge_rect_random_point(TGE_Rect r)
{
    if (r.w <= 0 || r.h <= 0)
        return tge_vec2i(r.x, r.y);
    return tge_vec2i(r.x + rand() % r.w, r.y + rand() % r.h);
}

TGE_Vec2i tge_rect_translate_point(TGE_Rect r, TGE_Vec2i local)
{
    TGE_Vec2i v = { r.x + local.x, r.y + local.y };
    return v;
}
