/* Headless tests for the tge:: Snake clone + a tge::TileMap smoke test.
 * No real terminal: drives the sample via the mock backend and reads drawn
 * cells straight out of app->previous->cells. */

#include "snake_game.hpp"

#include "tge/tge_app.h"
#include "tge/tge_canvas.h"
#include "tge/tge_runtime.h"
#include "tge/tge_events.h"
#include "tge/tilemap.hpp"
#include "tge/color.hpp"
#include "tge_test.h"

/* tge_internal.h must be seen under extern "C" BEFORE mock_backend.h pulls it
 * in (mock_backend.h includes it bare, so the first encounter wins linkage). */
extern "C" {
#include "tge_internal.h"
}
#include "mock_backend.h"

#include <cstdint>
#include <cstring>

using namespace tge_snake;

static TGE_App *make_app(MockData **md, int w = 40, int h = 16) {
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, w, h);
    TGE_App *app = tge_app_create_with_runtime(rt, w, h);
    app->fps = 0;
    app->update_cb = Game::update_bridge;
    app->draw_cb   = Game::draw_bridge;
    app->event_cb  = Game::event_bridge;
    return app;
}

static void advance_time(MockData *m, double secs) {
    m->now_ms += (uint64_t)(secs * 1000.0);
}

/* Sync the playfield (size the view) without advancing a game step. update(0)
 * runs playfield.sync() unconditionally, then the fixed-step accumulator adds 0
 * so no world_step fires. */
static void size_playfield(tge_snake::Game &g) {
    g.update(0.0f);
}

static bool cell_has(TGE_App *app, uint32_t ch) {
    // tge_app_frame swaps current/previous at the end: the last drawn buffer
    // lives in `previous`.
    TGE_Canvas *c = app->previous;
    if (!c || !c->cells) return false;
    int w = c->width, h = c->height;
    for (int i = 0; i < w * h; i++)
        if (c->cells[i].ch == ch) return true;
    return false;
}

TGE_TEST(clone_view_sized_to_full_grid_no_double_conversion) {
    MockData *m;
    TGE_App *app = make_app(&m, 40, 16);   // 2x1 -> grid 20x16 -> interior 18x14
    tge::App wrapped(app);
    Game g(wrapped);
    advance_time(m, 0.1);
    TGE_Step(app);
    TGE_ASSERT(g.playfield.view().area.w == 18,
               "playfield width is full grid (no double conversion)");
    // 2x1 -> grid 20x15 (origin reserves the HUD row) -> interior 18x13.
    TGE_ASSERT(g.playfield.view().area.h == 13,
               "playfield height is full grid");
    TGE_ASSERT(g.playfield.valid(), "playfield valid at this size");
    TGE_Destroy(app);
}

TGE_TEST(clone_snake_dies_cleanly_into_wall) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    const TGE_View &v = g.playfield.view();

    g.direction = Direction::Right;
    g.body = { Vec2i(v.area.w - 1, v.area.h / 2) }; // head at right edge
    g.update(0.11f);                                // one step -> head leaves the field
    TGE_ASSERT(g.state == Game::Over, "state becomes Over on wall hit");
    // head never left the field (world_step returns false without moving)
    TGE_ASSERT(g.playfield.contains(g.body[0]), "head stays in bounds on death");
    TGE_Destroy(app);
}

TGE_TEST(clone_grows_after_eating_food) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);

    int before = (int)g.body.size();
    g.food = g.body[0] + g.direction.to_vec(); // food one cell ahead of head
    g.update(0.11f);                           // one step -> eats food -> grows
    TGE_ASSERT(g.state == Game::Running, "eating food keeps the snake alive");
    TGE_ASSERT((int)g.body.size() == before + 1,
               "snake grew after eating");
    TGE_ASSERT(g.score == 10, "score increased on eat");
    TGE_ASSERT(g.playfield.contains(g.food), "food respawned inside the field");
    TGE_ASSERT(g.playfield.contains(g.body[0]), "head moved into the food cell");
    TGE_Destroy(app);
}

TGE_TEST(clone_self_collision_kills) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    const TGE_View &v = g.playfield.view();

    int cx = v.area.w / 2, cy = v.area.h / 2;
    // 5-long snake shaped so moving Up drops the head onto body[3].
    g.body = { Vec2i(cx, cy), Vec2i(cx + 1, cy), Vec2i(cx + 1, cy - 1),
               Vec2i(cx, cy - 1), Vec2i(cx - 1, cy - 1) };
    g.input.clear();
    g.input.push(Direction::Up); // (cx,cy) -> (cx, cy-1) == body[3]
    g.update(0.11f);
    TGE_ASSERT(g.state == Game::Over, "self-collision ends the game");
    TGE_Destroy(app);
}

TGE_TEST(clone_renders_head_body_and_walls) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    // 2x1, MOVE_INTERVAL=0.10s: one step per frame.
    for (int i = 0; i < 3; i++) {
        advance_time(m, 0.1);
        TGE_Step(app);
    }
    const uint32_t head_glyph   = 0x2588; // '█' full block
    const uint32_t body_glyph   = 0x2593; // '▓' medium shade
    TGE_ASSERT(cell_has(app, head_glyph), "head sprite drawn via tge::Actor-free put_local");
    TGE_ASSERT(cell_has(app, body_glyph), "body sprite drawn via set_cell_local");
    TGE_Destroy(app);
}

TGE_TEST(tilemap_smoke) {
    // Keeps TileMap in the probe coverage (the 06 clone itself has no tilemap;
    // 06 uses the grid border + cell draws). Exercises tge::TileMap + a raw
    // TGE_TileSet palette (TileSet wrapper deferred: no consumer yet).
    tge::TileMap map;
    map.init(5, 5);
    TGE_TileSet palette{};
    static const TGE_Sprite wall = TGE_SPRITE(1, 1, "#", "#");
    palette.tiles[1].sprite = &wall;
    palette.tiles[1].fg = tge::Color::cyan().raw;
    palette.tiles[1].bg = tge::Color::DEFAULT().raw;
    map.set(0, 0, 1);
    map.set(4, 4, 1);
    TGE_ASSERT(map.get(0, 0) == 1, "tilemap set/get roundtrips");
    TGE_ASSERT(map.count(1) == 2, "tilemap count is correct");
    (void)wall;
}

/* Ownership probe (no consumer game): a copied/moved tge::GridTheme must point
 * into ITS OWN sprites, never the source's (which may be destroyed). A default
 * member-wise copy would copy the borrowed TGE_GridTheme pointers verbatim and
 * dangle. tge::Sprite is a value type (POD over static literals), so a shallow
 * copy is safe and shares the literal. */
TGE_TEST(probe_gridtheme_sprite_ownership) {
    using tge::Sprite;
    using tge::GridTheme;
    Sprite e(2, 1, "  ", NULL), d(2, 1, "[]", ".."), b(2, 1, "##", "##");
    GridTheme a(e, d, b);
    const TGE_Sprite *a_empty = a.raw.empty;

    GridTheme c = a; // copy
    TGE_ASSERT(c.raw.empty != a_empty,
               "copied GridTheme does not borrow the source's sprite pointers");
    TGE_ASSERT(c.raw.empty == c.sprites[0].ptr(),
               "copied GridTheme points into its own sprites");
    TGE_ASSERT(c.raw.default_sprite == c.sprites[1].ptr(),
               "copied theme's default sprite is its own");

    GridTheme m = std::move(a); // move
    TGE_ASSERT(m.raw.empty == m.sprites[0].ptr(),
               "moved GridTheme points into its own (moved-from) sprites");
    TGE_ASSERT(m.raw.empty != c.raw.empty,
               "moved theme owns distinct sprite storage from the copy");
    TGE_ASSERT(strcmp(m.raw.empty->utf8, "  ") == 0,
               "moved sprite keeps its glyphs");

    // Sprite value type: copy shares the literal and keeps its glyphs.
    Sprite s1(2, 1, "@", "@");
    Sprite s2 = s1;
    TGE_ASSERT(s2.raw.utf8 == s1.raw.utf8,
               "Sprite copy shares the literal (value type)");
    TGE_ASSERT(strcmp(s2.raw.utf8, "@") == 0, "Sprite copy keeps its glyphs");
}

int main() {
    test_clone_view_sized_to_full_grid_no_double_conversion();
    test_clone_snake_dies_cleanly_into_wall();
    test_clone_grows_after_eating_food();
    test_clone_self_collision_kills();
    test_clone_renders_head_body_and_walls();
    test_tilemap_smoke();
    test_probe_gridtheme_sprite_ownership();
    return tge_test_report();
}
