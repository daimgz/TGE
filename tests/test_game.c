#include "tge-extra/game.h"

#include "tge/tge_app.h"
#include "tge/tge_events.h"
#include "tge_internal.h"
#include "tge_test.h"
#include "mock_backend.h"

#include <stdlib.h>

typedef struct {
    TGE_GameContext ctx;
    int updates;
    int draws;
    int events;
    int last_event;
    int last_key;
} TestGame;

static int g_destroys = 0;

static TGE_App *make_test_app(MockData **md)
{
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, 10, 5);
    TGE_App *app = tge_app_create_with_runtime(rt, 10, 5);
    app->fps = 0;
    return app;
}

static void tg_update(TGE_GameContext *ctx, float dt)
{
    (void)dt;
    TestGame *game = (TestGame *)tge_game_instance(ctx);
    game->updates++;
}

static void tg_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    (void)canvas;
    TestGame *game = (TestGame *)tge_game_instance(ctx);
    game->draws++;
}

static void tg_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    TestGame *game = (TestGame *)tge_game_instance(ctx);
    game->events++;
    game->last_event = ev->type;
    if (ev->type == TGE_EVENT_KEYDOWN)
        game->last_key = ev->data.key.keycode;
}

static void tg_destroy(TGE_GameContext *ctx)
{
    (void)ctx;
    g_destroys++;
}

static const TGE_GameCallbacks full_callbacks = {
    tg_update, tg_draw, tg_event, tg_destroy,
};

TGE_TEST(create_fills_context)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *sc = NULL;
    TestGame *game = (TestGame *)tge_game_scene_create(
        app, &sc, sizeof(TestGame), &full_callbacks);
    TGE_ASSERT(sc != NULL, "scene out");
    TGE_ASSERT(game != NULL && game == sc->userdata, "instance is userdata");
    TGE_ASSERT(sc->opaque == true, "opaque by default");
    TGE_ASSERT(game->ctx.app == app, "ctx.app filled");
    TGE_ASSERT(game->ctx.instance == game, "ctx.instance is the instance");
    TGE_ASSERT(game->ctx.instance == &game->ctx, "ctx == instance at offset 0");
    TGE_ASSERT(game->ctx.callbacks == &full_callbacks, "callbacks stored");
    TGE_ASSERT(sc->update != NULL && sc->draw != NULL && sc->event != NULL &&
               sc->destroy != NULL, "all trampolines wired");
    g_destroys = 0;
    tge_scene_destroy(sc);
    TGE_ASSERT(g_destroys == 1, "destroy trampoline forwards");
    TGE_Destroy(app);
}

TGE_TEST(forward_update_draw_event)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *sc = NULL;
    TestGame *game = (TestGame *)tge_game_scene_create(
        app, &sc, sizeof(TestGame), &full_callbacks);
    TGE_PushScene(app, sc);
    tge_app_process_scene_ops(app);

    TGE_Event ev;
    ev.type = TGE_EVENT_KEYDOWN;
    ev.data.key.keycode = 65; /* 'A' */
    TGE_PushEvent(app, &ev);
    tge_app_frame(app);

    TGE_ASSERT(game->updates == 1, "update forwarded");
    TGE_ASSERT(game->draws == 1, "draw forwarded");
    TGE_ASSERT(game->events == 1, "event forwarded");
    TGE_ASSERT(game->last_event == TGE_EVENT_KEYDOWN, "event type delivered");
    TGE_ASSERT(game->last_key == 65, "event data delivered");
    TGE_Destroy(app);
}

TGE_TEST(destroy_forwarded_on_pop)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *sc = NULL;
    tge_game_scene_create(app, &sc, sizeof(TestGame), &full_callbacks);
    g_destroys = 0;
    TGE_PushScene(app, sc);
    tge_app_process_scene_ops(app);
    TGE_PopScene(app);
    tge_app_process_scene_ops(app);
    TGE_ASSERT(g_destroys == 1, "destroy forwarded on pop");
    TGE_Destroy(app);
}

TGE_TEST(update_null_not_wired)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    static const TGE_GameCallbacks cbs = { NULL, tg_draw, tg_event, tg_destroy };
    TGE_Scene *sc = NULL;
    tge_game_scene_create(app, &sc, sizeof(TestGame), &cbs);
    TGE_ASSERT(sc != NULL && sc->update == NULL, "update trampoline skipped");
    TGE_ASSERT(sc->draw != NULL && sc->event != NULL && sc->destroy != NULL,
               "other callbacks wired");
    tge_scene_destroy(sc);
    TGE_Destroy(app);
}

TGE_TEST(draw_null_not_wired)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    static const TGE_GameCallbacks cbs = { tg_update, NULL, tg_event,
                                           tg_destroy };
    TGE_Scene *sc = NULL;
    tge_game_scene_create(app, &sc, sizeof(TestGame), &cbs);
    TGE_ASSERT(sc != NULL && sc->draw == NULL, "draw trampoline skipped");
    TGE_ASSERT(sc->update != NULL && sc->event != NULL && sc->destroy != NULL,
               "other callbacks wired");
    tge_scene_destroy(sc);
    TGE_Destroy(app);
}

TGE_TEST(event_null_not_wired)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    static const TGE_GameCallbacks cbs = { tg_update, tg_draw, NULL,
                                           tg_destroy };
    TGE_Scene *sc = NULL;
    tge_game_scene_create(app, &sc, sizeof(TestGame), &cbs);
    TGE_ASSERT(sc != NULL && sc->event == NULL, "event trampoline skipped");
    TGE_ASSERT(sc->update != NULL && sc->draw != NULL && sc->destroy != NULL,
               "other callbacks wired");
    tge_scene_destroy(sc);
    TGE_Destroy(app);
}

TGE_TEST(destroy_null_not_wired)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    static const TGE_GameCallbacks cbs = { tg_update, tg_draw, tg_event, NULL };
    TGE_Scene *sc = NULL;
    tge_game_scene_create(app, &sc, sizeof(TestGame), &cbs);
    TGE_ASSERT(sc != NULL, "scene created");
    TGE_ASSERT(sc->update != NULL && sc->draw != NULL && sc->event != NULL,
               "other callbacks wired");
    g_destroys = 0;
    tge_scene_destroy(sc);
    TGE_ASSERT(g_destroys == 0, "no user destroy without callback");
    TGE_Destroy(app);
}

TGE_TEST(game_size_too_small_rejected)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *sc = NULL;
    void *instance = tge_game_scene_create(
        app, &sc, sizeof(TGE_GameContext) - 1, &full_callbacks);
    TGE_ASSERT(instance == NULL, "too-small game rejected");
    TGE_ASSERT(sc == NULL, "scene untouched on rejection");
    TGE_Destroy(app);
}

TGE_TEST(game_create_pushes_scene)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_GameContext *ctx = tge_game_create(app, sizeof(TestGame),
                                           &full_callbacks);
    tge_app_process_scene_ops(app);
    TGE_ASSERT(ctx != NULL, "context returned");
    TGE_ASSERT(ctx->instance == ctx, "offset-0 context");
    TGE_ASSERT(ctx->callbacks == &full_callbacks, "callbacks stored");
    TGE_ASSERT(app->scene_count == 1, "scene pushed");
    TGE_ASSERT(app->scenes[0] != NULL && app->scenes[0]->userdata == ctx,
               "game scene on top");
    TGE_ASSERT(app->scenes[0]->update != NULL && app->scenes[0]->draw != NULL &&
               app->scenes[0]->event != NULL && app->scenes[0]->destroy != NULL,
               "trampolines wired");
    TGE_Destroy(app);
}

TGE_TEST(game_create_rejects_null_callbacks)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_GameContext *ctx = tge_game_create(app, sizeof(TestGame), NULL);
    TGE_ASSERT(ctx == NULL, "null callbacks rejected");
    tge_app_process_scene_ops(app);
    TGE_ASSERT(app->scene_count == 0, "nothing pushed on rejection");
    TGE_Destroy(app);
}

int main(void)
{
    test_create_fills_context();
    test_forward_update_draw_event();
    test_destroy_forwarded_on_pop();
    test_update_null_not_wired();
    test_draw_null_not_wired();
    test_event_null_not_wired();
    test_destroy_null_not_wired();
    test_game_size_too_small_rejected();
    test_game_create_pushes_scene();
    test_game_create_rejects_null_callbacks();
    return tge_test_report();
}
