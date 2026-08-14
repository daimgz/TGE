#pragma once

#include "tge-extra/fixedstep.h"

namespace tge {

/* Fixed-timestep accumulator for deterministic simulation (Snake/Tetris).
 * Consumer-driven wrapper: added because a real snake needs the fixed-step
 * model of 06_snake_grid, not as probe coverage. */
struct FixedStep {
    TGE_FixedStep raw;

    FixedStep(float step = 0.10f) { tge_fixedstep_init(&raw, step); }
    void reset() { tge_fixedstep_reset(&raw); }
    void update(float dt) { tge_fixedstep_update(&raw, dt); }
    bool next() { return tge_fixedstep_next(&raw); }
    int pending() const { return tge_fixedstep_pending(&raw); }
};

} // namespace tge
