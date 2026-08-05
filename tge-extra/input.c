#include "input.h"

TGE_Direction tge_input_direction(const TGE_Event *ev)
{
    if (!ev)
        return TGE_DIR_NONE;
    if (ev->type == TGE_EVENT_KEYDOWN) {
        switch (ev->data.key.keycode) {
        case TGE_KEY_UP:
            return TGE_DIR_UP;
        case TGE_KEY_DOWN:
            return TGE_DIR_DOWN;
        case TGE_KEY_LEFT:
            return TGE_DIR_LEFT;
        case TGE_KEY_RIGHT:
            return TGE_DIR_RIGHT;
        default:
            return TGE_DIR_NONE;
        }
    }
    if (ev->type == TGE_EVENT_TEXT) {
        switch (ev->data.text.codepoint) {
        case 'w': case 'W':
            return TGE_DIR_UP;
        case 's': case 'S':
            return TGE_DIR_DOWN;
        case 'a': case 'A':
            return TGE_DIR_LEFT;
        case 'd': case 'D':
            return TGE_DIR_RIGHT;
        default:
            return TGE_DIR_NONE;
        }
    }
    return TGE_DIR_NONE;
}

bool tge_input_confirm(const TGE_Event *ev)
{
    if (!ev)
        return false;
    if (ev->type == TGE_EVENT_KEYDOWN && ev->data.key.keycode == TGE_KEY_ENTER)
        return true;
    if (ev->type == TGE_EVENT_TEXT && ev->data.text.codepoint == 13)
        return true;
    return false;
}

bool tge_input_cancel(const TGE_Event *ev)
{
    return ev && ev->type == TGE_EVENT_KEYDOWN &&
           ev->data.key.keycode == TGE_KEY_ESC;
}

bool tge_input_quit(const TGE_Event *ev)
{
    if (!ev || ev->type != TGE_EVENT_TEXT)
        return false;
    uint32_t c = ev->data.text.codepoint;
    return c == 'q' || c == 'Q';
}
