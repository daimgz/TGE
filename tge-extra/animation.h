#ifndef TGE_EXTRA_ANIMATION_H_
#define TGE_EXTRA_ANIMATION_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Interpolation curves applied to the timeline position [0,1]. */
typedef enum {
    TGE_EASE_LINEAR,
    TGE_EASE_IN,     /* starts slow, ends fast */
    TGE_EASE_OUT,    /* starts fast, ends slow */
    TGE_EASE_IN_OUT  /* slow at both ends */
} TGE_Ease;

typedef struct TGE_Anim TGE_Anim;

/* Tweens a float from `from` to `to` over `duration` seconds using `ease`.
 * A duration <= 0 behaves as zero-duration: the anim finishes instantly when
 * played (value == to, finished == true).
 * Initial state: not playing, not finished, progress 0, value == from. */
TGE_Anim *tge_anim_create(float from, float to, float duration, TGE_Ease ease);
void      tge_anim_destroy(TGE_Anim *anim);

/* Rewinds to t=0 and starts playing. */
void tge_anim_play(TGE_Anim *anim);
/* Rewinds to t=0 without starting. */
void tge_anim_reset(TGE_Anim *anim);
/* Pauses at the current position; progress and value are kept. */
void tge_anim_stop(TGE_Anim *anim);
/* When set, the anim loops forever and never reports finished. */
void tge_anim_set_loop(TGE_Anim *anim, bool loop);

/* Advances the anim by dt seconds (no-op while stopped). */
void tge_anim_update(TGE_Anim *anim, float dt);

/* Current interpolated value in [from, to]. */
float tge_anim_value(const TGE_Anim *anim);
/* Raw timeline position in [0,1], before easing. */
float tge_anim_progress(const TGE_Anim *anim);
bool  tge_anim_playing(const TGE_Anim *anim);
bool  tge_anim_finished(const TGE_Anim *anim);

#ifdef __cplusplus
}
#endif

#endif
