/* Headless tests for the tge:: Pac-Man clone (gameplay port of 11_pacman.c).
 * No real terminal: drives the sample via the mock backend and reads drawn
 * cells straight out of app->previous->cells. Mirrors the Sokoban probe
 * harness in test_sokoban.cpp. This probe is the real consumer of tge::Actor
 * (the 4 ghosts + Pac-Man), validating the wrapper end-to-end. */

#include "pacman_game.hpp"

#include "tge/tge_app.h"
#include "tge/tge_canvas.h"
#include "tge/tge_runtime.h"
#include "tge/tge_events.h"
#include "tge/color.hpp"
#include "tge_test.h"

/* tge_internal.h must be seen under extern "C" BEFORE mock_backend.h pulls it
 * in (mock_backend.h includes it bare, so the first encounter wins linkage). */
extern "C" {
#include "tge_internal.h"
}
#include "mock_backend.h"

#include <cstdint>

using namespace tge_pacman;

static TGE_App *make_app(MockData **md) {
    int w = 2 * (MAZE_W + 2), h = MAZE_H + 3;
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, w, h);
    TGE_App *app = tge_app_create_with_runtime(rt, w, h);
    app->fps = 0;
    app->update_cb = Game::update_bridge;
    app->draw_cb = Game::draw_bridge;
    app->event_cb = Game::event_bridge;
    return app;
}

static void advance_time(MockData *m, double secs) {
    m->now_ms += (uint64_t)(secs * 1000.0);
}

/* Sync the playfield (size the view) without advancing a game step. update(0)
 * runs playfield.sync() -> FirstValid -> world_reset. */
static void size_playfield(Game &g) {
    g.update(0.0f);
}

static bool cell_has(TGE_App *app, uint32_t ch) {
    TGE_Canvas *c = app->previous;
    if (!c || !c->cells)
        return false;
    int w = c->width, h = c->height;
    for (int i = 0; i < w * h; i++)
        if (c->cells[i].ch == ch)
            return true;
    return false;
}

static Vec2i find_role(Game &g, uint8_t role) {
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++)
            if (g.map.get(x, y) == role)
                return Vec2i(x, y);
    return Vec2i(-1, -1);
}

static Direction first_open(Game &g) {
    static const Direction D[] = {Direction::Up, Direction::Left,
                                  Direction::Down, Direction::Right};
    for (Direction d : D) {
        Vec2i n = g.step_position(g.map, g.pac.actor.position(), d, true);
        if (!(n == g.pac.actor.position()))
            return d;
    }
    return Direction::None;
}

TGE_TEST(clone_loads_maze_roles) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    TGE_ASSERT(g.state == Running, "game starts Running");
    TGE_ASSERT(g.mode == SCATTER, "starts in SCATTER mode");
    TGE_ASSERT(g.lives == 3, "three lives");
    TGE_ASSERT(g.level.pellets_left > 0, "maze has pellets to eat");
    TGE_ASSERT(g.map.get(g.level.pac_spawn.x(), g.level.pac_spawn.y()) == FLOOR,
               "pac spawn marker replaced by FLOOR");
    TGE_ASSERT(g.map.get(g.level.ghost_home.x(), g.level.ghost_home.y()) ==
                   FLOOR,
               "ghost home marker replaced by FLOOR");
    TGE_ASSERT(g.ghosts[0].released, "Blinky starts released");
    TGE_ASSERT(!g.ghosts[2].released && !g.ghosts[3].released,
               "Inky/Clyde start unreleased");
    TGE_Destroy(app);
}

TGE_TEST(clone_pac_eats_pellet_when_moving) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int before = g.level.pellets_left;
    Direction d = first_open(g);
    TGE_ASSERT(d != Direction::None, "pac has an open neighbour at spawn");
    g.input.push(d);
    for (int i = 0; i < 6; i++)
        g.world_step();
    TGE_ASSERT(g.score > 0, "pac scored by eating while moving");
    TGE_ASSERT(g.level.pellets_left < before, "pellet count decreased");
    TGE_Destroy(app);
}

TGE_TEST(clone_power_pellet_triggers_frightened) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    Vec2i power = find_role(g, POWER);
    TGE_ASSERT(power.x() >= 0, "maze contains a power pellet");
    int before = g.level.pellets_left;
    g.pac.actor.set_position(power);
    g.eat_cell();
    TGE_ASSERT(g.mode == FRIGHTENED, "eating power pellet -> FRIGHTENED");
    TGE_ASSERT(g.frightened_timer == FRIGHTENED_DURATION,
               "frightened timer set to full duration");
    TGE_ASSERT(g.level.pellets_left == before - 1,
               "power pellet removed from count");
    TGE_ASSERT(g.score == 50, "power pellet scores 50");
    TGE_Destroy(app);
}

TGE_TEST(clone_eat_ghost_sets_eaten) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.score = 0;
    g.eat_ghost(g.ghosts[0]);
    TGE_ASSERT(g.ghosts[0].state == EATEN, "ghost becomes EATEN (eyes)");
    TGE_ASSERT(g.score == 200, "eating a ghost scores 200");
    TGE_Destroy(app);
}

TGE_TEST(clone_win_when_last_pellet_eaten) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    Vec2i p = g.pac.actor.position();
    g.map.set(p.x(), p.y(), PELLET); // make pac's own cell a pellet
    g.level.pellets_left = 1;
    g.eat_cell();
    TGE_ASSERT(g.level.pellets_left == 0, "last pellet removed");
    TGE_ASSERT(g.state == Won, "clearing the last pellet wins");
    TGE_Destroy(app);
}

TGE_TEST(clone_pause_on_resize) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    tge::Event e;
    e.raw.type = TGE_EVENT_RESIZE;
    e.raw.data.resize.w = 60;
    e.raw.data.resize.h = 34;
    g.on_event(e);
    TGE_ASSERT(g.paused, "resize pauses a running game");
    TGE_Destroy(app);
}

TGE_TEST(clone_renders_pellets_and_actors) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    for (int i = 0; i < 3; i++) {
        advance_time(m, 0.1);
        TGE_Step(app);
    }
    TGE_ASSERT(cell_has(app, (uint32_t)'<'), "pellet drawn");
    TGE_ASSERT(cell_has(app, (uint32_t)'@'), "ghost drawn");
    TGE_Destroy(app);
}

/* The only non-obvious promotion equivalence: world_palette() uses
 * TileSet::clear_sprite(WALL/DOOR) instead of the C's palette.fg/bg writes.
 * That is bit/semantically identical ONLY because tge_tilemap_draw skips
 * NULL-sprite tiles and renderer_draw_walls draws WALL/DOOR with an explicit
 * color. Lock that contract here. */
TGE_TEST(clone_palette_nulls_wall_door_sprites) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    TileSet pal = g.world_palette();
    TGE_ASSERT(pal.raw.tiles[WALL].sprite == nullptr,
               "WALL sprite nulled -> tilemap skips it (drawn by walls)");
    TGE_ASSERT(pal.raw.tiles[DOOR].sprite == nullptr,
               "DOOR sprite nulled -> tilemap skips it (drawn by walls)");
    TGE_ASSERT(pal.raw.tiles[PELLET].sprite != nullptr, "pellet sprite set");
    TGE_ASSERT(pal.raw.tiles[POWER].sprite != nullptr, "power sprite set");
    TGE_Destroy(app);
}

int main() {
    test_clone_loads_maze_roles();
    test_clone_pac_eats_pellet_when_moving();
    test_clone_power_pellet_triggers_frightened();
    test_clone_eat_ghost_sets_eaten();
    test_clone_win_when_last_pellet_eaten();
    test_clone_pause_on_resize();
    test_clone_renders_pellets_and_actors();
    test_clone_palette_nulls_wall_door_sprites();
    return tge_test_report();
}
