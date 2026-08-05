#include "tge-extra/fixedstep.h"

#include "tge_test.h"

TGE_TEST(steps_at_fixed_interval)
{
    TGE_FixedStep s;
    tge_fixedstep_init(&s, 0.1f);
    tge_fixedstep_update(&s, 0.05f);
    TGE_ASSERT(!tge_fixedstep_next(&s), "no step before full increment");
    TGE_ASSERT(tge_fixedstep_pending(&s) == 0, "pending 0");
    tge_fixedstep_update(&s, 0.05f);
    TGE_ASSERT(tge_fixedstep_next(&s), "one step after 0.1");
    TGE_ASSERT(!tge_fixedstep_next(&s), "single step consumed");
}

TGE_TEST(multiple_steps_pending_and_drained)
{
    TGE_FixedStep s;
    tge_fixedstep_init(&s, 0.1f);
    tge_fixedstep_update(&s, 0.35f);
    TGE_ASSERT(tge_fixedstep_pending(&s) == 3, "pending 3");
    int n = 0;
    while (tge_fixedstep_next(&s))
        n++;
    TGE_ASSERT(n == 3, "drains exactly 3");
    TGE_ASSERT(!tge_fixedstep_next(&s), "fully drained");
}

TGE_TEST(dt_clamped_to_max_step)
{
    TGE_FixedStep s;
    tge_fixedstep_init(&s, 0.1f);
    tge_fixedstep_update(&s, 100.0f);
    TGE_ASSERT(tge_fixedstep_pending(&s) == 10, "default max_step clamps to 10 steps");
}

TGE_TEST(negative_dt_is_ignored)
{
    TGE_FixedStep s;
    tge_fixedstep_init(&s, 0.1f);
    tge_fixedstep_update(&s, -5.0f);
    TGE_ASSERT(tge_fixedstep_pending(&s) == 0, "negative dt ignored");
    TGE_ASSERT(!tge_fixedstep_next(&s), "no steps from negative dt");
}

TGE_TEST(reset_clears)
{
    TGE_FixedStep s;
    tge_fixedstep_init(&s, 0.1f);
    tge_fixedstep_update(&s, 1.0f);
    tge_fixedstep_reset(&s);
    TGE_ASSERT(tge_fixedstep_pending(&s) == 0, "reset clears acc");
}

TGE_TEST(invalid_step_disabled)
{
    TGE_FixedStep s;
    tge_fixedstep_init(&s, 0.0f);
    tge_fixedstep_update(&s, 10.0f);
    TGE_ASSERT(!tge_fixedstep_next(&s), "zero step disabled");
    TGE_ASSERT(tge_fixedstep_pending(&s) == 0, "no pending with zero step");
}

TGE_TEST(null_safety)
{
    tge_fixedstep_init(NULL, 0.1f);
    tge_fixedstep_reset(NULL);
    tge_fixedstep_update(NULL, 1.0f);
    TGE_ASSERT(!tge_fixedstep_next(NULL), "NULL next is false");
    TGE_ASSERT(tge_fixedstep_pending(NULL) == 0, "NULL pending is 0");
}

int main(void)
{
    test_steps_at_fixed_interval();
    test_multiple_steps_pending_and_drained();
    test_dt_clamped_to_max_step();
    test_negative_dt_is_ignored();
    test_reset_clears();
    test_invalid_step_disabled();
    test_null_safety();
    return tge_test_report();
}
