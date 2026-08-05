#include "tge-extra/timer.h"

#include "tge_test.h"

TGE_TEST(no_tick_until_interval_elapses)
{
    TGE_Timer t;
    tge_timer_init(&t, 0.5f);
    tge_timer_update(&t, 0.4f);
    TGE_ASSERT(!tge_timer_tick(&t), "no tick before interval");
    TGE_ASSERT(!tge_timer_tick(&t), "repeated tick is still false");
    tge_timer_update(&t, 0.2f);
    TGE_ASSERT(tge_timer_tick(&t), "tick once interval elapses");
    TGE_ASSERT(!tge_timer_tick(&t), "tick consumed");
}

TGE_TEST(update_is_accumulative_across_frames)
{
    TGE_Timer t;
    tge_timer_init(&t, 1.0f);
    for (int i = 0; i < 10; i++)
        tge_timer_update(&t, 0.1f);
    TGE_ASSERT(tge_timer_tick(&t), "10 frames of 0.1 fire one tick");
    TGE_ASSERT(!tge_timer_tick(&t), "no more ticks");
}

TGE_TEST(drains_backlog)
{
    TGE_Timer t;
    tge_timer_init(&t, 0.1f);
    tge_timer_update(&t, 0.45f);
    int n = 0;
    while (tge_timer_tick(&t))
        n++;
    TGE_ASSERT(n == 4, "0.45/0.1 fires exactly 4 ticks");
}

TGE_TEST(exact_multiple_drains_cleanly)
{
    TGE_Timer t;
    tge_timer_init(&t, 0.25f);
    tge_timer_update(&t, 1.0f);
    int n = 0;
    while (tge_timer_tick(&t))
        n++;
    TGE_ASSERT(n == 4, "1.0/0.25 fires exactly 4 ticks");
}

TGE_TEST(reset_clears_accumulation)
{
    TGE_Timer t;
    tge_timer_init(&t, 0.1f);
    tge_timer_update(&t, 5.0f);
    tge_timer_reset(&t);
    TGE_ASSERT(!tge_timer_tick(&t), "reset clears acc");
    tge_timer_update(&t, 0.05f);
    TGE_ASSERT(!tge_timer_tick(&t), "accumulation restarts from zero");
}

TGE_TEST(invalid_interval_never_fires)
{
    TGE_Timer t;
    tge_timer_init(&t, 0.0f);
    tge_timer_update(&t, 100.0f);
    TGE_ASSERT(!tge_timer_tick(&t), "zero interval disabled");
    tge_timer_init(&t, -1.0f);
    tge_timer_update(&t, 100.0f);
    TGE_ASSERT(!tge_timer_tick(&t), "negative interval disabled");
}

TGE_TEST(null_safety)
{
    tge_timer_init(NULL, 0.1f);
    tge_timer_reset(NULL);
    tge_timer_update(NULL, 1.0f);
    TGE_ASSERT(!tge_timer_tick(NULL), "NULL tick is false");
}

int main(void)
{
    test_no_tick_until_interval_elapses();
    test_update_is_accumulative_across_frames();
    test_drains_backlog();
    test_exact_multiple_drains_cleanly();
    test_reset_clears_accumulation();
    test_invalid_interval_never_fires();
    test_null_safety();
    return tge_test_report();
}
