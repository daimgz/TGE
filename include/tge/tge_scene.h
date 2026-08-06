#ifndef TGE_SCENE_H_
#define TGE_SCENE_H_

#include <stdbool.h>
#include <stddef.h>
#include "tge_canvas.h"
#include "tge_events.h"
#include "tge_app.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Scene stack: push/pop/replace manage a stack of scenes on the app. Only
 * the top scene receives update and event callbacks. Draw runs from the
 * bottom of the stack upward, stopping at the first scene marked `opaque`. */
typedef struct TGE_Scene TGE_Scene;

typedef void (*tge_scene_init_fn)(TGE_Scene *scene);
typedef void (*tge_scene_update_fn)(TGE_Scene *scene, float dt);
typedef void (*tge_scene_draw_fn)(TGE_Scene *scene, TGE_Canvas *canvas);
typedef void (*tge_scene_event_fn)(TGE_Scene *scene, TGE_Event *ev);
typedef void (*tge_scene_destroy_fn)(TGE_Scene *scene);

/* Fill in the callbacks you need; unused ones may be NULL. `userdata` is a
 * free slot for your own state. When a scene is popped or replaced the
 * engine calls `destroy` (if set) so you can free resources. */
struct TGE_Scene {
    bool opaque;
    void *userdata;
    tge_scene_init_fn    init;
    tge_scene_update_fn  update;
    tge_scene_draw_fn    draw;
    tge_scene_event_fn   event;
    tge_scene_destroy_fn destroy;
};

void TGE_PushScene(TGE_App *app, TGE_Scene *scene);
/* Pops the top scene (destroying it) and returns to the previous one. */
void TGE_PopScene(TGE_App *app);
/* Replaces the top scene with `scene` (destroying the old one). */
void TGE_ReplaceScene(TGE_App *app, TGE_Scene *scene);

/* Convenience constructor for heap-allocated scenes. Allocates the scene and
 * a zeroed `userdata` block of `userdata_size` bytes in one call, wires the
 * callbacks (any may be NULL) and marks the scene opaque. Returns the
 * userdata pointer (NULL when `userdata_size` is 0); the scene is stored in
 * *out. The `destroy` callback only frees deeper resources (the block itself
 * is owned by the scene). Hand this scene to TGE_PushScene/TGE_ReplaceScene
 * or to tge_scene_destroy; either path frees everything.
 *
 * Ownership: while a scene is on the app's stack the app owns it. Pushing a
 * scene hands the ownership to the app. TGE_PopScene and TGE_ReplaceScene
 * destroy the scene they remove, and TGE_Destroy destroys any scene still on
 * the stack. Scenes built manually (destroy == NULL) are never freed by the
 * engine and remain the caller's responsibility. */
void *tge_scene_create(TGE_Scene **out, size_t userdata_size,
                       tge_scene_update_fn update, tge_scene_draw_fn draw,
                       tge_scene_event_fn event, tge_scene_destroy_fn destroy);

/* Frees a scene and its userdata. Calls the scene's `destroy` callback first
 * (which must NOT free the scene or its userdata), falling back to a plain
 * free for scenes built manually. No-op on NULL. */
void tge_scene_destroy(TGE_Scene *scene);

#ifdef __cplusplus
}
#endif

#endif
