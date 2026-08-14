#pragma once

#include "tge/tge_app.h"
#include "tge/canvas.hpp"
#include "tge/input.hpp"
#include "tge/color.hpp"

namespace tge {

/* RAII wrapper over the opaque TGE_App*. Owns the lifecycle (TGE_Create in the
 * constructor, TGE_Destroy in the destructor) so the C pointer is never
 * visible to the caller. `step()` is one frame of the manual loop and returns
 * false once a quit has been requested. */
struct App {
    TGE_App *raw;
    bool quit_ = false;
    bool own_ = true;

    /* Owning ctor: creates the app (TGE_Create) and destroys it on scope exit. */
    App(int w, int h, const char *title) : raw{TGE_Create(w, h, title)} {
        own_ = (raw != nullptr);
    }
    /* Adopting ctor: wraps an already-created TGE_App* (e.g. one built on a
     * mock backend for tests) without taking ownership. */
    explicit App(TGE_App *adopted) : raw{adopted}, own_{false} {}
    ~App() { if (raw && own_) TGE_Destroy(raw); }

    App(const App &) = delete;
    App &operator=(const App &) = delete;

    /* Raw C primitives (TGE_Step / TGE_PollEvent). NOTE: TGE_Step presents the
     * canvas *inside* the frame, so a caller that draws after step() is never
     * shown. The supported rendering path is run() with a draw callback. Keep
     * these only for low-level probing, not for drawing games. */
    bool step() {
        if (quit_) return false;
        TGE_Step(raw);
        return !quit_;
    }

    bool poll_event(Event &e) {
        bool ok = TGE_PollEvent(raw, &e.raw);
        if (ok && e.type() == EventType::Quit) quit_ = true;
        return ok;
    }

    void quit() { quit_ = true; TGE_Quit(raw); }

    Canvas canvas() { return Canvas{TGE_GetCanvas(raw)}; }

    void set_title(const char *t) { TGE_SetTitle(raw, t); }
    void set_fps(float f)        { TGE_SetFPS(raw, f); }
    void set_userdata(void *d)   { TGE_SetUserData(raw, d); }
    void *userdata() const       { return TGE_GetUserData(raw); }

    void run(tge_init_fn init, tge_update_fn update,
             tge_draw_fn draw, tge_event_fn event) {
        TGE_Run(raw, init, update, draw, event);
    }
};

} // namespace tge
