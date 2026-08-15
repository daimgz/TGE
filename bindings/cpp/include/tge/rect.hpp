#pragma once

#include "tge/vec2i.hpp"

namespace tge {

/* Axis-aligned rectangle in cell (integer) coordinates, mirroring TGE_Rect.
 * Carries the containment test the game needs (e.g. the ghost house) so code
 * never names tge_rect_contains / TGE_Rect directly. */
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
    bool contains(Vec2i p) const { return contains(p.x(), p.y()); }
};

} // namespace tge
