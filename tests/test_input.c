#include "tge-extra/input.h"

#include "tge_test.h"

#include <string.h>

static TGE_Event keydown(int key)
{
    TGE_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = TGE_EVENT_KEYDOWN;
    ev.data.key.keycode = key;
    return ev;
}

static TGE_Event text(uint32_t cp)
{
    TGE_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = TGE_EVENT_TEXT;
    ev.data.text.codepoint = cp;
    return ev;
}

TGE_TEST(arrow_keys_map_to_directions)
{
    TGE_Event up = keydown(TGE_KEY_UP);
    TGE_Event down = keydown(TGE_KEY_DOWN);
    TGE_Event left = keydown(TGE_KEY_LEFT);
    TGE_Event right = keydown(TGE_KEY_RIGHT);
    TGE_ASSERT(tge_input_direction(&up) == TGE_DIR_UP, "up");
    TGE_ASSERT(tge_input_direction(&down) == TGE_DIR_DOWN, "down");
    TGE_ASSERT(tge_input_direction(&left) == TGE_DIR_LEFT, "left");
    TGE_ASSERT(tge_input_direction(&right) == TGE_DIR_RIGHT, "right");
}

TGE_TEST(wasd_text_maps_to_directions)
{
    TGE_Event w = text('w');
    TGE_Event W = text('W');
    TGE_Event s = text('s');
    TGE_Event S = text('S');
    TGE_Event a = text('a');
    TGE_Event A = text('A');
    TGE_Event d = text('d');
    TGE_Event D = text('D');
    TGE_ASSERT(tge_input_direction(&w) == TGE_DIR_UP, "w");
    TGE_ASSERT(tge_input_direction(&W) == TGE_DIR_UP, "W");
    TGE_ASSERT(tge_input_direction(&s) == TGE_DIR_DOWN, "s");
    TGE_ASSERT(tge_input_direction(&S) == TGE_DIR_DOWN, "S");
    TGE_ASSERT(tge_input_direction(&a) == TGE_DIR_LEFT, "a");
    TGE_ASSERT(tge_input_direction(&A) == TGE_DIR_LEFT, "A");
    TGE_ASSERT(tge_input_direction(&d) == TGE_DIR_RIGHT, "d");
    TGE_ASSERT(tge_input_direction(&D) == TGE_DIR_RIGHT, "D");
}

TGE_TEST(non_directional_events_return_none)
{
    TGE_Event esc = keydown(TGE_KEY_ESC);
    TGE_Event enter = keydown(TGE_KEY_ENTER);
    TGE_Event x = text('x');
    TGE_Event sp = text(' ');
    TGE_Event mouse;
    memset(&mouse, 0, sizeof(mouse));
    mouse.type = TGE_EVENT_MOUSEDOWN;
    TGE_ASSERT(tge_input_direction(&esc) == TGE_DIR_NONE, "esc");
    TGE_ASSERT(tge_input_direction(&enter) == TGE_DIR_NONE, "enter");
    TGE_ASSERT(tge_input_direction(&x) == TGE_DIR_NONE, "x");
    TGE_ASSERT(tge_input_direction(&sp) == TGE_DIR_NONE, "space");
    TGE_ASSERT(tge_input_direction(NULL) == TGE_DIR_NONE, "NULL");
    TGE_ASSERT(tge_input_direction(&mouse) == TGE_DIR_NONE, "non-key event");
}

TGE_TEST(actions)
{
    TGE_Event enter_kd = keydown(TGE_KEY_ENTER);
    TGE_Event enter_tx = text(13);
    TGE_Event a = text('a');
    TGE_Event esc = keydown(TGE_KEY_ESC);
    TGE_Event esc_tx = text(27);
    TGE_Event q = text('q');
    TGE_Event Q = text('Q');
    TGE_Event q_kd = keydown('q');
    TGE_Event x = text('x');
    TGE_Event p = text('p');
    TGE_Event P = text('P');
    TGE_Event p_kd = keydown('p');
    TGE_ASSERT(tge_input_confirm(&enter_kd), "enter keydown");
    TGE_ASSERT(tge_input_confirm(&enter_tx), "enter text");
    TGE_ASSERT(!tge_input_confirm(&a), "letter not confirm");
    TGE_ASSERT(tge_input_cancel(&esc), "esc cancel");
    TGE_ASSERT(!tge_input_cancel(&esc_tx), "esc as text not cancel");
    TGE_ASSERT(tge_input_quit(&q), "q quit");
    TGE_ASSERT(tge_input_quit(&Q), "Q quit");
    TGE_ASSERT(!tge_input_quit(&q_kd), "keydown q not quit");
    TGE_ASSERT(!tge_input_quit(&x), "other letter not quit");
    TGE_ASSERT(tge_input_pause(&p), "p pause");
    TGE_ASSERT(tge_input_pause(&P), "P pause");
    TGE_ASSERT(!tge_input_pause(&p_kd), "keydown p not pause");
    TGE_ASSERT(!tge_input_pause(&x), "other letter not pause");
    TGE_ASSERT(!tge_input_confirm(NULL) && !tge_input_cancel(NULL) &&
               !tge_input_quit(NULL) && !tge_input_pause(NULL), "NULL safe");
}

int main(void)
{
    test_arrow_keys_map_to_directions();
    test_wasd_text_maps_to_directions();
    test_non_directional_events_return_none();
    test_actions();
    return tge_test_report();
}
