#include "tge/tge_app.h"
#include "tge/tge_canvas.h"
#include "tge/tge_runtime.h"
#include "tge/tge_scene.h"
#include "tge_internal.h"
#include "tge_test.h"
#include "mock_backend.h"

#include <string.h>

static int g_received = 0;
static uint32_t g_codepoint = 0;
static int g_draws = 0;
static int g_updates = 0;

static void event_cb(TGE_App *app, TGE_Event *ev)
{
    (void)app;
    g_received++;
    if (ev->type == TGE_EVENT_TEXT)
        g_codepoint = ev->data.text.codepoint;
}

static TGE_App *make_test_app(MockData **md)
{
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, 10, 5);
    TGE_App *app = tge_app_create_with_runtime(rt, 10, 5);
    app->fps = 0;
    return app;
}

TGE_TEST(pushevent_dispatched_to_event_cb)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    app->event_cb = event_cb;
    g_received = 0;
    g_codepoint = 0;
    TGE_Event ev;
    ev.type = TGE_EVENT_TEXT;
    ev.data.text.codepoint = 'X';
    TGE_PushEvent(app, &ev);
    tge_app_frame(app);
    TGE_ASSERT(g_received == 1, "synthetic event dispatched");
    TGE_ASSERT(g_codepoint == 'X', "codepoint delivered");
    TGE_Destroy(app);
}

TGE_TEST(pushevent_overflow_drops_extras)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    app->event_cb = event_cb;
    g_received = 0;
    TGE_Event ev;
    ev.type = TGE_EVENT_TEXT;
    ev.data.text.codepoint = 'Z';
    for (int i = 0; i < TGE_APP_EVENT_PENDING * 2; i++)
        TGE_PushEvent(app, &ev);
    tge_app_frame(app);
    TGE_ASSERT(g_received == TGE_APP_EVENT_PENDING, "only pending cap delivered");
    TGE_Destroy(app);
}

TGE_TEST(setfps_and_settitle)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_SetFPS(app, 30.0f);
    TGE_ASSERT(app->fps == 30.0f, "fps set to 30");
    TGE_SetFPS(app, 0.0f);
    TGE_ASSERT(app->fps == 0.0f, "fps set to 0 (uncapped)");
    TGE_SetTitle(app, "hello");
    TGE_ASSERT(strcmp(m->title, "hello") == 0, "title routed to backend");
    TGE_SetTitle(app, NULL);
    TGE_ASSERT(strcmp(m->title, "hello") == 0, "NULL title ignored");
    TGE_SetFPS(NULL, 60.0f);
    TGE_SetTitle(NULL, "x");
    TGE_PushEvent(NULL, NULL);
    TGE_Destroy(app);
}

TGE_TEST(quit_event_via_pushevent)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    app->event_cb = event_cb;
    g_received = 0;
    TGE_Event ev;
    ev.type = TGE_EVENT_QUIT;
    TGE_PushEvent(app, &ev);
    tge_app_frame(app);
    TGE_ASSERT(app->quit == true, "synthetic quit stops app");
    TGE_Destroy(app);
}

TGE_TEST(resize_forces_full_repaint)
{
    MockData *m;
    TGE_App *app = make_test_app(&m); /* starts 10x5 */
    mock_set_input(m, "\x1b[8;4;8t", 8); /* XTerm report -> resize to 8x4 */
    tge_app_frame(app);
    TGE_ASSERT(app->current->width == 8 && app->current->height == 4,
               "canvas resized to 8x4");
    TGE_ASSERT(m->presented >= 1, "frame presented");
    TGE_ASSERT(m->captured_count == 8 * 4, "full repaint after resize");
    TGE_Destroy(app);
}

static void update_quit_after_3(TGE_App *app, float dt)
{
    (void)dt;
    g_updates++;
    if (g_updates >= 3)
        TGE_Quit(app);
}

static void draw_counter(TGE_App *app, TGE_Canvas *canvas)
{
    (void)app;
    (void)canvas;
    g_draws++;
}

static int g_scene_destroyed = 0;

static void scene_destroy_cb(TGE_Scene *scene)
{
    (void)scene;
    g_scene_destroyed++;
}

TGE_TEST(step_runs_full_pipeline)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    app->event_cb = event_cb;
    app->draw_cb = draw_counter;
    g_received = 0;
    g_draws = 0;
    TGE_Event ev;
    ev.type = TGE_EVENT_TEXT;
    ev.data.text.codepoint = 'Q';
    TGE_PushEvent(app, &ev);
    TGE_Step(app);
    TGE_ASSERT(g_received == 1, "Step dispatched queued event");
    TGE_ASSERT(g_draws == 1, "Step ran draw");
    TGE_ASSERT(m->presented >= 1, "Step presented the frame");

    g_scene_destroyed = 0;
    TGE_Scene *scene;
    tge_scene_create(&scene, 0, NULL, NULL, NULL, scene_destroy_cb);
    TGE_ASSERT(scene != NULL, "scene created");
    TGE_PushScene(app, scene);
    TGE_PopScene(app);
    TGE_Step(app);
    TGE_ASSERT(app->scene_count == 0, "Step applied scene ops at end of frame");
    TGE_ASSERT(g_scene_destroyed == 1, "Step processed the queued pop");
    TGE_Destroy(app);
}

TGE_TEST(run_equivalent_to_step_loop)
{
    /* TGE_Run must be observably the repetition of TGE_Step. */
    MockData *m;
    TGE_App *app = make_test_app(&m);
    g_updates = 0;
    g_draws = 0;
    TGE_Run(app, NULL, update_quit_after_3, draw_counter, NULL);
    int run_updates = g_updates;
    int run_draws = g_draws;
    int run_presented = m->presented;
    TGE_Destroy(app);

    MockData *m2;
    TGE_App *app2 = make_test_app(&m2);
    g_updates = 0;
    g_draws = 0;
    app2->update_cb = update_quit_after_3;
    app2->draw_cb = draw_counter;
    app2->quit = false;
    while (!app2->quit)
        TGE_Step(app2);
    TGE_ASSERT(g_updates == run_updates, "Step loop ran same updates as Run");
    TGE_ASSERT(g_draws == run_draws, "Step loop ran same draws as Run");
    TGE_ASSERT(m2->presented == run_presented, "Step loop presented same frames as Run");
    TGE_Destroy(app2);
}

TGE_TEST(step_quit_stops_loop)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    g_updates = 0;
    app->update_cb = update_quit_after_3;
    app->quit = false;
    while (!app->quit)
        TGE_Step(app);
    TGE_ASSERT(g_updates == 3, "quit honored by Step loop");
    TGE_Step(NULL);
    TGE_Destroy(app);
}

TGE_TEST(userdata_slot_roundtrip)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    int marker = 42;
    int other = 7;
    TGE_ASSERT(TGE_GetUserData(app) == NULL, "userdata starts NULL");
    TGE_SetUserData(app, &marker);
    TGE_ASSERT(TGE_GetUserData(app) == &marker, "userdata read back");
    TGE_SetUserData(app, &other);
    TGE_ASSERT(TGE_GetUserData(app) == &other, "userdata overwritten");
    TGE_SetUserData(app, NULL);
    TGE_ASSERT(TGE_GetUserData(app) == NULL, "userdata cleared");
    TGE_SetUserData(NULL, &marker);
    TGE_ASSERT(TGE_GetUserData(NULL) == NULL, "NULL app is safe");
    TGE_Destroy(app);
}

static void *g_seen_userdata = NULL;

static void userdata_event_cb(TGE_App *app, TGE_Event *ev)
{
    (void)ev;
    g_seen_userdata = TGE_GetUserData(app);
}

TGE_TEST(userdata_visible_from_callback)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    int marker = 99;
    app->event_cb = userdata_event_cb;
    TGE_SetUserData(app, &marker);
    g_seen_userdata = NULL;
    TGE_Event ev;
    ev.type = TGE_EVENT_TEXT;
    ev.data.text.codepoint = 'U';
    TGE_PushEvent(app, &ev);
    TGE_Step(app);
    TGE_ASSERT(g_seen_userdata == &marker,
               "callback read userdata via TGE_GetUserData");
    TGE_Destroy(app);
}

int main(void)
{
    test_pushevent_dispatched_to_event_cb();
    test_pushevent_overflow_drops_extras();
    test_setfps_and_settitle();
    test_quit_event_via_pushevent();
    test_resize_forces_full_repaint();
    test_step_runs_full_pipeline();
    test_run_equivalent_to_step_loop();
    test_step_quit_stops_loop();
    test_userdata_slot_roundtrip();
    test_userdata_visible_from_callback();
    return tge_test_report();
}
