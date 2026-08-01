#include "tge/tge_scene.h"
#include "tge/tge_app.h"
#include "tge/tge_canvas.h"
#include "tge/tge_events.h"
#include "tge_internal.h"
#include "tge_test.h"
#include "mock_backend.h"

#include <stdlib.h>

typedef struct {
    int id;
    int events;
    int updates;
    int draws;
    int last_event;
    int last_key;
} Rec;

static int g_trace[8];
static int g_trace_count = 0;
static int g_inits = 0;
static TGE_App *g_app = NULL;
static TGE_Scene *g_scene_b = NULL;

static TGE_Scene *make_scene(int id)
{
    TGE_Scene *sc = (TGE_Scene *)calloc(1, sizeof(TGE_Scene));
    Rec *r = (Rec *)calloc(1, sizeof(Rec));
    r->id = id;
    sc->userdata = r;
    return sc;
}

static TGE_App *make_test_app(MockData **md)
{
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, 10, 5);
    TGE_App *app = tge_app_create_with_runtime(rt, 10, 5);
    app->fps = 0;
    return app;
}

static void sc_init(TGE_Scene *sc)
{
    (void)sc;
    g_inits++;
}

static void sc_update(TGE_Scene *sc, float dt)
{
    (void)dt;
    ((Rec *)sc->userdata)->updates++;
}

static void sc_draw(TGE_Scene *sc, TGE_Canvas *canvas)
{
    (void)canvas;
    g_trace[g_trace_count++] = ((Rec *)sc->userdata)->id;
}

static void sc_event(TGE_Scene *sc, TGE_Event *ev)
{
    Rec *r = (Rec *)sc->userdata;
    r->events++;
    r->last_event = ev->type;
    if (ev->type == TGE_EVENT_KEYDOWN)
        r->last_key = ev->data.key.keycode;
}

static void update_pushes_b(TGE_Scene *sc, float dt)
{
    (void)sc;
    (void)dt;
    TGE_PushScene(g_app, g_scene_b);
}

static void event_pops(TGE_Scene *sc, TGE_Event *ev)
{
    (void)sc;
    (void)ev;
    TGE_PopScene(g_app);
}

TGE_TEST(push_pop_replace_deferred)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    TGE_Scene *b = make_scene(2);

    TGE_PushScene(app, a);
    TGE_ASSERT(app->scene_count == 0, "push deferred");

    tge_app_process_scene_ops(app);
    TGE_ASSERT(app->scene_count == 1 && app->scenes[0] == a, "push applied");

    TGE_PopScene(app);
    tge_app_process_scene_ops(app);
    TGE_ASSERT(app->scene_count == 0, "pop applied");

    TGE_ReplaceScene(app, b);
    tge_app_process_scene_ops(app);
    TGE_ASSERT(app->scene_count == 1 && app->scenes[0] == b, "replace applied");

    TGE_Destroy(app);
}

TGE_TEST(init_called_on_apply)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    a->init = sc_init;
    g_inits = 0;
    TGE_PushScene(app, a);
    TGE_ASSERT(g_inits == 0, "init not called before apply");
    tge_app_process_scene_ops(app);
    TGE_ASSERT(g_inits == 1, "init called once on apply");
    TGE_Destroy(app);
}

TGE_TEST(top_scene_updated_and_drawn)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    a->update = sc_update;
    a->draw = sc_draw;
    TGE_PushScene(app, a);
    tge_app_process_scene_ops(app);

    Rec *r = (Rec *)a->userdata;
    TGE_ASSERT(r->updates == 0, "no updates yet");
    g_trace_count = 0;
    tge_app_frame(app);
    TGE_ASSERT(r->updates == 1, "top scene updated");
    TGE_ASSERT(g_trace_count == 1 && g_trace[0] == 1, "top scene drawn");
    TGE_Destroy(app);
}

TGE_TEST(draw_bottom_to_top_opaque_stops)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    TGE_Scene *b = make_scene(2);
    TGE_Scene *c = make_scene(3);
    a->draw = sc_draw;
    b->draw = sc_draw;
    b->opaque = true;
    c->draw = sc_draw;
    TGE_PushScene(app, a);
    TGE_PushScene(app, b);
    TGE_PushScene(app, c);
    tge_app_process_scene_ops(app);

    g_trace_count = 0;
    tge_app_frame(app);
    TGE_ASSERT(g_trace_count == 2, "c hidden by opaque b");
    TGE_ASSERT(g_trace[0] == 1 && g_trace[1] == 2, "a then b, bottom to top");
    TGE_Destroy(app);
}

TGE_TEST(transparent_scene_does_not_hide)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    TGE_Scene *b = make_scene(2);
    a->draw = sc_draw;
    b->draw = sc_draw;
    b->opaque = false;
    TGE_PushScene(app, a);
    TGE_PushScene(app, b);
    tge_app_process_scene_ops(app);

    g_trace_count = 0;
    tge_app_frame(app);
    TGE_ASSERT(g_trace_count == 2, "both drawn when transparent");
    TGE_ASSERT(g_trace[0] == 1 && g_trace[1] == 2, "a below, b above");
    TGE_Destroy(app);
}

TGE_TEST(push_during_update_applied_after_frame)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    g_scene_b = make_scene(2);
    a->update = update_pushes_b;
    a->draw = sc_draw;
    g_scene_b->draw = sc_draw;
    g_app = app;
    TGE_PushScene(app, a);
    tge_app_process_scene_ops(app);

    g_trace_count = 0;
    tge_app_frame(app);
    TGE_ASSERT(app->scene_count == 2, "push applied at end of frame");
    TGE_ASSERT(g_trace_count == 1, "only a drawn during that frame");

    g_trace_count = 0;
    tge_app_frame(app);
    TGE_ASSERT(g_trace_count == 2, "a and b drawn next frame");
    TGE_Destroy(app);
}

TGE_TEST(events_go_to_top_scene)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    TGE_Scene *b = make_scene(2);
    a->event = sc_event;
    b->event = sc_event;
    TGE_PushScene(app, a);
    TGE_PushScene(app, b);
    tge_app_process_scene_ops(app);

    mock_set_input(m, "\x1b[A", 3);
    tge_app_frame(app);
    Rec *ra = (Rec *)a->userdata;
    Rec *rb = (Rec *)b->userdata;
    TGE_ASSERT(ra->events == 0, "bottom scene gets no events");
    TGE_ASSERT(rb->events == 1, "top scene receives event");
    TGE_ASSERT(rb->last_event == TGE_EVENT_KEYDOWN, "keydown");
    TGE_ASSERT(rb->last_key == TGE_KEY_UP, "up arrow");
    TGE_Destroy(app);
}

TGE_TEST(pop_during_event_applied_after_frame)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    TGE_Scene *a = make_scene(1);
    TGE_Scene *b = make_scene(2);
    b->event = event_pops;
    g_app = app;
    TGE_PushScene(app, a);
    TGE_PushScene(app, b);
    tge_app_process_scene_ops(app);

    mock_set_input(m, "x", 1);
    tge_app_frame(app);
    TGE_ASSERT(app->scene_count == 1, "pop applied at end of frame");
    TGE_ASSERT(app->scenes[0] == a, "a remains");
    TGE_Destroy(app);
}

TGE_TEST(resize_resizes_canvases)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    mock_set_input(m, "\x1b[8;80;24t", 10);
    tge_app_frame(app);
    TGE_ASSERT(tge_canvas_width(app->current) == 80, "current width");
    TGE_ASSERT(tge_canvas_height(app->current) == 24, "current height");
    TGE_ASSERT(tge_canvas_width(app->previous) == 80, "previous width");
    TGE_ASSERT(tge_runtime_width(app->runtime) == 80, "runtime width");
    TGE_Destroy(app);
}

TGE_TEST(app_callbacks_when_no_scenes)
{
    MockData *m;
    TGE_App *app = make_test_app(&m);
    int cb_updates = 0;
    int cb_draws = 0;
    int cb_events = 0;
    app->update_cb = NULL;
    app->draw_cb = NULL;
    app->event_cb = NULL;
    tge_app_frame(app);
    TGE_ASSERT(cb_updates == 0 && cb_draws == 0 && cb_events == 0,
               "no callbacks, no crash");
    TGE_Destroy(app);
}

int main(void)
{
    test_push_pop_replace_deferred();
    test_init_called_on_apply();
    test_top_scene_updated_and_drawn();
    test_draw_bottom_to_top_opaque_stops();
    test_transparent_scene_does_not_hide();
    test_push_during_update_applied_after_frame();
    test_events_go_to_top_scene();
    test_pop_during_event_applied_after_frame();
    test_resize_resizes_canvases();
    test_app_callbacks_when_no_scenes();
    return tge_test_report();
}
