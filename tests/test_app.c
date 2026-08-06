#include "tge/tge_app.h"
#include "tge/tge_canvas.h"
#include "tge/tge_runtime.h"
#include "tge_internal.h"
#include "tge_test.h"
#include "mock_backend.h"

#include <string.h>

static int g_received = 0;
static uint32_t g_codepoint = 0;

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

int main(void)
{
    test_pushevent_dispatched_to_event_cb();
    test_pushevent_overflow_drops_extras();
    test_setfps_and_settitle();
    test_quit_event_via_pushevent();
    test_resize_forces_full_repaint();
    return tge_test_report();
}
