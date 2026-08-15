#pragma once

#include "tge-extra/actor.h"
#include "tge/vec2i.hpp"
#include "tge/color.hpp"
#include "tge/playfield.hpp"

namespace tge {

/* A positioned sprite on a grid (Pac-Man's ghosts, a player, a unit). Wraps
 * TGE_Actor and moves the draw helper onto the object: `actor.draw(playfield)`
 * instead of `tge_actor_draw(&pf->grid_view, &pf->view, &actor)`. */
struct Actor {
    TGE_Actor raw;

    Actor() : raw{TGE_Vec2i{0, 0}, nullptr, Color::DEFAULT(), Color::DEFAULT()} {}
    Actor(Vec2i pos, const TGE_Sprite *sprite,
          Color fg = Color::DEFAULT(), Color bg = Color::DEFAULT())
        : raw{pos, sprite, fg, bg} {}

    void set_position(Vec2i p) { raw.position = p; }
    Vec2i position() const { return Vec2i(raw.position); }

    void set_sprite(const TGE_Sprite *s) { raw.sprite = s; }
    const TGE_Sprite *sprite() const { return raw.sprite; }

    void set_fg(Color c) { raw.fg = c; }
    void set_bg(Color c) { raw.bg = c; }
    Color fg() const { return Color(raw.fg); }
    Color bg() const { return Color(raw.bg); }

    /* Drawn into the given playfield's grid view + view. Consumes the wrapper
     * (not raw C pointers), so gameplay never names TGE_GridView / TGE_View.
     * The playfield is taken non-const because drawing mutates its grid. */
    void draw(Playfield &pf) const {
        tge_actor_draw(&pf.raw.grid_view, &pf.raw.view, &raw);
    }
};

} // namespace tge
