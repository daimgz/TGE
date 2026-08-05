#ifndef TGE_EXTRA_FIXEDSTEP_H_
#define TGE_EXTRA_FIXEDSTEP_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed-timestep accumulator for deterministic simulation stepping (Snake,
 * Tetris, roguelikes): the simulation advances in fixed `step`-second
 * increments regardless of frame time.
 *
 * Usage:
 *   TGE_FixedStep s;
 *   tge_fixedstep_init(&s, 0.10f);
 *   // per frame: feed dt, then run every step that is due
 *   tge_fixedstep_update(&s, dt);
 *   while (tge_fixedstep_next(&s))
 *       step_once();
 *
 * dt fed to a single update() is clamped to `max_step` seconds so a long
 * hitch cannot produce an unbounded backlog ("spiral of death"). A step
 * <= 0 disables the stepper. */
typedef struct {
    float step;
    float max_step;
    float acc;
} TGE_FixedStep;

/* step: fixed simulation increment. max_step defaults to 10 steps. */
void tge_fixedstep_init(TGE_FixedStep *s, float step);
/* Clears accumulated time; keeps step and max_step. */
void tge_fixedstep_reset(TGE_FixedStep *s);
/* Accumulates dt (clamped to max_step, ignored while step <= 0). */
void tge_fixedstep_update(TGE_FixedStep *s, float dt);
/* True when at least one full step is due; consumes one. */
bool tge_fixedstep_next(TGE_FixedStep *s);
/* Number of full steps currently pending, without consuming them. */
int tge_fixedstep_pending(const TGE_FixedStep *s);

#ifdef __cplusplus
}
#endif

#endif
