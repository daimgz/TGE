#include "tge-extra/game.h"

static void update_trampoline(TGE_Scene *scene, float dt)
{
    TGE_GameContext *ctx = (TGE_GameContext *)scene->userdata;
    ctx->callbacks->update(ctx, dt);
}

static void draw_trampoline(TGE_Scene *scene, TGE_Canvas *canvas)
{
    TGE_GameContext *ctx = (TGE_GameContext *)scene->userdata;
    ctx->callbacks->draw(ctx, canvas);
}

static void event_trampoline(TGE_Scene *scene, TGE_Event *ev)
{
    TGE_GameContext *ctx = (TGE_GameContext *)scene->userdata;
    ctx->callbacks->event(ctx, ev);
}

static void destroy_trampoline(TGE_Scene *scene)
{
    TGE_GameContext *ctx = (TGE_GameContext *)scene->userdata;
    ctx->callbacks->destroy(ctx);
}

void *tge_game_scene_create(TGE_App *app, TGE_Scene **out, size_t game_size,
                            const TGE_GameCallbacks *callbacks)
{
    if (!callbacks)
        return NULL;
    if (game_size < sizeof(TGE_GameContext))
        return NULL;

    void *instance = tge_scene_create(out, game_size,
                                      callbacks->update ? update_trampoline
                                                        : NULL,
                                      callbacks->draw ? draw_trampoline : NULL,
                                      callbacks->event ? event_trampoline
                                                       : NULL,
                                      callbacks->destroy ? destroy_trampoline
                                                         : NULL);
    if (!instance)
        return NULL;

    TGE_GameContext *ctx = (TGE_GameContext *)instance;
    ctx->app = app;
    ctx->instance = instance;
    ctx->callbacks = callbacks;
    return instance;
}
