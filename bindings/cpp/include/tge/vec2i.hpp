#pragma once

#include "tge-extra/vec2i.h"

namespace tge {

/* Integer 2D cell coordinate. A value type over TGE_Vec2i with the usual
 * arithmetic so grid math reads naturally: `head + dir.to_vec()`. */
struct Vec2i {
    TGE_Vec2i raw;

    Vec2i() : raw{tge_vec2i_zero()} {}
    Vec2i(int x, int y) : raw{tge_vec2i(x, y)} {}
    explicit Vec2i(TGE_Vec2i r) : raw{r} {}

    int x() const { return raw.x; }
    int y() const { return raw.y; }

    Vec2i operator+(Vec2i o) const { return Vec2i(tge_vec2i_add(raw, o.raw)); }
    Vec2i operator-(Vec2i o) const { return Vec2i(tge_vec2i_sub(raw, o.raw)); }
    Vec2i operator*(int s) const    { return Vec2i(tge_vec2i_scale(raw, s)); }
    bool operator==(Vec2i o) const  { return tge_vec2i_eq(raw, o.raw); }
    bool operator!=(Vec2i o) const  { return !(*this == o); }

    operator TGE_Vec2i() const { return raw; }
};

} // namespace tge
