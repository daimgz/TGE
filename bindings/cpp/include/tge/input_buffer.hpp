#pragma once

#include "tge-extra/input_buffer.h"
#include "tge/direction.hpp"

namespace tge {

/* Fixed-capacity FIFO of queued turns. Consumer-driven: added because a real
 * snake needs the turn-queue semantics of 06_snake_grid (fast input between
 * fixed steps is not lost), not as probe coverage. Capacity defaults to 4,
 * matching 01_snake.c / 06_snake_grid.c. */
struct InputBuffer {
    TGE_InputBuffer raw;

    explicit InputBuffer(int capacity = 4) { tge_input_buffer_init(&raw, capacity); }

    bool push(Direction d) { return tge_input_buffer_push(&raw, TGE_Direction(d.v)); }

    bool pop(Direction &out) {
        TGE_Direction d;
        if (!tge_input_buffer_pop(&raw, &d))
            return false;
        out = Direction(Direction::Value(d));
        return true;
    }

    void clear() { tge_input_buffer_clear(&raw); }
    int count() const { return tge_input_buffer_count(&raw); }
};

} // namespace tge
