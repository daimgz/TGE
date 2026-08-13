#include <string.h>

#include "tilemap.h"

bool tge_tilemap_load_ascii(TGE_TileMap *map, const char *const *rows,
                            int width, int height,
                            const TGE_TileLegend *legend, int legend_count,
                            TGE_TileMarkerFn marker_fn, void *userdata)
{
    if (!map || !rows || !legend || legend_count < 1 || width < 1 ||
        height < 1 || width > TGE_TILEMAP_MAX_W ||
        height > TGE_TILEMAP_MAX_H)
        return false;

    int lookup[256];
    for (int i = 0; i < 256; i++)
        lookup[i] = -1;
    for (int i = 0; i < legend_count; i++)
        lookup[(unsigned char)legend[i].glyph] = i;

    tge_tilemap_init(map, width, height);
    for (int y = 0; y < height; y++) {
        if (!rows[y] || (int)strlen(rows[y]) != width)
            return false;
        for (int x = 0; x < width; x++) {
            unsigned char glyph = (unsigned char)rows[y][x];
            int index = lookup[glyph];
            if (index >= 0) {
                map->cells[y][x] = legend[index].role;
            } else if (marker_fn) {
                marker_fn(userdata, (char)glyph, x, y);
            } else {
                return false;
            }
        }
    }
    return true;
}

int tge_tilemap_count(const TGE_TileMap *map, uint8_t role)
{
    if (!map)
        return 0;
    int n = 0;
    for (int y = 0; y < map->height; y++)
        for (int x = 0; x < map->width; x++)
            if (map->cells[y][x] == role)
                n++;
    return n;
}

void tge_tilemap_init(TGE_TileMap *map, int width, int height)
{
    if (!map)
        return;
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    if (width > TGE_TILEMAP_MAX_W)
        width = TGE_TILEMAP_MAX_W;
    if (height > TGE_TILEMAP_MAX_H)
        height = TGE_TILEMAP_MAX_H;
    map->width = width;
    map->height = height;
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            map->cells[y][x] = 0;
}

bool tge_tilemap_set(TGE_TileMap *map, int x, int y, uint8_t role)
{
    if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height)
        return false;
    map->cells[y][x] = role;
    return true;
}

uint8_t tge_tilemap_get(const TGE_TileMap *map, int x, int y)
{
    if (!map || x < 0 || y < 0 || x >= map->width || y >= map->height)
        return 0;
    return map->cells[y][x];
}

void tge_tilemap_draw(const TGE_TileMap *map, TGE_Grid *grid, int ox, int oy,
                      const TGE_TileSet *tiles)
{
    if (!map || !grid || !tiles)
        return;
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            const TGE_Tile *tile = &tiles->tiles[map->cells[y][x]];
            if (!tile->sprite)
                continue;
            tge_grid_put(grid, ox + x, oy + y, tile->sprite, tile->fg,
                         tile->bg);
        }
    }
}
