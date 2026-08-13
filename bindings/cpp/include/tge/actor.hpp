#pragma once

#include "tge-extra/actor.h"
#include "tge/vec2i.hpp"
#include "tge/color.hpp"

namespace tge {

/* A positioned sprite on a grid (Pac-Man's ghosts, a player, a unit). Wraps
 * TGE_Actor and moves the draw helper onto the object: `actor.draw(view,
 * layout)` instead of `tge_actor_draw(view, layout, &actor)`. */
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

    void draw(TGE_GridView *view, const TGE_View *layout) const {
        tge_actor_draw(view, layout, &raw);
    }
};

} // namespace tge
