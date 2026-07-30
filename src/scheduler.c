#include "tge/tge_events.h"
#include "tge_internal.h"
#include <stdlib.h>

struct TGE_Scheduler {
    int dummy;
};

TGE_Scheduler *tge_scheduler_new(void)
{
    TGE_Scheduler *s = calloc(1, sizeof(TGE_Scheduler));
    (void)s;
    return NULL;
}

void tge_scheduler_free(TGE_Scheduler *s)
{
    (void)s;
}

int tge_scheduler_call_later(TGE_Scheduler *s, double delay_sec,
                              int event_id, int priority)
{
    (void)s; (void)delay_sec; (void)event_id; (void)priority;
    return 0;
}

int tge_scheduler_call_every(TGE_Scheduler *s, double interval,
                              int event_id, int priority)
{
    (void)s; (void)interval; (void)event_id; (void)priority;
    return 0;
}

void tge_scheduler_cancel(TGE_Scheduler *s, int timer_id)
{
    (void)s; (void)timer_id;
}

void tge_scheduler_poll(TGE_Scheduler *s, double now,
                        TGE_Event *ev_out, int *count)
{
    (void)s; (void)now; (void)ev_out; (void)count;
}
