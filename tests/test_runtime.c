#include "tge/tge_runtime.h"
#include "tge/tge_events.h"
#include "tge_internal.h"
#include "tge_test.h"
#include "mock_backend.h"

#include <string.h>

TGE_TEST(create_destroy)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    TGE_ASSERT(rt != 0, "runtime created");
    TGE_ASSERT(tge_runtime_width(rt) == 80, "width");
    TGE_ASSERT(tge_runtime_height(rt) == 24, "height");
    tge_runtime_destroy(rt);
}

TGE_TEST(poll_event_keydown)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    mock_set_input(m, "\x1b[A", 3);
    TGE_Event ev;
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev) == true, "got event");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN, "keydown");
    TGE_ASSERT(ev.data.key.keycode == TGE_KEY_UP, "up arrow");
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev) == false, "no more");
    tge_runtime_destroy(rt);
}

TGE_TEST(poll_event_lone_esc)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    mock_set_input(m, "\x1b", 1);
    TGE_Event ev;
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev) == true, "ESC flushed");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN, "keydown");
    TGE_ASSERT(ev.data.key.keycode == TGE_KEY_ESC, "ESC");
    tge_runtime_destroy(rt);
}

TGE_TEST(poll_event_text_utf8)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    mock_set_input(m, "H\xc3\xa9llo", 6);
    TGE_Event ev;
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev) && ev.data.text.codepoint == 'H',
               "H");
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev) &&
               ev.data.text.codepoint == 0xE9, "é");
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev), "l");
    tge_runtime_destroy(rt);
}

TGE_TEST(present_receives_cells)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      4, 2);
    TGE_Cell buf[8];
    for (int i = 0; i < 8; i++) {
        buf[i].ch = 'A' + (uint32_t)i;
        buf[i].fg = TGE_COLOR_RED;
        buf[i].bg = TGE_COLOR_BLACK;
        buf[i].attr = 0;
    }
    tge_runtime_present_full(rt, buf, 4);
    TGE_ASSERT(m->presented == 1, "present called");
    TGE_ASSERT(m->captured_count == 8, "all cells captured");
    TGE_ASSERT(m->captured[0].ch == 'A', "first cell");
    TGE_ASSERT(m->captured[7].ch == 'H', "last cell");
    tge_runtime_destroy(rt);
}

TGE_TEST(scheduler_integration)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    int id = tge_runtime_call_later(rt, 2.0, 99, TGE_TIMER_NORMAL);
    TGE_ASSERT(id > 0, "timer id");

    tge_runtime_pump_timers(rt, 1.0);
    TGE_Event ev;
    TGE_ASSERT(tge_runtime_poll_queued(rt, &ev) == false, "not yet");

    tge_runtime_pump_timers(rt, 3.0);
    TGE_ASSERT(tge_runtime_poll_queued(rt, &ev) == true, "timer queued");
    TGE_ASSERT(ev.type == TGE_EVENT_TIMER, "timer event");
    TGE_ASSERT(ev.data.timer.id == 99, "event id delivered");

    tge_runtime_cancel_scheduled(rt, id);
    tge_runtime_destroy(rt);
}

TGE_TEST(resize_updates_runtime)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    mock_set_input(m, "\x1b[8;120;40t", 11);
    TGE_Event ev;
    TGE_ASSERT(tge_runtime_poll_event(rt, &ev) == true, "resize event");
    TGE_ASSERT(ev.type == TGE_EVENT_RESIZE, "RESIZE");
    TGE_ASSERT(ev.data.resize.w == 120 && ev.data.resize.h == 40, "dims");
    TGE_ASSERT(tge_runtime_width(rt) == 120 && tge_runtime_height(rt) == 40,
               "runtime dims updated");
    tge_runtime_destroy(rt);
}

TGE_TEST(queue_drops_oldest)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    char buf[70];
    memset(buf, 'a', 70);
    mock_set_input(m, buf, 70);
    TGE_Event ev;
    int count = 0;
    while (tge_runtime_poll_event(rt, &ev)) {
        TGE_ASSERT(ev.type == TGE_EVENT_TEXT && ev.data.text.codepoint == 'a',
                   "only 'a' events");
        count++;
    }
    TGE_ASSERT(count <= 64, "queue bounded");
    TGE_ASSERT(count == 63, "parser + runtime drop oldest (70 - 7)");
    tge_runtime_destroy(rt);
}

TGE_TEST(ticks_and_now)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    m->now_ms = 2500;
    TGE_ASSERT(tge_runtime_ticks(rt) == 2500, "ticks");
    TGE_ASSERT(tge_runtime_now(rt) > 2.49 && tge_runtime_now(rt) < 2.51,
               "now in seconds");
    tge_runtime_destroy(rt);
}

TGE_TEST(title_routed_to_backend)
{
    MockData *m;
    TGE_Runtime *rt = tge_runtime_create_with_backend(mock_backend_create(&m),
                                                      80, 24);
    tge_runtime_set_title(rt, "Hola");
    tge_runtime_destroy(rt);
}

int main(void)
{
    test_create_destroy();
    test_poll_event_keydown();
    test_poll_event_lone_esc();
    test_poll_event_text_utf8();
    test_present_receives_cells();
    test_scheduler_integration();
    test_resize_updates_runtime();
    test_queue_drops_oldest();
    test_ticks_and_now();
    test_title_routed_to_backend();
    return tge_test_report();
}
