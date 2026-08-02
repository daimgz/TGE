#include "animation.h"

#include <stdlib.h>

struct TGE_Anim {
    float from;
    float to;
    float duration;
    TGE_Ease ease;
    float elapsed;
    bool playing;
    bool finished;
    bool loop;
};

static float ease_value(TGE_Ease ease, float t)
{
    switch (ease) {
    case TGE_EASE_IN:
        return t * t;
    case TGE_EASE_OUT:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case TGE_EASE_IN_OUT:
        return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    case TGE_EASE_LINEAR:
    default:
        return t;
    }
}

TGE_Anim *tge_anim_create(float from, float to, float duration, TGE_Ease ease)
{
    TGE_Anim *anim = (TGE_Anim *)calloc(1, sizeof(*anim));
    if (!anim)
        return NULL;
    anim->from = from;
    anim->to = to;
    anim->duration = duration;
    anim->ease = ease;
    return anim;
}

void tge_anim_destroy(TGE_Anim *anim)
{
    free(anim);
}

void tge_anim_play(TGE_Anim *anim)
{
    if (!anim)
        return;
    anim->playing = true;
    anim->finished = false;
    anim->elapsed = 0.0f;
    if (anim->duration <= 0.0f)
        anim->finished = !anim->loop; /* zero-duration: instantly done unless looping */
}

void tge_anim_reset(TGE_Anim *anim)
{
    if (!anim)
        return;
    anim->playing = false;
    anim->finished = false;
    anim->elapsed = 0.0f;
}

void tge_anim_stop(TGE_Anim *anim)
{
    if (!anim)
        return;
    anim->playing = false;
}

void tge_anim_set_loop(TGE_Anim *anim, bool loop)
{
    if (!anim)
        return;
    anim->loop = loop;
}

void tge_anim_update(TGE_Anim *anim, float dt)
{
    if (!anim || !anim->playing)
        return;
    if (anim->duration <= 0.0f) {
        anim->finished = !anim->loop;
        return;
    }
    anim->elapsed += dt;
    while (anim->elapsed >= anim->duration) {
        if (anim->loop) {
            anim->elapsed -= anim->duration;
        } else {
            anim->elapsed = anim->duration;
            anim->playing = false;
            anim->finished = true;
            break;
        }
    }
}

float tge_anim_value(const TGE_Anim *anim)
{
    if (!anim)
        return 0.0f;
    float p = tge_anim_progress(anim);
    return anim->from + (anim->to - anim->from) * ease_value(anim->ease, p);
}

float tge_anim_progress(const TGE_Anim *anim)
{
    if (!anim)
        return 0.0f;
    if (anim->duration <= 0.0f)
        return 1.0f;
    float p = anim->elapsed / anim->duration;
    if (p < 0.0f)
        return 0.0f;
    if (p > 1.0f)
        return 1.0f;
    return p;
}

bool tge_anim_playing(const TGE_Anim *anim)
{
    return anim && anim->playing;
}

bool tge_anim_finished(const TGE_Anim *anim)
{
    return anim && anim->finished;
}
