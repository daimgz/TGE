#pragma once

#include "tge-extra/tilemap.h"
#include "tge/color.hpp"
#include "tge/sprite.hpp"
#include <array>
#include <vector>
#include <initializer_list>

namespace tge {

/* Owning role->representation palette (mirror of TGE_TileSet). Like
 * tge::GridTheme, it owns its tge::Sprite entries and rebinds the borrowed
 * TGE_TileSet pointers into its own storage on copy/move, so a copied/moved
 * palette never dangles. A NULL sprite (set via clear_sprite) is preserved so
 * the draw helper keeps skipping that role. */
struct TileSet {
    TGE_TileSet raw{};
    std::array<Sprite, TGE_TILEMAP_ROLES> owned{};

    TileSet() = default;
    TileSet(const TileSet &o) : raw(o.raw), owned(o.owned) { bind(); }
    TileSet(TileSet &&o) : raw(o.raw), owned(std::move(o.owned)) { bind(); }
    TileSet &operator=(const TileSet &o) {
        if (this != &o) {
            raw = o.raw;
            owned = o.owned;
            bind();
        }
        return *this;
    }
    TileSet &operator=(TileSet &&o) {
        if (this != &o) {
            raw = o.raw;
            owned = std::move(o.owned);
            bind();
        }
        return *this;
    }

    void set(int role, const Sprite &s, Color fg, Color bg) {
        owned[role] = s;
        raw.tiles[role].sprite = owned[role].ptr();
        raw.tiles[role].fg = fg.raw;
        raw.tiles[role].bg = bg.raw;
    }
    void clear_sprite(int role) { raw.tiles[role].sprite = NULL; }

    operator const TGE_TileSet *() const { return &raw; }

private:
    /* Rebind borrowed pointers into THIS palette's own sprites (the source's
     * pointers dangle after a copy/move). */
    void bind() {
        for (int i = 0; i < TGE_TILEMAP_ROLES; i++)
            if (raw.tiles[i].sprite != NULL)
                raw.tiles[i].sprite = owned[i].ptr();
    }
};

/* One ASCII legend entry: a glyph in the level string maps to a role. Mirrors
 * TGE_TileLegend so the C++ loader can build the C array without copying the
 * field layout. */
struct TileLegend {
    char glyph;
    uint8_t role;
};

/* Fixed-size role matrix. Wraps TGE_TileMap (a value type: the cells live in
 * the struct, no allocation). */
struct TileMap {
    TGE_TileMap raw;

    void init(int w, int h) { tge_tilemap_init(&raw, w, h); }
    bool set(int x, int y, uint8_t role) { return tge_tilemap_set(&raw, x, y, role); }
    uint8_t get(int x, int y) const { return tge_tilemap_get(&raw, x, y); }
    int count(uint8_t role) const { return tge_tilemap_count(&raw, role); }

    /* Map dimensions (mirrors map.width / map.height in C). Lets gameplay read
     * the size without reaching into raw. */
    int width() const { return raw.width; }
    int height() const { return raw.height; }

    /* C interop: legend as a C array + marker callback + userdata. Kept for
     * callers that already have TGE_TileLegend / TGE_TileMarkerFn. */
    bool load_ascii(const char *const *rows, int w, int h,
                    const TGE_TileLegend *legend, int legend_count,
                    TGE_TileMarkerFn marker_fn, void *userdata) {
        return tge_tilemap_load_ascii(&raw, rows, w, h, legend, legend_count,
                                      marker_fn, userdata);
    }

    /* C++ ergonomics facade over tge_tilemap_load_ascii. The legend is given as
     * a braced initializer list and the marker as a callable (lambda) that
     * captures the level context directly — no TGE_TileLegend array, no
     * TGE_TileMarkerFn, no void* userdata, no static method. The contract is
     * unchanged from the C loader: glyphs in `legend` become their role; every
     * other glyph is handed to `marker`, which decides the cell's final role
     * (usually via map.set) and records level metadata. Level loading is not a
     * hot path, so the temporary legend vector is fine. The C callback is only
     * used synchronously within this call, so the marker's lifetime is sound. */
    template <typename MarkerFn>
    bool load_ascii(const char *const *rows, int w, int h,
                    std::initializer_list<TileLegend> legend, MarkerFn marker) {
        std::vector<TGE_TileLegend> leg;
        leg.reserve(legend.size());
        for (const auto &e : legend)
            leg.push_back(TGE_TileLegend{e.glyph, e.role});
        auto c_adapter = +[](void *ud, char g, int x, int y) {
            (*static_cast<MarkerFn *>(ud))(g, x, y);
        };
        return tge_tilemap_load_ascii(&raw, rows, w, h, leg.data(),
                                     (int)leg.size(), c_adapter, &marker);
    }

    void draw(TGE_Grid *grid, int ox, int oy, const TGE_TileSet *tiles) const {
        tge_tilemap_draw(&raw, grid, ox, oy, tiles);
    }
    void draw(TGE_Grid *grid, int ox, int oy, const TileSet &tiles) const {
        tge_tilemap_draw(&raw, grid, ox, oy, &tiles.raw);
    }
};

} // namespace tge
