#include "fixedstep.h"

void tge_fixedstep_init(TGE_FixedStep *s, float step)
{
    if (!s)
        return;
    s->step = step;
    s->max_step = step > 0.0f ? step * 10.0f : 0.0f;
    s->acc = 0.0f;
}

void tge_fixedstep_reset(TGE_FixedStep *s)
{
    if (!s)
        return;
    s->acc = 0.0f;
}

void tge_fixedstep_update(TGE_FixedStep *s, float dt)
{
    if (!s || s->step <= 0.0f)
        return;
    float add = dt;
    if (add < 0.0f)
        add = 0.0f;
    if (s->max_step > 0.0f && add > s->max_step)
        add = s->max_step;
    s->acc += add;
}

bool tge_fixedstep_next(TGE_FixedStep *s)
{
    if (!s || s->step <= 0.0f)
        return false;
    if (s->acc < s->step)
        return false;
    s->acc -= s->step;
    return true;
}

int tge_fixedstep_pending(const TGE_FixedStep *s)
{
    if (!s || s->step <= 0.0f)
        return 0;
    return (int)(s->acc / s->step);
}
