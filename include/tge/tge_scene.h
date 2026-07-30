#ifndef TGE_SCENE_H_
#define TGE_SCENE_H_

#include <stdbool.h>
#include "tge_canvas.h"
#include "tge_events.h"
#include "tge_app.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TGE_Scene TGE_Scene;

typedef void (*tge_scene_init_fn)(TGE_Scene *scene);
typedef void (*tge_scene_update_fn)(TGE_Scene *scene, float dt);
typedef void (*tge_scene_draw_fn)(TGE_Scene *scene, TGE_Canvas *canvas);
typedef void (*tge_scene_event_fn)(TGE_Scene *scene, TGE_Event *ev);
typedef void (*tge_scene_destroy_fn)(TGE_Scene *scene);

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
void TGE_PopScene(TGE_App *app);
void TGE_ReplaceScene(TGE_App *app, TGE_Scene *scene);

#ifdef __cplusplus
}
#endif

#endif
