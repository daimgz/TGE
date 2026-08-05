#include "timer.h"

void tge_timer_init(TGE_Timer *t, float interval)
{
    if (!t)
        return;
    t->interval = interval;
    t->acc = 0.0f;
}

void tge_timer_reset(TGE_Timer *t)
{
    if (!t)
        return;
    t->acc = 0.0f;
}

void tge_timer_update(TGE_Timer *t, float dt)
{
    if (!t || t->interval <= 0.0f)
        return;
    t->acc += dt;
}

bool tge_timer_tick(TGE_Timer *t)
{
    if (!t || t->interval <= 0.0f)
        return false;
    if (t->acc < t->interval)
        return false;
    t->acc -= t->interval;
    return true;
}
