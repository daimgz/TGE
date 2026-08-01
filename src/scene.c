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
            if (app->scene_count > 0)
                app->scene_count--;
            break;
        case TGE_SCENE_OP_REPLACE:
            if (app->scene_count > 0)
                app->scene_count--;
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
