#include "tge/tge_runtime.h"
#include "tge_internal.h"
#include <stdlib.h>
#include <string.h>

static void rq_push(TGE_Runtime *rt, TGE_Event *ev)
{
    int next = (rt->q_head + 1) % TGE_RUNTIME_QUEUE_CAP;
    if (next == rt->q_tail)
        rt->q_tail = (rt->q_tail + 1) % TGE_RUNTIME_QUEUE_CAP;
    rt->queue[rt->q_head] = *ev;
    rt->q_head = next;
}

void tge_runtime_pump_input(TGE_Runtime *rt)
{
    if (!rt || !rt->backend)
        return;
    /* Terminals change size without sending anything; poll the real size each
     * frame (TIOCGWINSZ on the ANSI backend) and turn a change into a resize
     * event so the app can rebuild its canvases. */
    if (rt->backend->query_size) {
        int qw = 0, qh = 0;
        if (rt->backend->query_size(rt->backend->data, &qw, &qh) &&
            qw > 0 && qh > 0 && (qw != rt->width || qh != rt->height)) {
            rt->width = qw;
            rt->height = qh;
            TGE_Event ev;
            ev.type = TGE_EVENT_RESIZE;
            ev.data.resize.w = qw;
            ev.data.resize.h = qh;
            rq_push(rt, &ev);
        }
    }
    if (!rt->backend->read_input)
        return;
    char buf[256];
    int n = rt->backend->read_input(rt->backend->data, buf, (int)sizeof(buf));
    if (n > 0)
        tge_parser_feed(rt->parser, buf, n);
    tge_parser_flush(rt->parser);
    TGE_Event ev;
    while (tge_parser_poll(rt->parser, &ev))
        rq_push(rt, &ev);
}

void tge_runtime_pump_timers(TGE_Runtime *rt, double now_sec)
{
    if (!rt || !rt->scheduler)
        return;
    TGE_Event evs[TGE_SCHED_MAX_TIMERS];
    int count = 0;
    tge_scheduler_poll(rt->scheduler, now_sec, evs, &count);
    for (int i = 0; i < count; i++)
        rq_push(rt, &evs[i]);
}

bool tge_runtime_poll_queued(TGE_Runtime *rt, TGE_Event *ev)
{
    if (!rt || rt->q_tail == rt->q_head)
        return false;
    *ev = rt->queue[rt->q_tail];
    rt->q_tail = (rt->q_tail + 1) % TGE_RUNTIME_QUEUE_CAP;
    if (ev->type == TGE_EVENT_RESIZE) {
        rt->width = ev->data.resize.w;
        rt->height = ev->data.resize.h;
    }
    return true;
}

TGE_Runtime *tge_runtime_create(int width, int height)
{
    TGE_Backend *b = tge_backend_ansi_create();
    if (!b)
        return NULL;
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, width, height);
    if (!rt) {
        free(b->data);
        free(b);
    }
    return rt;
}

TGE_Runtime *tge_runtime_create_with_backend(TGE_Backend *backend,
                                             int width, int height)
{
    if (!backend)
        return NULL;
    TGE_Runtime *rt = (TGE_Runtime *)calloc(1, sizeof(TGE_Runtime));
    if (!rt)
        return NULL;
    rt->backend = backend;
    rt->width = width;
    rt->height = height;
    rt->parser = tge_parser_create();
    rt->scheduler = tge_scheduler_new();
    if (!rt->parser || !rt->scheduler) {
        tge_runtime_destroy(rt);
        return NULL;
    }
    if (backend->init && !backend->init(backend->data, width, height)) {
        tge_runtime_destroy(rt);
        return NULL;
    }
    if (backend->query_size) {
        int qw = 0, qh = 0;
        if (backend->query_size(backend->data, &qw, &qh) && qw > 0 && qh > 0) {
            rt->width = qw;
            rt->height = qh;
        }
    }
    {
        TGE_Event evs[TGE_SCHED_MAX_TIMERS];
        int count = 0;
        tge_scheduler_poll(rt->scheduler, tge_runtime_now(rt), evs, &count);
    }
    return rt;
}

void tge_runtime_destroy(TGE_Runtime *rt)
{
    if (!rt)
        return;
    if (rt->backend) {
        if (rt->backend->term)
            rt->backend->term(rt->backend->data);
        free(rt->backend->data);
        free(rt->backend);
    }
    if (rt->parser)
        tge_parser_destroy(rt->parser);
    if (rt->scheduler)
        tge_scheduler_free(rt->scheduler);
    free(rt->full_spans);
    free(rt);
}

bool tge_runtime_poll_event(TGE_Runtime *rt, TGE_Event *ev)
{
    if (!rt)
        return false;
    tge_runtime_pump_input(rt);
    tge_runtime_pump_timers(rt, tge_runtime_now(rt));
    return tge_runtime_poll_queued(rt, ev);
}

void tge_runtime_present(TGE_Runtime *rt, struct TGE_Diff *diff,
                         const TGE_Cell *cells, int stride)
{
    if (rt && rt->backend && rt->backend->present)
        rt->backend->present(rt->backend->data, diff, cells, stride);
}

void tge_runtime_present_full(TGE_Runtime *rt, const TGE_Cell *cells, int stride)
{
    if (!rt || !rt->backend || !rt->backend->present || !cells)
        return;
    if (rt->full_spans_cap < rt->height) {
        TGE_DirtySpan *ns = (TGE_DirtySpan *)realloc(
            rt->full_spans, (size_t)rt->height * sizeof(TGE_DirtySpan));
        if (!ns)
            return;
        rt->full_spans = ns;
        rt->full_spans_cap = rt->height;
    }
    for (int y = 0; y < rt->height; y++) {
        rt->full_spans[y].y = y;
        rt->full_spans[y].x_start = 0;
        rt->full_spans[y].x_end = rt->width;
    }
    TGE_Diff diff = { rt->full_spans, rt->height, rt->full_spans_cap };
    rt->backend->present(rt->backend->data, &diff, cells, stride);
}

void tge_runtime_set_title(TGE_Runtime *rt, const char *title)
{
    if (rt && rt->backend && rt->backend->set_title && title)
        rt->backend->set_title(rt->backend->data, title);
}

int tge_runtime_call_later(TGE_Runtime *rt, double delay, int event_id,
                           int priority)
{
    if (!rt)
        return 0;
    return tge_scheduler_call_later(rt->scheduler, delay, event_id, priority);
}

int tge_runtime_call_every(TGE_Runtime *rt, double interval, int event_id,
                           int priority)
{
    if (!rt)
        return 0;
    return tge_scheduler_call_every(rt->scheduler, interval, event_id, priority);
}

void tge_runtime_cancel_scheduled(TGE_Runtime *rt, int timer_id)
{
    if (!rt)
        return;
    tge_scheduler_cancel(rt->scheduler, timer_id);
}

uint64_t tge_runtime_ticks(TGE_Runtime *rt)
{
    if (!rt || !rt->backend || !rt->backend->ticks)
        return 0;
    return rt->backend->ticks(rt->backend->data);
}

double tge_runtime_now(TGE_Runtime *rt)
{
    return (double)tge_runtime_ticks(rt) / 1000.0;
}

int tge_runtime_width(TGE_Runtime *rt)
{
    return rt ? rt->width : 0;
}

int tge_runtime_height(TGE_Runtime *rt)
{
    return rt ? rt->height : 0;
}
