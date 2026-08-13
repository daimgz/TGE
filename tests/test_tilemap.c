#include "tge-extra/tilemap.h"

#include "tge/tge_canvas.h"
#include "tge/tge_unicode.h"
#include "tge/tge_utf8.h"
#include "tge_internal.h"
#include "tge_test.h"

#include <string.h>

static const TGE_Cell *cell_at(const TGE_Canvas *c, int x, int y)
{
    return &c->cells[(size_t)y * (size_t)c->width + (size_t)x];
}

static bool cell_is(const TGE_Canvas *c, int x, int y, const char *glyph)
{
    const TGE_Cell *cell = cell_at(c, x, y);
    const unsigned char *p = (const unsigned char *)glyph;
    uint32_t cp;
    int n = tge_utf8_decode((const char *)p, (int)strlen(glyph), &cp);
    return n > 0 && cell->ch == cp;
}

static const TGE_Sprite SPRITE_WALL   = TGE_SPRITE(1, 1, "\xE2\x96\x88", "#");
static const TGE_Sprite SPRITE_PELLET = TGE_SPRITE(1, 1, ".", ".");
static const TGE_Sprite SPRITE_POWER  = TGE_SPRITE(1, 1, "o", "o");

#define ROLE_EMPTY 0
#define ROLE_WALL  1
#define ROLE_PELLET 2
#define ROLE_POWER  3

static TGE_TileSet palette_default(void)
{
    TGE_TileSet pal;
    memset(&pal, 0, sizeof(pal));
    pal.tiles[ROLE_WALL].sprite = &SPRITE_WALL;
    pal.tiles[ROLE_WALL].fg = tge_color_indexed(4); /* blue */
    pal.tiles[ROLE_WALL].bg = tge_color_indexed(0);
    pal.tiles[ROLE_PELLET].sprite = &SPRITE_PELLET;
    pal.tiles[ROLE_PELLET].fg = tge_color_indexed(3); /* yellow */
    pal.tiles[ROLE_PELLET].bg = tge_color_indexed(0);
    pal.tiles[ROLE_POWER].sprite = &SPRITE_POWER;
    pal.tiles[ROLE_POWER].fg = tge_color_indexed(3);
    pal.tiles[ROLE_POWER].bg = tge_color_indexed(0);
    return pal;
}

TGE_TEST(init_sets_size_and_zeroes)
{
    TGE_TileMap m;
    tge_tilemap_init(&m, 5, 7);
    TGE_ASSERT(m.width == 5 && m.height == 7, "size stored");
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 5; x++)
            TGE_ASSERT(tge_tilemap_get(&m, x, y) == 0, "cells zeroed");
}

TGE_TEST(init_clamps_bounds)
{
    TGE_TileMap m;
    tge_tilemap_init(&m, -3, 0);
    TGE_ASSERT(m.width == 1 && m.height == 1, "0/negative clamped to 1");
    tge_tilemap_init(&m, 999, 999);
    TGE_ASSERT(m.width == TGE_TILEMAP_MAX_W &&
                   m.height == TGE_TILEMAP_MAX_H,
               "oversize clamped to max");
}

TGE_TEST(set_get_roundtrip)
{
    TGE_TileMap m;
    tge_tilemap_init(&m, 8, 4);
    TGE_ASSERT(tge_tilemap_set(&m, 3, 1, ROLE_WALL), "set in bounds");
    TGE_ASSERT(tge_tilemap_get(&m, 3, 1) == ROLE_WALL, "get returns what was set");
    TGE_ASSERT(tge_tilemap_set(&m, 7, 3, ROLE_PELLET), "set at last cell");
    TGE_ASSERT(tge_tilemap_get(&m, 7, 3) == ROLE_PELLET, "last cell read back");
    TGE_ASSERT(tge_tilemap_get(&m, 0, 0) == ROLE_EMPTY, "untouched cell stays 0");
}

TGE_TEST(set_out_of_bounds_rejected)
{
    TGE_TileMap m;
    tge_tilemap_init(&m, 4, 4);
    TGE_ASSERT(!tge_tilemap_set(&m, 4, 0, ROLE_WALL), "x == width rejected");
    TGE_ASSERT(!tge_tilemap_set(&m, 0, 4, ROLE_WALL), "y == height rejected");
    TGE_ASSERT(!tge_tilemap_set(&m, -1, 0, ROLE_WALL), "negative x rejected");
    TGE_ASSERT(!tge_tilemap_set(&m, 0, -1, ROLE_WALL), "negative y rejected");
    TGE_ASSERT(tge_tilemap_get(&m, 9, 9) == 0, "get out of bounds is 0");
    TGE_ASSERT(tge_tilemap_get(&m, -1, 0) == 0, "get negative is 0");
}

TGE_TEST(draw_places_sprites_at_offset)
{
    TGE_Canvas *c = tge_canvas_create(20, 10);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    TGE_TileSet pal = palette_default();
    TGE_TileMap m;
    tge_tilemap_init(&m, 3, 2);
    tge_tilemap_set(&m, 0, 0, ROLE_WALL);
    tge_tilemap_set(&m, 2, 1, ROLE_PELLET);
    tge_tilemap_draw(&m, &g, 4, 2, &pal);

    TGE_ASSERT(cell_is(c, 4, 2, "\xE2\x96\x88"), "wall at (ox+0, oy+0)");
    TGE_ASSERT(cell_is(c, 6, 3, "."), "pellet at (ox+2, oy+1)");
    TGE_ASSERT(cell_is(c, 4, 3, " "), "unset cell not drawn");
    TGE_ASSERT(cell_at(c, 4, 2)->fg.data.index == 4, "wall fg from palette");
    TGE_ASSERT(cell_at(c, 6, 3)->fg.data.index == 3, "pellet fg from palette");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_skips_null_sprite_roles)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    TGE_TileSet pal = palette_default();
    TGE_TileMap m;
    tge_tilemap_init(&m, 2, 1);
    tge_tilemap_set(&m, 0, 0, ROLE_EMPTY); /* palette role 0 has NULL sprite */
    tge_tilemap_set(&m, 1, 0, ROLE_WALL);
    tge_tilemap_draw(&m, &g, 0, 0, &pal);

    TGE_ASSERT(cell_is(c, 0, 0, " "), "role 0 (NULL sprite) skipped");
    TGE_ASSERT(cell_is(c, 1, 0, "\xE2\x96\x88"), "wall drawn");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_null_safety)
{
    TGE_Canvas *c = tge_canvas_create(8, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    TGE_TileSet pal = palette_default();
    TGE_TileMap m;
    tge_tilemap_init(&m, 2, 2);
    tge_tilemap_draw(NULL, &g, 0, 0, &pal);
    tge_tilemap_draw(&m, NULL, 0, 0, &pal);
    tge_tilemap_draw(&m, &g, 0, 0, NULL);
    tge_tilemap_set(NULL, 0, 0, ROLE_WALL);
    TGE_ASSERT(tge_tilemap_get(NULL, 0, 0) == 0, "get NULL safe");
    tge_canvas_destroy(c);
}

TGE_TEST(draw_respects_two_wide_cells)
{
    TGE_Canvas *c = tge_canvas_create(10, 4);
    TGE_Grid g;
    tge_grid_init(&g, c);
    tge_grid_set_cell_size(&g, 2, 1);
    tge_clear(c, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    static const TGE_Sprite wide = TGE_SPRITE(2, 1, "AA", "AA");
    TGE_TileSet pal;
    memset(&pal, 0, sizeof(pal));
    pal.tiles[1].sprite = &wide;
    pal.tiles[1].fg = tge_color_indexed(2);
    pal.tiles[1].bg = tge_color_indexed(0);

    TGE_TileMap m;
    tge_tilemap_init(&m, 1, 1);
    tge_tilemap_set(&m, 0, 0, 1);
    tge_tilemap_draw(&m, &g, 3, 1, &pal);

    TGE_ASSERT(cell_is(c, 6, 1, "A") && cell_is(c, 7, 1, "A"),
               "2x1 sprite anchored at logical cell (3,1) = physical (6,1)");
    TGE_ASSERT(cell_is(c, 3, 2, " "), "next logical row untouched");
    tge_canvas_destroy(c);
}

TGE_TEST(load_ascii_writes_legend_roles)
{
    static const char *rows[] = { "ab", "bc" };
    static const TGE_TileLegend legend[] = {
        { 'a', 1 }, { 'b', 2 }, { 'c', 3 },
    };
    TGE_TileMap m;
    TGE_ASSERT(tge_tilemap_load_ascii(&m, rows, 2, 2, legend, 3, NULL, NULL),
               "load succeeds");
    TGE_ASSERT(m.width == 2 && m.height == 2, "size stored");
    TGE_ASSERT(tge_tilemap_get(&m, 0, 0) == 1, "a -> role 1");
    TGE_ASSERT(tge_tilemap_get(&m, 1, 0) == 2, "b -> role 2");
    TGE_ASSERT(tge_tilemap_get(&m, 0, 1) == 2, "b -> role 2");
    TGE_ASSERT(tge_tilemap_get(&m, 1, 1) == 3, "c -> role 3");
}

typedef struct {
    char marker;
    int x;
    int y;
} MarkerHit;

static void marker_record(void *userdata, char marker, int x, int y)
{
    MarkerHit *hit = (MarkerHit *)userdata;
    hit->marker = marker;
    hit->x = x;
    hit->y = y;
}

TGE_TEST(load_ascii_marker_calls_callback)
{
    static const char *rows[] = { "aX" };
    static const TGE_TileLegend legend[] = { { 'a', 1 } };
    TGE_TileMap m;
    MarkerHit hit = { 0, -1, -1 };
    TGE_ASSERT(tge_tilemap_load_ascii(&m, rows, 2, 1, legend, 1,
                                      marker_record, &hit),
               "marker with a handler loads");
    TGE_ASSERT(hit.marker == 'X' && hit.x == 1 && hit.y == 0,
               "marker reported with its coordinates");
    TGE_ASSERT(tge_tilemap_get(&m, 0, 0) == 1, "legend cell written");
    TGE_ASSERT(tge_tilemap_get(&m, 1, 0) == 0,
               "marker leaves the cell untouched");
}

static void marker_set_role(void *userdata, char marker, int x, int y)
{
    (void)marker;
    TGE_TileMap *map = (TGE_TileMap *)userdata;
    tge_tilemap_set(map, x, y, 7);
}

TGE_TEST(load_ascii_marker_can_set_role)
{
    static const char *rows[] = { "XY" };
    static const TGE_TileLegend legend[] = { { 'X', 3 } };
    TGE_TileMap m;
    TGE_ASSERT(tge_tilemap_load_ascii(&m, rows, 2, 1, legend, 1,
                                      marker_set_role, &m),
               "load with a marker that writes a role");
    TGE_ASSERT(tge_tilemap_get(&m, 0, 0) == 3, "legend role kept");
    TGE_ASSERT(tge_tilemap_get(&m, 1, 0) == 7, "marker role applied");
}

TGE_TEST(load_ascii_unknown_marker_without_handler_rejected)
{
    static const char *rows[] = { "aX" };
    static const TGE_TileLegend legend[] = { { 'a', 1 } };
    TGE_TileMap m;
    TGE_ASSERT(!tge_tilemap_load_ascii(&m, rows, 2, 1, legend, 1, NULL, NULL),
               "unknown glyph without a handler fails");
}

TGE_TEST(load_ascii_width_mismatch_rejected)
{
    static const char *rows[] = { "abc", "ab" };
    static const TGE_TileLegend legend[] = {
        { 'a', 1 }, { 'b', 2 }, { 'c', 3 },
    };
    TGE_TileMap m;
    TGE_ASSERT(!tge_tilemap_load_ascii(&m, rows, 3, 2, legend, 3, NULL, NULL),
               "short row fails");
}

TGE_TEST(load_ascii_null_safety)
{
    static const char *rows[] = { "a" };
    static const TGE_TileLegend legend[] = { { 'a', 1 } };
    TGE_TileMap m;
    TGE_ASSERT(!tge_tilemap_load_ascii(NULL, rows, 1, 1, legend, 1, NULL,
                                       NULL), "NULL map fails");
    TGE_ASSERT(!tge_tilemap_load_ascii(&m, NULL, 1, 1, legend, 1, NULL,
                                       NULL), "NULL rows fails");
    TGE_ASSERT(!tge_tilemap_load_ascii(&m, rows, 1, 1, NULL, 1, NULL, NULL),
               "NULL legend fails");
    TGE_ASSERT(!tge_tilemap_load_ascii(&m, rows, 999, 999, legend, 1, NULL,
                                       NULL), "oversize fails");
    TGE_ASSERT(!tge_tilemap_load_ascii(&m, rows, 0, 1, legend, 1, NULL, NULL),
               "zero width fails");
}

TGE_TEST(count_counts_role)
{
    TGE_TileMap m;
    tge_tilemap_init(&m, 3, 3);
    tge_tilemap_set(&m, 0, 0, 2);
    tge_tilemap_set(&m, 2, 2, 2);
    TGE_ASSERT(tge_tilemap_count(&m, 2) == 2, "counts occurrences");
    TGE_ASSERT(tge_tilemap_count(&m, 1) == 0, "absent role is 0");
    TGE_ASSERT(tge_tilemap_count(NULL, 2) == 0, "NULL map is 0");
}

int main(void)
{
    test_init_sets_size_and_zeroes();
    test_init_clamps_bounds();
    test_set_get_roundtrip();
    test_set_out_of_bounds_rejected();
    test_draw_places_sprites_at_offset();
    test_draw_skips_null_sprite_roles();
    test_draw_null_safety();
    test_draw_respects_two_wide_cells();
    test_load_ascii_writes_legend_roles();
    test_load_ascii_marker_calls_callback();
    test_load_ascii_marker_can_set_role();
    test_load_ascii_unknown_marker_without_handler_rejected();
    test_load_ascii_width_mismatch_rejected();
    test_load_ascii_null_safety();
    test_count_counts_role();
    return tge_test_report();
}
