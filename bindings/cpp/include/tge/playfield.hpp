#pragma once

#include "tge-extra/playfield.h"
#include "tge-extra/grid.h"
#include "tge-extra/view.h"
#include "tge/canvas.hpp"
#include "tge/sprite.hpp"
#include "tge/vec2i.hpp"
#include "tge/color.hpp"
#include <array>
#include <utility>

namespace tge {

/* An owning grid theme: owns its sprites so the borrowed TGE_TileSet pointers
 * stay valid for the theme's lifetime (the C theme only borrows pointers).
 * Built from value sprites instead of a TGE_GridTheme aggregate literal /
 * extern consts, so game code never names TGE_Sprite* or TGE_GRID_THEME_*. */
struct GridTheme {
    TGE_GridTheme raw{};
    std::array<Sprite, 4> sprites;

    GridTheme(Sprite empty, Sprite def, Sprite border,
              Sprite selection = Sprite())
        : sprites{empty, def, border, selection} {
        bind();
    }
    GridTheme(const GridTheme &o) : sprites(o.sprites) { bind(); }
    GridTheme(GridTheme &&o)      : sprites(std::move(o.sprites)) { bind(); }
    GridTheme &operator=(const GridTheme &o) {
        if (this != &o) {
            sprites = o.sprites;
            bind();
        }
        return *this;
    }
    GridTheme &operator=(GridTheme &&o) {
        if (this != &o) {
            sprites = std::move(o.sprites);
            bind();
        }
        return *this;
    }
    operator const TGE_GridTheme *() const { return &raw; }

private:
    /* Rebind the borrowed TGE_GridTheme pointers into THIS theme's own sprites,
     * so a copied/moved theme never points at the source's (possibly dead)
     * sprite storage. */
    void bind() {
        raw.empty          = sprites[0].ptr();
        raw.default_sprite = sprites[1].ptr();
        raw.border         = sprites[2].ptr();
        raw.selection      = sprites[3].ptr();
    }
};

/* The playfield composition (view + grid view + layout). Wraps TGE_Playfield
 * and exposes the embedded view/grid view and the local-space operations the
 * game needs (contains, random_point, clamp, translate), so drawing/culling/
 * collision do not leak raw tge_view_* / tge_grid_view_* calls into game logic. */
struct Playfield {
    TGE_Playfield raw;

    void init(const GridTheme &theme, TGE_GridScale scale, int min_w, int min_h) {
        tge_playfield_init(&raw, theme, scale, min_w, min_h);
    }

    void attach(Canvas canvas) { tge_playfield_attach(&raw, canvas.raw); }

    bool sync(int surface_w, int surface_h, tge_grid_layout_resize_fn on_resize,
              void *userdata) {
        return tge_playfield_sync(&raw, surface_w, surface_h, on_resize, userdata);
    }

    void draw_border(Color fg, Color bg) { tge_playfield_draw_border(&raw, fg, bg); }

    void set_origin(int x, int y) { tge_grid_set_origin(&raw.grid_view.grid, x, y); }

    TGE_GridView &grid_view() { return raw.grid_view; }
    TGE_View &view() { return raw.view; }
    const TGE_View &view() const { return raw.view; }

    bool valid() const { return raw.view.valid; }
    TGE_ViewUpdate update_view(int w, int h) { return tge_view_update(&raw.view, w, h); }
    bool contains(Vec2i p) const { return tge_view_contains(&raw.view, p); }
    Vec2i random_point() const { return Vec2i(tge_view_random_point(&raw.view)); }
    Vec2i translate(Vec2i local) const {
        return Vec2i(tge_view_translate(&raw.view, local));
    }
    Vec2i clamp_local(Vec2i p) const {
        return Vec2i(tge_vec2i_clamp_rect(p, tge_view_local_bounds(&raw.view)));
    }

    void set_cell_local(Vec2i local, Color fg, Color bg) {
        tge_grid_view_set_cell_local(&raw.grid_view, &raw.view, local, fg, bg);
    }
    void put_local(Vec2i local, Sprite sprite, Color fg, Color bg) {
        tge_grid_view_put_local(&raw.grid_view, &raw.view, local, sprite.ptr(),
                                fg, bg);
    }
    void put_attr_local(Vec2i local, Sprite sprite, Color fg, Color bg,
                        uint8_t attr) {
        tge_grid_view_put_attr_local(&raw.grid_view, &raw.view, local,
                                     sprite.ptr(), fg, bg, attr);
    }
};

} // namespace tge
