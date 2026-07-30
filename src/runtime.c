#include "tge/tge_runtime.h"
#include "tge_internal.h"
#include <stdlib.h>
#include <string.h>

TGE_Runtime *tge_runtime_create(int width, int height)
{
    TGE_Runtime *rt = calloc(1, sizeof(TGE_Runtime));
    if (!rt) return NULL;
    rt->width = width;
    rt->height = height;
    rt->backend = tge_backend_ansi_create();
    if (rt->backend && rt->backend->init)
        rt->backend->init(rt->backend->data, width, height);
    return rt;
}

void tge_runtime_destroy(TGE_Runtime *rt)
{
    if (!rt) return;
    if (rt->backend) {
        if (rt->backend->term)
            rt->backend->term(rt->backend->data);
        free(rt->backend->data);
        free(rt->backend);
    }
    free(rt);
}

bool tge_runtime_poll_event(TGE_Runtime *rt, TGE_Event *ev)
{
    (void)rt; (void)ev;
    return false;
}

void tge_runtime_present(TGE_Runtime *rt, struct TGE_Diff *diff,
                         const TGE_Cell *cells, int stride)
{
    (void)rt; (void)diff; (void)cells; (void)stride;
}

int tge_runtime_call_later(TGE_Runtime *rt, double delay, int event_id,
                            int priority)
{
    (void)rt; (void)delay; (void)event_id; (void)priority;
    return 0;
}

int tge_runtime_call_every(TGE_Runtime *rt, double interval, int event_id,
                            int priority)
{
    (void)rt; (void)interval; (void)event_id; (void)priority;
    return 0;
}

void tge_runtime_cancel_scheduled(TGE_Runtime *rt, int timer_id)
{
    (void)rt; (void)timer_id;
}

uint64_t tge_runtime_ticks(TGE_Runtime *rt)
{
    (void)rt;
    return 0;
}

double tge_runtime_now(TGE_Runtime *rt)
{
    (void)rt;
    return 0.0;
}

int tge_runtime_width(TGE_Runtime *rt)
{
    return rt ? rt->width : 0;
}

int tge_runtime_height(TGE_Runtime *rt)
{
    return rt ? rt->height : 0;
}
