#include "tge/tge_events.h"
#include "tge_internal.h"
#include "tge_test.h"

TGE_TEST(create_free)
{
    TGE_Scheduler *s = tge_scheduler_new();
    TGE_ASSERT(s != 0, "scheduler created");
    tge_scheduler_free(s);
}

TGE_TEST(call_later_fires_once)
{
    TGE_Scheduler *s = tge_scheduler_new();
    int id = tge_scheduler_call_later(s, 2.0, 42, TGE_TIMER_NORMAL);
    TGE_ASSERT(id > 0, "id assigned");

    TGE_Event ev[4];
    int n = 0;
    tge_scheduler_poll(s, 1.0, ev, &n);
    TGE_ASSERT(n == 0, "not due at 1.0");

    tge_scheduler_poll(s, 3.0, ev, &n);
    TGE_ASSERT(n == 1, "due at 3.0 (2.0 after creation)");
    TGE_ASSERT(ev[0].type == TGE_EVENT_TIMER, "timer event");
    TGE_ASSERT(ev[0].data.timer.id == id, "same id");
    TGE_ASSERT(ev[0].data.timer.priority == TGE_TIMER_NORMAL, "priority");

    tge_scheduler_poll(s, 10.0, ev, &n);
    TGE_ASSERT(n == 0, "one-shot does not repeat");

    tge_scheduler_free(s);
}

TGE_TEST(call_later_negative_delay_rejected)
{
    TGE_Scheduler *s = tge_scheduler_new();
    TGE_ASSERT(tge_scheduler_call_later(s, -1.0, 1, TGE_TIMER_NORMAL) == 0,
               "negative delay rejected");
    tge_scheduler_free(s);
}

TGE_TEST(priority_order)
{
    TGE_Scheduler *s = tge_scheduler_new();
    int low = tge_scheduler_call_later(s, 1.0, 1, TGE_TIMER_LOW);
    int high = tge_scheduler_call_later(s, 1.0, 2, TGE_TIMER_HIGH);
    int normal_a = tge_scheduler_call_later(s, 1.0, 3, TGE_TIMER_NORMAL);
    int normal_b = tge_scheduler_call_later(s, 1.0, 4, TGE_TIMER_NORMAL);
    TGE_ASSERT(low > 0 && high > 0 && normal_a > 0 && normal_b > 0, "ids");

    TGE_Event ev[8];
    int n = 0;
    tge_scheduler_poll(s, 0.9, ev, &n);
    TGE_ASSERT(n == 0, "not due at 0.9");

    tge_scheduler_poll(s, 2.0, ev, &n);
    TGE_ASSERT(n == 4, "all four expired");
    TGE_ASSERT(ev[0].data.timer.id == high, "HIGH first");
    TGE_ASSERT(ev[1].data.timer.id == normal_a, "NORMAL first inserted");
    TGE_ASSERT(ev[2].data.timer.id == normal_b, "NORMAL FIFO");
    TGE_ASSERT(ev[3].data.timer.id == low, "LOW last");

    tge_scheduler_free(s);
}

TGE_TEST(call_every_repeats)
{
    TGE_Scheduler *s = tge_scheduler_new();
    int id = tge_scheduler_call_every(s, 0.5, 7, TGE_TIMER_NORMAL);
    TGE_ASSERT(id > 0, "id");

    TGE_Event ev[4];
    int n = 0;
    tge_scheduler_poll(s, 1.0, ev, &n);
    TGE_ASSERT(n == 1, "first fire at 1.0 (interval 0.5 since creation)");

    tge_scheduler_poll(s, 1.5, ev, &n);
    TGE_ASSERT(n == 1, "first fire at 1.5");
    TGE_ASSERT(ev[0].data.timer.id == id, "correct id");

    tge_scheduler_poll(s, 2.0, ev, &n);
    TGE_ASSERT(n == 1, "second fire at 2.0");

    tge_scheduler_poll(s, 2.1, ev, &n);
    TGE_ASSERT(n == 0, "not due at 2.1");

    tge_scheduler_free(s);
}

TGE_TEST(call_every_catch_up_no_burst)
{
    TGE_Scheduler *s = tge_scheduler_new();
    tge_scheduler_call_every(s, 0.5, 7, TGE_TIMER_NORMAL);

    TGE_Event ev[8];
    int n = 0;
    tge_scheduler_poll(s, 0.4, ev, &n);
    TGE_ASSERT(n == 0, "not yet");

    tge_scheduler_poll(s, 3.0, ev, &n);
    TGE_ASSERT(n == 1, "single event after long stall, no burst");

    tge_scheduler_free(s);
}

TGE_TEST(call_every_bad_interval)
{
    TGE_Scheduler *s = tge_scheduler_new();
    TGE_ASSERT(tge_scheduler_call_every(s, 0.0, 1, TGE_TIMER_NORMAL) == 0,
               "zero interval rejected");
    TGE_ASSERT(tge_scheduler_call_every(s, -5.0, 1, TGE_TIMER_NORMAL) == 0,
               "negative interval rejected");
    tge_scheduler_free(s);
}

TGE_TEST(cancel_before_fire)
{
    TGE_Scheduler *s = tge_scheduler_new();
    int a = tge_scheduler_call_later(s, 1.0, 1, TGE_TIMER_NORMAL);
    int b = tge_scheduler_call_later(s, 1.0, 2, TGE_TIMER_NORMAL);
    TGE_ASSERT(a > 0 && b > 0, "ids");

    tge_scheduler_cancel(s, a);
    tge_scheduler_cancel(s, 9999);

    TGE_Event ev[4];
    int n = 0;
    tge_scheduler_poll(s, 0.9, ev, &n);
    TGE_ASSERT(n == 0, "not due at 0.9");

    tge_scheduler_poll(s, 2.0, ev, &n);
    TGE_ASSERT(n == 1, "only b fires");
    TGE_ASSERT(ev[0].data.timer.id == b, "b fired");
    tge_scheduler_free(s);
}

TGE_TEST(cancel_repeating)
{
    TGE_Scheduler *s = tge_scheduler_new();
    int a = tge_scheduler_call_every(s, 0.5, 1, TGE_TIMER_NORMAL);
    TGE_ASSERT(a > 0, "id");

    TGE_Event ev[4];
    int n = 0;
    tge_scheduler_poll(s, 1.5, ev, &n);
    TGE_ASSERT(n == 1, "first fire");

    tge_scheduler_cancel(s, a);
    tge_scheduler_poll(s, 2.5, ev, &n);
    TGE_ASSERT(n == 0, "cancelled repeating timer stops");
    tge_scheduler_free(s);
}

TGE_TEST(slot_reuse_after_fire)
{
    TGE_Scheduler *s = tge_scheduler_new();
    TGE_Event ev[4];
    int n = 0;

    int a = tge_scheduler_call_later(s, 0.5, 1, TGE_TIMER_NORMAL);
    TGE_ASSERT(a > 0, "a");
    tge_scheduler_poll(s, 1.0, ev, &n);
    TGE_ASSERT(n == 1, "a fired");

    int b = tge_scheduler_call_later(s, 0.5, 2, TGE_TIMER_NORMAL);
    TGE_ASSERT(b > 0, "slot reused");
    tge_scheduler_poll(s, 2.0, ev, &n);
    TGE_ASSERT(n == 1, "b fired");
    TGE_ASSERT(ev[0].data.timer.id == b, "b id");
    tge_scheduler_free(s);
}

TGE_TEST(capacity_exhausted)
{
    TGE_Scheduler *s = tge_scheduler_new();
    int ids = 0;
    int zeros = 0;
    for (int i = 0; i < TGE_SCHED_MAX_TIMERS + 10; i++) {
        int id = tge_scheduler_call_later(s, 100.0, i, TGE_TIMER_NORMAL);
        if (id > 0)
            ids++;
        else
            zeros++;
    }
    TGE_ASSERT(ids == TGE_SCHED_MAX_TIMERS, "accepted up to capacity");
    TGE_ASSERT(zeros == 10, "rejects beyond capacity");
    tge_scheduler_free(s);
}

int main(void)
{
    test_create_free();
    test_call_later_fires_once();
    test_call_later_negative_delay_rejected();
    test_priority_order();
    test_call_every_repeats();
    test_call_every_catch_up_no_burst();
    test_call_every_bad_interval();
    test_cancel_before_fire();
    test_cancel_repeating();
    test_slot_reuse_after_fire();
    test_capacity_exhausted();
    return tge_test_report();
}
