#ifndef TGE_EXTRA_TILEMAP_H_
#define TGE_EXTRA_TILEMAP_H_

#include <stdint.h>

#include "tge-extra/grid.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TGE_TILEMAP_MAX_W 32
#define TGE_TILEMAP_MAX_H 32
#define TGE_TILEMAP_ROLES 16

/* A fixed-size matrix of logical cells, each holding a role byte, rendered
 * over a TGE_Grid. TileMap is deliberately dumb: it stores "what is in each
 * cell" (a role) and knows how to represent it (through a TGE_TileSet); it
 * does NOT know about walls, pellets, collisions, ghosts or any game rule.
 * The game defines its own role enum, fills the palette, and reads roles
 * back with tge_tilemap_get() for its logic (e.g. `role == MY_WALL` is
 * solid).
 *
 * No allocation: the cells array is embedded, so the struct is self-contained
 * (max 32x32 cells, roles 0..15). A role with no palette sprite is skipped
 * when drawing, which is the natural way to represent empty floor.
 *
 * Typical use (a level defined as strings, each char mapped to a role):
 *   TGE_TileMap map;
 *   tge_tilemap_init(&map, 21, 13);
 *   tge_tilemap_set(&map, x, y, ROLE_WALL);
 *   // per draw:
 *   tge_tilemap_draw(&map, &grid, ox, oy, &palette);
 */
typedef struct {
    int width;
    int height;
    uint8_t cells[TGE_TILEMAP_MAX_H][TGE_TILEMAP_MAX_W];
} TGE_TileMap;

/* One palette entry: how a role is represented. A NULL sprite makes the draw
 * skip that cell. */
typedef struct {
    const TGE_Sprite *sprite;
    TGE_Color fg;
    TGE_Color bg;
} TGE_Tile;

/* The full palette: role -> TGE_Tile. Indexed by the role bytes stored in the
 * map, so `tiles.tiles[role]` is the representation of that role. */
typedef struct {
    TGE_Tile tiles[TGE_TILEMAP_ROLES];
} TGE_TileSet;

/* One ASCII legend entry: a glyph in the level string maps to a role. */
typedef struct {
    char glyph;
    uint8_t role;
} TGE_TileLegend;

/* Called for each glyph that is NOT in the legend. Markers are level metadata
 * (spawns, targets, ...), not tiles: the handler decides what the cell
 * becomes, usually with a tge_tilemap_set() call (a marker that sets nothing
 * keeps the cell's previous role). This keeps the loader, and the tilemap
 * itself, game-agnostic. */
typedef void (*TGE_TileMarkerFn)(void *userdata, char marker, int x, int y);

/* Load a level defined as one string per row into `map`. Glyphs present in
 * `legend` are written as their role; any other glyph is a marker and is
 * handed to `marker_fn`. Returns false when the dimensions exceed the fixed
 * cap, a row is not exactly `width` chars, or a marker glyph appears with no
 * handler (the map contents are then unspecified). */
bool tge_tilemap_load_ascii(TGE_TileMap *map, const char *const *rows,
                            int width, int height,
                            const TGE_TileLegend *legend, int legend_count,
                            TGE_TileMarkerFn marker_fn, void *userdata);

/* Count cells currently holding `role` (0 when map is NULL). */
int tge_tilemap_count(const TGE_TileMap *map, uint8_t role);

/* Zero the map and set its logical size. Width/height outside
 * [1, TGE_TILEMAP_MAX_W/H] are clamped; a 0 size keeps the previous value. */
void tge_tilemap_init(TGE_TileMap *map, int width, int height);

/* Store `role` at (x, y). False when out of bounds. */
bool tge_tilemap_set(TGE_TileMap *map, int x, int y, uint8_t role);

/* The role stored at (x, y); 0 when out of bounds. */
uint8_t tge_tilemap_get(const TGE_TileMap *map, int x, int y);

/* Draw the map over `grid` with cell (x, y) anchored at grid position
 * (ox + x, oy + y). Each cell draws its palette sprite through tge_grid_put
 * (natural sprite size); roles with a NULL sprite are skipped, so empty floor
 * is simply a role the palette renders as nothing. Sprites wider than a cell
 * are NOT scaled: the palette decides the per-cell look (a 2x1 cell with a
 * 2x1 wall sprite reads as a solid block). */
void tge_tilemap_draw(const TGE_TileMap *map, TGE_Grid *grid, int ox, int oy,
                      const TGE_TileSet *tiles);

#ifdef __cplusplus
}
#endif

#endif
