#include "tge-extra/animation.h"

#include "tge_test.h"

static bool fok(float v, float want)
{
    float lo = want - 0.001f;
    float hi = want + 0.001f;
    return v >= lo && v <= hi;
}

TGE_TEST(initial_state_after_create)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 2.0f, TGE_EASE_LINEAR);
    TGE_ASSERT(fok(tge_anim_value(a), 0.0f), "value == from");
    TGE_ASSERT(fok(tge_anim_progress(a), 0.0f), "progress 0");
    TGE_ASSERT(!tge_anim_playing(a), "not playing");
    TGE_ASSERT(!tge_anim_finished(a), "not finished");
    tge_anim_destroy(a);
}

TGE_TEST(linear_progress)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    tge_anim_update(a, 0.25f);
    TGE_ASSERT(fok(tge_anim_value(a), 2.5f), "value 2.5");
    TGE_ASSERT(fok(tge_anim_progress(a), 0.25f), "progress 0.25");
    TGE_ASSERT(tge_anim_playing(a), "playing");
    TGE_ASSERT(!tge_anim_finished(a), "not finished");
    tge_anim_destroy(a);
}

TGE_TEST(reaches_end)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    tge_anim_update(a, 1.0f);
    TGE_ASSERT(fok(tge_anim_value(a), 10.0f), "value == to");
    TGE_ASSERT(fok(tge_anim_progress(a), 1.0f), "progress 1");
    TGE_ASSERT(!tge_anim_playing(a), "stopped");
    TGE_ASSERT(tge_anim_finished(a), "finished");
    tge_anim_destroy(a);
}

TGE_TEST(overshoot_clamps)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    tge_anim_update(a, 0.6f);
    tge_anim_update(a, 0.6f);
    TGE_ASSERT(tge_anim_finished(a), "finished");
    TGE_ASSERT(fok(tge_anim_value(a), 10.0f), "clamped to to");
    TGE_ASSERT(fok(tge_anim_progress(a), 1.0f), "progress clamped");
    tge_anim_destroy(a);
}

TGE_TEST(reset_keeps_stopped)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    tge_anim_update(a, 0.5f);
    tge_anim_reset(a);
    TGE_ASSERT(fok(tge_anim_value(a), 0.0f), "value back to from");
    TGE_ASSERT(fok(tge_anim_progress(a), 0.0f), "progress 0");
    TGE_ASSERT(!tge_anim_playing(a), "not playing after reset");
    TGE_ASSERT(!tge_anim_finished(a), "not finished after reset");
    tge_anim_destroy(a);
}

TGE_TEST(play_rewinds)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    tge_anim_update(a, 0.8f);
    tge_anim_play(a);
    TGE_ASSERT(fok(tge_anim_progress(a), 0.0f), "replay rewinds");
    tge_anim_destroy(a);
}

TGE_TEST(stop_holds_position)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    tge_anim_update(a, 0.4f);
    tge_anim_stop(a);
    float v = tge_anim_value(a);
    tge_anim_update(a, 1.0f);
    TGE_ASSERT(tge_anim_value(a) == v, "stopped anim ignores update");
    TGE_ASSERT(!tge_anim_playing(a), "not playing");
    tge_anim_destroy(a);
}

TGE_TEST(loop_wraps)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_set_loop(a, true);
    tge_anim_play(a);
    tge_anim_update(a, 1.5f);
    TGE_ASSERT(fok(tge_anim_progress(a), 0.5f), "wrapped progress 0.5");
    TGE_ASSERT(fok(tge_anim_value(a), 5.0f), "wrapped value 5");
    TGE_ASSERT(tge_anim_playing(a), "still playing");
    TGE_ASSERT(!tge_anim_finished(a), "loop never finished");
    tge_anim_destroy(a);
}

TGE_TEST(loop_never_finished)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_LINEAR);
    tge_anim_set_loop(a, true);
    tge_anim_play(a);
    for (int i = 0; i < 100; i++)
        tge_anim_update(a, 0.1f);
    TGE_ASSERT(!tge_anim_finished(a), "not finished after many updates");
    TGE_ASSERT(tge_anim_playing(a), "playing");
    tge_anim_destroy(a);
}

TGE_TEST(zero_duration_finishes_instantly)
{
    TGE_Anim *a = tge_anim_create(5.0f, 9.0f, 0.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    TGE_ASSERT(tge_anim_finished(a), "finished immediately after play");
    TGE_ASSERT(fok(tge_anim_value(a), 9.0f), "value == to");
    TGE_ASSERT(fok(tge_anim_progress(a), 1.0f), "progress 1");
    tge_anim_update(a, 0.5f);
    TGE_ASSERT(tge_anim_finished(a), "still finished after update");
    tge_anim_destroy(a);
}

TGE_TEST(negative_duration_behaves_as_zero)
{
    TGE_Anim *a = tge_anim_create(5.0f, 9.0f, -2.0f, TGE_EASE_LINEAR);
    tge_anim_play(a);
    TGE_ASSERT(tge_anim_finished(a), "finished immediately");
    TGE_ASSERT(fok(tge_anim_value(a), 9.0f), "value == to");
    tge_anim_destroy(a);
}

TGE_TEST(zero_duration_loop_never_finishes)
{
    TGE_Anim *a = tge_anim_create(5.0f, 9.0f, 0.0f, TGE_EASE_LINEAR);
    tge_anim_set_loop(a, true);
    tge_anim_play(a);
    TGE_ASSERT(!tge_anim_finished(a), "looping zero-duration not finished");
    tge_anim_update(a, 1.0f);
    TGE_ASSERT(!tge_anim_finished(a), "still not finished");
    TGE_ASSERT(fok(tge_anim_value(a), 9.0f), "value == to");
    tge_anim_destroy(a);
}

TGE_TEST(ease_in_out_continuous_at_half)
{
    TGE_Anim *a = tge_anim_create(0.0f, 1.0f, 1.0f, TGE_EASE_IN_OUT);
    tge_anim_play(a);
    tge_anim_update(a, 0.5f);
    TGE_ASSERT(fok(tge_anim_value(a), 0.5f), "in-out midpoint is 0.5");
    tge_anim_destroy(a);
}

TGE_TEST(ease_in_is_slow_start)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_IN);
    tge_anim_play(a);
    tge_anim_update(a, 0.25f);
    float v = tge_anim_value(a);
    TGE_ASSERT(v > 0.0f && v < 2.5f, "ease-in below linear at 25%");
    tge_anim_destroy(a);
}

TGE_TEST(ease_out_is_fast_start)
{
    TGE_Anim *a = tge_anim_create(0.0f, 10.0f, 1.0f, TGE_EASE_OUT);
    tge_anim_play(a);
    tge_anim_update(a, 0.25f);
    float v = tge_anim_value(a);
    TGE_ASSERT(v > 2.5f && v < 10.0f, "ease-out above linear at 25%");
    tge_anim_destroy(a);
}

TGE_TEST(null_safety)
{
    TGE_ASSERT(fok(tge_anim_value(NULL), 0.0f), "value NULL");
    TGE_ASSERT(fok(tge_anim_progress(NULL), 0.0f), "progress NULL");
    TGE_ASSERT(!tge_anim_playing(NULL), "playing NULL");
    TGE_ASSERT(!tge_anim_finished(NULL), "finished NULL");
    tge_anim_play(NULL);
    tge_anim_reset(NULL);
    tge_anim_stop(NULL);
    tge_anim_set_loop(NULL, true);
    tge_anim_update(NULL, 1.0f);
    tge_anim_destroy(NULL);
}

int main(void)
{
    test_initial_state_after_create();
    test_linear_progress();
    test_reaches_end();
    test_overshoot_clamps();
    test_reset_keeps_stopped();
    test_play_rewinds();
    test_stop_holds_position();
    test_loop_wraps();
    test_loop_never_finished();
    test_zero_duration_finishes_instantly();
    test_negative_duration_behaves_as_zero();
    test_zero_duration_loop_never_finishes();
    test_ease_in_out_continuous_at_half();
    test_ease_in_is_slow_start();
    test_ease_out_is_fast_start();
    test_null_safety();
    return tge_test_report();
}
