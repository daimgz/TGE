#include "tge/tge_scene.h"
#include "tge/tge_app.h"
#include "tge_internal.h"
#include <stdlib.h>

void TGE_PushScene(TGE_App *app, TGE_Scene *scene)
{
    if (!app || !scene)
        return;
    if (app->op_count < TGE_SCENE_OPS_MAX) {
        app->op_queue[app->op_count].type = TGE_SCENE_OP_PUSH;
        app->op_queue[app->op_count].scene = scene;
        app->op_count++;
    }
}

void TGE_PopScene(TGE_App *app)
{
    if (!app)
        return;
    if (app->op_count < TGE_SCENE_OPS_MAX) {
        app->op_queue[app->op_count].type = TGE_SCENE_OP_POP;
        app->op_queue[app->op_count].scene = NULL;
        app->op_count++;
    }
}

void TGE_ReplaceScene(TGE_App *app, TGE_Scene *scene)
{
    if (!app || !scene)
        return;
    if (app->op_count < TGE_SCENE_OPS_MAX) {
        app->op_queue[app->op_count].type = TGE_SCENE_OP_REPLACE;
        app->op_queue[app->op_count].scene = scene;
        app->op_count++;
    }
}

/* A scene built by tge_scene_create is one allocation: the TGE_Scene block
 * followed by the user's destroy callback and the userdata. scene->destroy
 * points here so TGE_PopScene/TGE_ReplaceScene (which call `destroy`) free
 * everything; tge_scene_destroy uses it too. */
typedef struct {
    TGE_Scene scene;
    tge_scene_destroy_fn user_destroy;
} TGE_SceneBlock;

static void tge_scene_block_destroy(TGE_Scene *scene)
{
    TGE_SceneBlock *block = (TGE_SceneBlock *)scene;
    if (block->user_destroy)
        block->user_destroy(scene);
    free(block);
}

void *tge_scene_create(TGE_Scene **out, size_t userdata_size,
                       tge_scene_update_fn update, tge_scene_draw_fn draw,
                       tge_scene_event_fn event, tge_scene_destroy_fn destroy)
{
    if (!out)
        return NULL;
    *out = NULL;
    TGE_SceneBlock *block =
        calloc(1, sizeof(TGE_SceneBlock) + userdata_size);
    if (!block)
        return NULL;
    block->user_destroy = destroy;
    block->scene.opaque = true;
    block->scene.update = update;
    block->scene.draw = draw;
    block->scene.event = event;
    block->scene.destroy = tge_scene_block_destroy;
    *out = &block->scene;
    if (userdata_size > 0) {
        void *ud = (char *)(block + 1);
        block->scene.userdata = ud;
        return ud;
    }
    return NULL;
}

void tge_scene_destroy(TGE_Scene *scene)
{
    if (!scene)
        return;
    if (scene->destroy)
        scene->destroy(scene);
    else
        free(scene);
}

void tge_app_process_scene_ops(TGE_App *app)
{
    for (int i = 0; i < app->op_count; i++) {
        TGE_SceneOp *op = &app->op_queue[i];
        switch (op->type) {
        case TGE_SCENE_OP_PUSH:
            if (app->scene_count < TGE_SCENE_MAX) {
                app->scenes[app->scene_count++] = op->scene;
                if (op->scene->init)
                    op->scene->init(op->scene);
            }
            break;
        case TGE_SCENE_OP_POP:
            if (app->scene_count > 0) {
                app->scene_count--;
                TGE_Scene *popped = app->scenes[app->scene_count];
                if (popped->destroy)
                    popped->destroy(popped);
            }
            break;
        case TGE_SCENE_OP_REPLACE:
            if (app->scene_count > 0) {
                app->scene_count--;
                TGE_Scene *old = app->scenes[app->scene_count];
                if (old->destroy)
                    old->destroy(old);
            }
            if (app->scene_count < TGE_SCENE_MAX) {
                app->scenes[app->scene_count++] = op->scene;
                if (op->scene->init)
                    op->scene->init(op->scene);
            }
            break;
        }
    }
    app->op_count = 0;
}
