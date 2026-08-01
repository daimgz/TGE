#ifndef TGE_RUNTIME_H_
#define TGE_RUNTIME_H_

#include <stdint.h>
#include <stdbool.h>
#include "tge_events.h"
#include "tge_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TGE_Runtime TGE_Runtime;
typedef struct TGE_Diff TGE_Diff;

TGE_Runtime *tge_runtime_create(int width, int height);
void         tge_runtime_destroy(TGE_Runtime *rt);

bool tge_runtime_poll_event(TGE_Runtime *rt, TGE_Event *ev);

void tge_runtime_present(TGE_Runtime *rt, TGE_Diff *diff,
                         const TGE_Cell *cells, int stride);

void tge_runtime_present_full(TGE_Runtime *rt, const TGE_Cell *cells,
                              int stride);

int  tge_runtime_call_later(TGE_Runtime *rt, double delay, int event_id,
                            int priority);
int  tge_runtime_call_every(TGE_Runtime *rt, double interval, int event_id,
                            int priority);
void tge_runtime_cancel_scheduled(TGE_Runtime *rt, int timer_id);

uint64_t tge_runtime_ticks(TGE_Runtime *rt);
double   tge_runtime_now(TGE_Runtime *rt);

int tge_runtime_width(TGE_Runtime *rt);
int tge_runtime_height(TGE_Runtime *rt);

#ifdef __cplusplus
}
#endif

#endif
