#ifndef TGE_RUNTIME_H_
#define TGE_RUNTIME_H_

#include <stdint.h>
#include <stdbool.h>
#include "tge_events.h"
#include "tge_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque runtime context: owns the terminal backend, input parsing and the
 * timer scheduler. A TGE_Diff describes which cells changed this frame and
 * is produced internally by the renderer. */
typedef struct TGE_Runtime TGE_Runtime;
typedef struct TGE_Diff TGE_Diff;

TGE_Runtime *tge_runtime_create(int width, int height);
void         tge_runtime_destroy(TGE_Runtime *rt);

bool tge_runtime_poll_event(TGE_Runtime *rt, TGE_Event *ev);

/* Presents only the cells that changed (fast path). */
void tge_runtime_present(TGE_Runtime *rt, TGE_Diff *diff,
                         const TGE_Cell *cells, int stride);

/* Presents every cell (full redraw). */
void tge_runtime_present_full(TGE_Runtime *rt, const TGE_Cell *cells,
                              int stride);

/* Schedules an event after a delay / on every interval. Returns a timer id
 * usable with tge_runtime_cancel_scheduled, or 0 on failure. */
int  tge_runtime_call_later(TGE_Runtime *rt, double delay, int event_id,
                            int priority);
int  tge_runtime_call_every(TGE_Runtime *rt, double interval, int event_id,
                            int priority);
void tge_runtime_cancel_scheduled(TGE_Runtime *rt, int timer_id);

/* Monotonic clock in ms and seconds since runtime creation. */
uint64_t tge_runtime_ticks(TGE_Runtime *rt);
double   tge_runtime_now(TGE_Runtime *rt);

int  tge_runtime_width(TGE_Runtime *rt);
int  tge_runtime_height(TGE_Runtime *rt);
/* Sets the terminal window title; no-op when the backend has no title
 * support. Passing NULL leaves the current title unchanged. */
void tge_runtime_set_title(TGE_Runtime *rt, const char *title);

#ifdef __cplusplus
}
#endif

#endif
