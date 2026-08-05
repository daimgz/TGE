#ifndef TGE_EXTRA_TIMER_H_
#define TGE_EXTRA_TIMER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Accumulator timer: fires one "tick" every `interval` seconds of
 * accumulated update time. Use it for cooldowns, spawn rates and event
 * pacing; for stepping a simulation at a fixed rate prefer TGE_FixedStep.
 *
 * Usage:
 *   TGE_Timer t;
 *   tge_timer_init(&t, 0.5f);
 *   // per frame: feed dt, then drain any ticks that fired
 *   tge_timer_update(&t, dt);
 *   while (tge_timer_tick(&t))
 *       fire();
 *
 * An interval <= 0 disables the timer: update() does not accumulate and
 * tick() always returns false. */
typedef struct {
    float interval;
    float acc;
} TGE_Timer;

void tge_timer_init(TGE_Timer *t, float interval);
/* Clears accumulated time; keeps the interval. */
void tge_timer_reset(TGE_Timer *t);
/* Accumulates dt seconds. No-op while interval <= 0. */
void tge_timer_update(TGE_Timer *t, float dt);
/* True when at least one full interval has accumulated; consumes one.
 * Repeated calls drain the backlog. Never fires while interval <= 0. */
bool tge_timer_tick(TGE_Timer *t);

#ifdef __cplusplus
}
#endif

#endif
