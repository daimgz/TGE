#pragma once

#include "tge-extra/playfield.h"
#include "tge/color.hpp"

namespace tge {

/* The playfield composition (view + grid view + layout). Wraps TGE_Playfield
 * and exposes the embedded view / grid view by reference so drawing the actor
 * is `actor.draw(&pf.grid_view(), &pf.view())` with no raw() in sight. */
struct Playfield {
    TGE_Playfield raw;

    void init(const TGE_GridTheme *theme, TGE_GridScale scale,
              int min_w, int min_h) {
        tge_playfield_init(&raw, theme, scale, min_w, min_h);
    }

    void attach(TGE_Canvas *canvas) { tge_playfield_attach(&raw, canvas); }

    /* Recompute the logical grid size; fires on_resize on change (C callback +
     * userdata, the same contract as TGE). */
    bool sync(int surface_w, int surface_h,
              tge_grid_layout_resize_fn on_resize, void *userdata) {
        return tge_playfield_sync(&raw, surface_w, surface_h, on_resize, userdata);
    }

    void draw_border(Color fg, Color bg) { tge_playfield_draw_border(&raw, fg, bg); }

    TGE_GridView &grid_view() { return raw.grid_view; }
    const TGE_View &view() const { return raw.view; }
};

} // namespace tge
