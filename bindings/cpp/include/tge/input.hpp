#pragma once

#include "tge/tge_events.h"
#include "tge-extra/input.h"
#include "tge/direction.hpp"
#include "tge/color.hpp"

namespace tge {

/* Mirror of TGE_EventType so C++ callers switch / compare without the C prefix. */
enum class EventType : int {
    None = TGE_EVENT_NONE,
    KeyDown = TGE_EVENT_KEYDOWN,
    KeyUp = TGE_EVENT_KEYUP,
    Text = TGE_EVENT_TEXT,
    MouseDown = TGE_EVENT_MOUSEDOWN,
    MouseUp = TGE_EVENT_MOUSEUP,
    MouseMove = TGE_EVENT_MOUSEMOVE,
    Resize = TGE_EVENT_RESIZE,
    Quit = TGE_EVENT_QUIT,
    Timer = TGE_EVENT_TIMER,
    User = TGE_EVENT_USER,
};

/* A single input/timer/window event. Wraps TGE_Event and surfaces the
 * game-agnostic predicates from tge-extra/input.h (confirm/cancel/quit/pause)
 * as methods. */
struct Event {
    TGE_Event raw;

    Event() {
        raw.type = TGE_EVENT_NONE;
        raw.data = {};
    }
    explicit Event(const TGE_Event &r) : raw{r} {}

    EventType type() const { return static_cast<EventType>(raw.type); }

    int keycode() const { return raw.data.key.keycode; }
    uint32_t codepoint() const { return raw.data.text.codepoint; }

    bool confirm() const { return tge_input_confirm(&raw); }
    bool cancel() const { return tge_input_cancel(&raw); }
    bool quit() const   { return tge_input_quit(&raw); }
    bool pause() const  { return tge_input_pause(&raw); }

    operator const TGE_Event *() const { return &raw; }
};

inline Direction Direction::from_event(const Event &e) {
    return Direction(Direction::Value(tge_input_direction(&e.raw)));
}

} // namespace tge
