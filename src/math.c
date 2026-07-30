#include "tge/tge_math.h"
#include <math.h>

float tge_vec2_length(TGE_Vector2 v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

float tge_vec2_distance(TGE_Vector2 a, TGE_Vector2 b)
{
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

bool tge_rect_contains(TGE_Rect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

bool tge_rect_intersects(TGE_Rect a, TGE_Rect b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

TGE_Vector2 tge_vec2(float x, float y)
{
    TGE_Vector2 v = {x, y};
    return v;
}

TGE_Vector2 tge_vec2_add(TGE_Vector2 a, TGE_Vector2 b)
{
    TGE_Vector2 v = {a.x + b.x, a.y + b.y};
    return v;
}

TGE_Vector2 tge_vec2_sub(TGE_Vector2 a, TGE_Vector2 b)
{
    TGE_Vector2 v = {a.x - b.x, a.y - b.y};
    return v;
}

TGE_Vector2 tge_vec2_scale(TGE_Vector2 v, float s)
{
    TGE_Vector2 r = {v.x * s, v.y * s};
    return r;
}

TGE_Rect tge_rect(int x, int y, int w, int h)
{
    TGE_Rect r = {x, y, w, h};
    return r;
}

bool tge_circle_intersects(TGE_Vector2 center, float r, TGE_Rect rect)
{
    float nearest_x = (float)(center.x < rect.x ? rect.x :
                      center.x > rect.x + rect.w ? rect.x + rect.w :
                      center.x);
    float nearest_y = (float)(center.y < rect.y ? rect.y :
                      center.y > rect.y + rect.h ? rect.y + rect.h :
                      center.y);
    float dx = center.x - nearest_x;
    float dy = center.y - nearest_y;
    return (dx * dx + dy * dy) <= (r * r);
}
