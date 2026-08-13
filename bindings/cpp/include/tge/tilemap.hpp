#pragma once

#include "tge-extra/tilemap.h"
#include "tge/color.hpp"

namespace tge {

/* Fixed-size role matrix. Wraps TGE_TileMap (a value type: the cells live in
 * the struct, no allocation). The palette (TGE_TileSet / TGE_TileLegend) is
 * still the C struct because the loader takes it by pointer; that friction is
 * exactly what the probe is meant to surface. */
struct TileMap {
    TGE_TileMap raw;

    void init(int w, int h) { tge_tilemap_init(&raw, w, h); }
    bool set(int x, int y, uint8_t role) { return tge_tilemap_set(&raw, x, y, role); }
    uint8_t get(int x, int y) const { return tge_tilemap_get(&raw, x, y); }
    int count(uint8_t role) const { return tge_tilemap_count(&raw, role); }

    bool load_ascii(const char *const *rows, int w, int h,
                    const TGE_TileLegend *legend, int legend_count,
                    TGE_TileMarkerFn marker_fn, void *userdata) {
        return tge_tilemap_load_ascii(&raw, rows, w, h, legend, legend_count,
                                      marker_fn, userdata);
    }

    void draw(TGE_Grid *grid, int ox, int oy, const TGE_TileSet *tiles) const {
        tge_tilemap_draw(&raw, grid, ox, oy, tiles);
    }
};

} // namespace tge
