#include "tge/tge_events.h"
#include "tge_internal.h"
#include "tge_test.h"

#include <string.h>

/* ── create/destroy ───────────────────────────────────── */

TGE_TEST(create_destroy)
{
    TGE_Parser *p = tge_parser_create();
    TGE_ASSERT(p != 0, "parser created");
    tge_parser_destroy(p);
}

/* ── ASCII printable ──────────────────────────────────── */

TGE_TEST(text_ascii)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "Hello", 5);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "got event");
    TGE_ASSERT(ev.type == TGE_EVENT_TEXT, "text event");
    TGE_ASSERT(ev.data.text.codepoint == 'H', "'H'");
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "got event 2");
    TGE_ASSERT(ev.data.text.codepoint == 'e', "'e'");
    tge_parser_poll(p, &ev); /* l */
    tge_parser_poll(p, &ev); /* l */
    tge_parser_poll(p, &ev); /* o */
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "no more events");
    tge_parser_destroy(p);
}

/* ── Ctrl+letter ──────────────────────────────────────── */

TGE_TEST(ctrl_letter)
{
    TGE_Parser *p = tge_parser_create();
    char ctrl_c[1] = { 3 };
    tge_parser_feed(p, ctrl_c, 1);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "got event");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN, "keydown");
    TGE_ASSERT(ev.data.key.keycode == 'C', "Ctrl+C → C");
    TGE_ASSERT(ev.data.key.mod == TGE_MOD_CTRL, "Ctrl modifier");
    tge_parser_destroy(p);
}

/* ── Enter / Tab / Backspace ──────────────────────────── */

TGE_TEST(control_keys)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\n\r\t\x08", 4);

    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_ENTER, "LF → ENTER");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_ENTER, "CR → ENTER");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_TAB, "TAB");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_BACKSPACE, "BS → BACKSPACE");
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "no more");
    tge_parser_destroy(p);
}

/* ── ESC key ──────────────────────────────────────────── */

TGE_TEST(escape_key)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B", 1);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "ESC buffered, not yet");
    tge_parser_flush(p);
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "ESC after flush");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN, "keydown");
    TGE_ASSERT(ev.data.key.keycode == TGE_KEY_ESC, "ESC");
    tge_parser_destroy(p);
}

/* ── ESC with more data (Alt) ─────────────────────────── */

TGE_TEST(esc_then_char)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1Bxyz", 4);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "ESC key");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_ESC, "ESC first");
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "text x");
    TGE_ASSERT(ev.type == TGE_EVENT_TEXT
               && ev.data.text.codepoint == 'x', "'x'");
    tge_parser_destroy(p);
}

/* ── Arrow keys ───────────────────────────────────────── */

TGE_TEST(arrow_keys)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[A\x1B[B\x1B[C\x1B[D", 12);
    TGE_Event ev;

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "UP");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_UP, "UP key");

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "DOWN");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_DOWN, "DOWN key");

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "RIGHT");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_RIGHT, "RIGHT key");

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "LEFT");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_LEFT, "LEFT key");

    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "no more");
    tge_parser_destroy(p);
}

/* ── Arrows with modifier ─────────────────────────────── */

TGE_TEST(arrow_modifiers)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[1;2A\x1B[1;5B\x1B[1;3C\x1B[1;4D", 24);
    TGE_Event ev;

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "Shift+UP");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_UP
               && ev.data.key.mod == TGE_MOD_SHIFT, "Shift+UP");

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "Ctrl+DOWN");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_DOWN
               && ev.data.key.mod == TGE_MOD_CTRL, "Ctrl+DOWN");

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "Alt+RIGHT");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_RIGHT
               && ev.data.key.mod == TGE_MOD_ALT, "Alt+RIGHT");

    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "Alt+Shift+LEFT");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_LEFT
               && ev.data.key.mod == (TGE_MOD_ALT | TGE_MOD_SHIFT),
               "Alt+Shift+LEFT");
    tge_parser_destroy(p);
}

/* ── Home / End / PageUp / PageDown ───────────────────── */

TGE_TEST(nav_keys)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[H\x1B[F\x1B[5~\x1B[6~", 16);
    TGE_Event ev;

    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_HOME, "HOME");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_END, "END");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_PAGEUP, "PGUP");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_PAGEDOWN, "PGDN");
    tge_parser_destroy(p);
}

/* ── Insert / Delete ──────────────────────────────────── */

TGE_TEST(ins_del)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[2~\x1B[3~", 8);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_INSERT, "INS");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_DELETE, "DEL");
    tge_parser_destroy(p);
}

/* ── F-keys ───────────────────────────────────────────── */

TGE_TEST(fkeys_ss3)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1BOP\x1BOQ\x1BOR\x1BOS", 12);
    TGE_Event ev;

    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_F1, "F1");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_F2, "F2");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_F3, "F3");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_F4, "F4");
    tge_parser_destroy(p);
}

TGE_TEST(fkeys_csi)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[11~\x1B[12~\x1B[13~\x1B[14~"
                       "\x1B[15~\x1B[17~\x1B[18~\x1B[19~"
                       "\x1B[20~\x1B[21~\x1B[23~\x1B[24~", 60);
    TGE_Event ev;

    int keys[] = { TGE_KEY_F1, TGE_KEY_F2, TGE_KEY_F3, TGE_KEY_F4,
                   TGE_KEY_F5, TGE_KEY_F6, TGE_KEY_F7, TGE_KEY_F8,
                   TGE_KEY_F9, TGE_KEY_F10, TGE_KEY_F11, TGE_KEY_F12 };
    for (int i = 0; i < 12; i++) {
        TGE_ASSERT(tge_parser_poll(p, &ev) == true, "got F-key");
        TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
                   && ev.data.key.keycode == keys[i], "correct F-key");
    }
    tge_parser_destroy(p);
}

/* ── Shift+Tab ────────────────────────────────────────── */

TGE_TEST(shift_tab)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[Z", 3);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "shift+tab");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_TAB
               && ev.data.key.mod == TGE_MOD_SHIFT, "Shift+Tab");
    tge_parser_destroy(p);
}

/* ── Mixed text and controls ──────────────────────────── */

TGE_TEST(mixed_text_and_controls)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "hola\x1B[A", 8);
    TGE_Event ev;

    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_TEXT
               && ev.data.text.codepoint == 'h', "h");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_TEXT
               && ev.data.text.codepoint == 'o', "o");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_TEXT
               && ev.data.text.codepoint == 'l', "l");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_TEXT
               && ev.data.text.codepoint == 'a', "a");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_UP, "UP");
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "no more");
    tge_parser_destroy(p);
}

/* ── Sequence split across feeds ──────────────────────── */

TGE_TEST(split_sequence)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[", 2);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "nothing after ESC[");
    tge_parser_feed(p, "A", 1);
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "UP after completing");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_UP, "UP split");
    tge_parser_destroy(p);
}

TGE_TEST(split_params)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[1;", 4);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "nothing after ESC[1;");
    tge_parser_feed(p, "5B", 2);
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "Ctrl+DOWN after completing");
    TGE_ASSERT(ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_DOWN
               && ev.data.key.mod == TGE_MOD_CTRL, "Ctrl+DOWN split");
    tge_parser_destroy(p);
}

/* ── UTF-8 in parser ──────────────────────────────────── */

TGE_TEST(utf8_in_parser)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\xE2\x82\xAC", 3);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "€ event");
    TGE_ASSERT(ev.type == TGE_EVENT_TEXT, "text");
    TGE_ASSERT(ev.data.text.codepoint == 0x20AC, "€ codepoint");
    tge_parser_destroy(p);
}

TGE_TEST(utf8_split)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\xE2\x82", 2);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "nothing after partial UTF-8");
    tge_parser_feed(p, "\xAC", 1);
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "€ after completing");
    TGE_ASSERT(ev.type == TGE_EVENT_TEXT
               && ev.data.text.codepoint == 0x20AC, "€ split");
    tge_parser_destroy(p);
}

/* ── SGR mouse ────────────────────────────────────────── */

TGE_TEST(sgr_mouse_press)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[<0;15;30M", 11);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "mouse down");
    TGE_ASSERT(ev.type == TGE_EVENT_MOUSEDOWN, "MOUSEDOWN");
    TGE_ASSERT(ev.data.mouse.button == 0, "button 0 (left)");
    TGE_ASSERT(ev.data.mouse.x == 15, "x=15");
    TGE_ASSERT(ev.data.mouse.y == 30, "y=30");
    tge_parser_destroy(p);
}

TGE_TEST(sgr_mouse_release)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[<0;10;20m", 11);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "mouse up");
    TGE_ASSERT(ev.type == TGE_EVENT_MOUSEUP, "MOUSEUP");
    tge_parser_destroy(p);
}

TGE_TEST(sgr_mouse_motion)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[<32;5;10M", 12);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "mouse motion");
    TGE_ASSERT(ev.type == TGE_EVENT_MOUSEMOVE, "MOUSEMOVE");
    TGE_ASSERT(ev.data.mouse.x == 5, "x=5");
    TGE_ASSERT(ev.data.mouse.y == 10, "y=10");
    tge_parser_destroy(p);
}

TGE_TEST(sgr_mouse_modifier)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[<20;8;8M", 11);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "mouse ctrl+alt");
    TGE_ASSERT(ev.type == TGE_EVENT_MOUSEDOWN, "MOUSEDOWN");
    TGE_ASSERT(ev.data.mouse.button == 20, "button=20"); /* ctrl+alt+left+alt? */
    tge_parser_destroy(p);
}

/* ── Resize ───────────────────────────────────────────── */

TGE_TEST(resize)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1B[8;80;24t", 10);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "resize");
    TGE_ASSERT(ev.type == TGE_EVENT_RESIZE, "RESIZE");
    TGE_ASSERT(ev.data.resize.w == 80, "width=80");
    TGE_ASSERT(ev.data.resize.h == 24, "height=24");
    tge_parser_destroy(p);
}

/* ── DEL byte ignored ─────────────────────────────────── */

TGE_TEST(del_ignored)
{
    TGE_Parser *p = tge_parser_create();
    char buf[2] = { 'a', 0x7F };
    tge_parser_feed(p, buf, 2);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == true, "text 'a'");
    TGE_ASSERT(ev.type == TGE_EVENT_TEXT && ev.data.text.codepoint == 'a', "'a'");
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "DEL ignored");
    tge_parser_destroy(p);
}

/* ── SS3 arrows (alternate encoding) ──────────────────── */

TGE_TEST(ss3_arrows)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "\x1BOA\x1BOB\x1BOC\x1BOD", 12);
    TGE_Event ev;

    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_UP, "SS3 UP");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_DOWN, "SS3 DOWN");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_RIGHT, "SS3 RIGHT");
    TGE_ASSERT(tge_parser_poll(p, &ev) && ev.type == TGE_EVENT_KEYDOWN
               && ev.data.key.keycode == TGE_KEY_LEFT, "SS3 LEFT");
    tge_parser_destroy(p);
}

/* ── Queue overflow ───────────────────────────────────── */

TGE_TEST(queue_wrap)
{
    TGE_Parser *p = tge_parser_create();
    char many[100];
    memset(many, 'x', 100);
    tge_parser_feed(p, many, 100);
    TGE_Event ev;
    int n = 0;
    while (tge_parser_poll(p, &ev))
        n++;
    TGE_ASSERT(n > 0, "some events delivered");
    TGE_ASSERT(n <= 64, "at most queue size");
    tge_parser_destroy(p);
}

/* ── Empty feed ───────────────────────────────────────── */

TGE_TEST(empty_feed)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_feed(p, "", 0);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "no events from empty feed");
    tge_parser_destroy(p);
}

/* ── Flush with nothing pending ───────────────────────── */

TGE_TEST(flush_idle)
{
    TGE_Parser *p = tge_parser_create();
    tge_parser_flush(p);
    TGE_Event ev;
    TGE_ASSERT(tge_parser_poll(p, &ev) == false, "flush idle = no events");
    tge_parser_destroy(p);
}

int main(void)
{
    test_create_destroy();
    test_text_ascii();
    test_ctrl_letter();
    test_control_keys();
    test_escape_key();
    test_esc_then_char();
    test_arrow_keys();
    test_arrow_modifiers();
    test_nav_keys();
    test_ins_del();
    test_fkeys_ss3();
    test_fkeys_csi();
    test_shift_tab();
    test_mixed_text_and_controls();
    test_split_sequence();
    test_split_params();
    test_utf8_in_parser();
    test_utf8_split();
    test_sgr_mouse_press();
    test_sgr_mouse_release();
    test_sgr_mouse_motion();
    test_sgr_mouse_modifier();
    test_resize();
    test_del_ignored();
    test_ss3_arrows();
    test_queue_wrap();
    test_empty_feed();
    test_flush_idle();
    return tge_test_report();
}
