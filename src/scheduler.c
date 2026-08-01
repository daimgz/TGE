#include "tge/tge_events.h"
#include "tge_internal.h"
#include <stdlib.h>

typedef struct {
    double due;
    double interval;
    int event_id;
    int priority;
    int seq;
    int heap_pos;
    int timer_id;
    bool active;
    bool repeating;
} SchedEntry;

struct TGE_Scheduler {
    SchedEntry items[TGE_SCHED_MAX_TIMERS];
    int heap[TGE_SCHED_MAX_TIMERS];
    int heap_count;
    int item_count;
    int next_timer_id;
    int seq;
    double now;
    SchedEntry *expired[TGE_SCHED_MAX_TIMERS];
};

TGE_Scheduler *tge_scheduler_new(void)
{
    TGE_Scheduler *s = (TGE_Scheduler *)calloc(1, sizeof(TGE_Scheduler));
    return s;
}

void tge_scheduler_free(TGE_Scheduler *s)
{
    free(s);
}

static bool heap_less(const TGE_Scheduler *s, int a, int b)
{
    const SchedEntry *ea = &s->items[s->heap[a]];
    const SchedEntry *eb = &s->items[s->heap[b]];
    if (ea->due != eb->due)
        return ea->due < eb->due;
    return ea->seq < eb->seq;
}

static void heap_swap(TGE_Scheduler *s, int a, int b)
{
    int tmp = s->heap[a];
    s->heap[a] = s->heap[b];
    s->heap[b] = tmp;
    s->items[s->heap[a]].heap_pos = a;
    s->items[s->heap[b]].heap_pos = b;
}

static void heap_sift_up(TGE_Scheduler *s, int idx)
{
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (!heap_less(s, idx, parent))
            break;
        heap_swap(s, idx, parent);
        idx = parent;
    }
}

static void heap_sift_down(TGE_Scheduler *s, int idx)
{
    for (;;) {
        int left = idx * 2 + 1;
        int right = left + 1;
        int smallest = idx;
        if (left < s->heap_count && heap_less(s, left, smallest))
            smallest = left;
        if (right < s->heap_count && heap_less(s, right, smallest))
            smallest = right;
        if (smallest == idx)
            break;
        heap_swap(s, idx, smallest);
        idx = smallest;
    }
}

static void heap_push(TGE_Scheduler *s, int item_idx)
{
    s->heap[s->heap_count] = item_idx;
    s->items[item_idx].heap_pos = s->heap_count;
    s->heap_count++;
    heap_sift_up(s, s->heap_count - 1);
}

static int heap_pop(TGE_Scheduler *s)
{
    int root = s->heap[0];
    s->items[root].heap_pos = -1;
    s->heap_count--;
    if (s->heap_count > 0) {
        s->heap[0] = s->heap[s->heap_count];
        s->items[s->heap[0]].heap_pos = 0;
        heap_sift_down(s, 0);
    }
    return root;
}

static int alloc_item(TGE_Scheduler *s)
{
    for (int i = 0; i < s->item_count; i++) {
        if (!s->items[i].active)
            return i;
    }
    if (s->item_count < TGE_SCHED_MAX_TIMERS)
        return s->item_count++;
    return -1;
}

static int sched_add(TGE_Scheduler *s, double when, double interval,
                     int event_id, int priority, bool repeating)
{
    int idx = alloc_item(s);
    if (idx < 0)
        return 0;
    SchedEntry *e = &s->items[idx];
    e->due = when;
    e->interval = interval;
    e->event_id = event_id;
    e->priority = priority;
    e->seq = s->seq++;
    e->timer_id = ++s->next_timer_id;
    e->active = true;
    e->repeating = repeating;
    heap_push(s, idx);
    return e->timer_id;
}

int tge_scheduler_call_later(TGE_Scheduler *s, double delay_sec,
                             int event_id, int priority)
{
    if (!s || delay_sec < 0.0)
        return 0;
    return sched_add(s, s->now + delay_sec, 0.0, event_id, priority, false);
}

int tge_scheduler_call_every(TGE_Scheduler *s, double interval,
                             int event_id, int priority)
{
    if (!s || interval <= 0.0)
        return 0;
    return sched_add(s, s->now + interval, interval, event_id, priority, true);
}

void tge_scheduler_cancel(TGE_Scheduler *s, int timer_id)
{
    if (!s || timer_id <= 0)
        return;
    for (int i = 0; i < s->item_count; i++) {
        SchedEntry *e = &s->items[i];
        if (!e->active || e->timer_id != timer_id)
            continue;
        int pos = e->heap_pos;
        e->active = false;
        s->heap_count--;
        if (pos < s->heap_count) {
            s->heap[pos] = s->heap[s->heap_count];
            s->items[s->heap[pos]].heap_pos = pos;
            heap_sift_up(s, pos);
            heap_sift_down(s, pos);
        }
        return;
    }
}

void tge_scheduler_poll(TGE_Scheduler *s, double now,
                        TGE_Event *ev_out, int *count)
{
    *count = 0;
    if (!s)
        return;

    s->now = now;

    int n = 0;
    while (s->heap_count > 0 && s->items[s->heap[0]].due <= now) {
        int idx = heap_pop(s);
        SchedEntry *e = &s->items[idx];
        if (!e->active)
            continue;
        if (e->repeating) {
            double next = e->due;
            do {
                next += e->interval;
            } while (next <= now);
            e->due = next;
            heap_push(s, idx);
        } else {
            e->active = false;
        }
        s->expired[n++] = e;
    }

    for (int i = 1; i < n; i++) {
        SchedEntry *tmp = s->expired[i];
        int j = i - 1;
        while (j >= 0 && (s->expired[j]->priority > tmp->priority ||
               (s->expired[j]->priority == tmp->priority &&
                s->expired[j]->seq > tmp->seq))) {
            s->expired[j + 1] = s->expired[j];
            j--;
        }
        s->expired[j + 1] = tmp;
    }

    for (int i = 0; i < n; i++) {
        ev_out[*count].type = TGE_EVENT_TIMER;
        ev_out[*count].data.timer.id = s->expired[i]->timer_id;
        ev_out[*count].data.timer.priority = s->expired[i]->priority;
        (*count)++;
    }
}
