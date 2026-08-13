#pragma once

#include "tge-extra/direction.h"
#include "tge/vec2i.hpp"

namespace tge {

struct Event; // defined in input.hpp; Direction::from_event needs it

/* Cardinal grid direction. A thin wrapper over TGE_Direction that carries the
 * helpers where they belong (opposite / to_vec / from_event) instead of leaving
 * the caller to call the free tge_direction_* functions. */
struct Direction {
    enum Value : int {
        None = TGE_DIR_NONE,
        Up = TGE_DIR_UP,
        Down = TGE_DIR_DOWN,
        Left = TGE_DIR_LEFT,
        Right = TGE_DIR_RIGHT,
    } v;

    Direction(Value val = None) : v(val) {}
    explicit operator Value() const { return v; }

    bool operator==(Direction o) const { return v == o.v; }
    bool operator!=(Direction o) const { return v != o.v; }

    Direction opposite() const {
        return Direction(Value(tge_direction_opposite(TGE_Direction(v))));
    }
    Vec2i to_vec() const {
        return Vec2i(tge_direction_vec(TGE_Direction(v)));
    }

    static Direction from_event(const Event &e);
};

} // namespace tge
