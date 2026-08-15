/* Headless tests for the tge:: Sokoban clone (gameplay port of 12_sokoban.c).
 * No real terminal: drives the sample via the mock backend and reads drawn
 * cells straight out of app->previous->cells. Mirrors the Snake probe harness
 * in test_snake.cpp, but Sokoban's 1x1 glyphs make cell assertions trivial. */

#include "sokoban_game.hpp"

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
#include <cstring>

using namespace tge_sokoban;

static TGE_App *make_app(MockData **md, int w = 40, int h = 16) {
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
 * runs playfield.sync() unconditionally, then the event-driven game does
 * nothing per frame. */
static void size_playfield(Game &g) {
    g.update(0.0f);
}

static bool cell_has(TGE_App *app, uint32_t ch) {
    TGE_Canvas *c = app->previous;
    if (!c || !c->cells) return false;
    int w = c->width, h = c->height;
    for (int i = 0; i < w * h; i++)
        if (c->cells[i].ch == ch) return true;
    return false;
}

TGE_TEST(clone_loads_level0_roles) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    TGE_ASSERT(g.state == Playing, "level0 starts Playing");
    TGE_ASSERT(g.player == Vec2i(1, 2), "player at (1,2) from LEVEL0 '@'");
    TGE_ASSERT(g.map.count(BOX) + g.map.count(BOX_ON_GOAL) == 1, "one box");
    TGE_ASSERT(g.map.count(GOAL) + g.map.count(BOX_ON_GOAL) == 1, "one goal");
    TGE_Destroy(app);
}

TGE_TEST(clone_push_moves_box) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world_move(Direction::Right);
    TGE_ASSERT(g.player == Vec2i(2, 2), "player moved to (2,2)");
    TGE_ASSERT(g.map.get(3, 2) == BOX, "box pushed to (3,2)");
    TGE_ASSERT(g.map.get(2, 2) == FLOOR, "cell (2,2) reverted to floor");
    TGE_ASSERT(g.map.count(BOX) + g.map.count(BOX_ON_GOAL) == 1, "box count unchanged");
    TGE_ASSERT(g.moves == 1, "moves incremented on push");
    TGE_Destroy(app);
}

TGE_TEST(clone_cannot_move_into_wall) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world_move(Direction::Left); // target (0,2) is the border wall
    TGE_ASSERT(g.player == Vec2i(1, 2), "player blocked by wall");
    TGE_ASSERT(g.map.get(2, 2) == BOX, "box untouched");
    TGE_ASSERT(g.moves == 0, "no move counted");
    TGE_Destroy(app);
}

TGE_TEST(clone_undo_restores) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world_move(Direction::Right); // push: box (2,2)->(3,2), player (2,2)
    g.world_undo();
    TGE_ASSERT(g.player == Vec2i(1, 2), "player restored to (1,2)");
    TGE_ASSERT(g.map.get(2, 2) == BOX, "box restored to (2,2)");
    TGE_ASSERT(g.map.get(3, 2) == FLOOR, "cell (3,2) cleared");
    TGE_ASSERT(g.moves == 0, "moves decremented on undo");
    TGE_Destroy(app);
}

TGE_TEST(clone_win_on_box_on_goal) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    // LEVEL0: box at (2,2), goal at (4,2). Two Rights push the box onto goal.
    g.world_move(Direction::Right);
    g.world_move(Direction::Right);
    TGE_ASSERT(g.player == Vec2i(3, 2), "player ends at (3,2)");
    TGE_ASSERT(g.map.get(4, 2) == BOX_ON_GOAL, "box on goal at (4,2)");
    TGE_ASSERT(g.map.count(BOX) == 0, "no loose box remains");
    TGE_ASSERT(g.state == Won, "state becomes Won");
    TGE_ASSERT(g.moves == 2, "two moves to win");
    TGE_Destroy(app);
}

TGE_TEST(clone_advance_level_after_win) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.state = Won;
    g.advance_level(); // level0 -> level1
    TGE_ASSERT(g.level == 1, "advanced to level 1");
    TGE_ASSERT(g.state == Playing, "level1 is Playing");
    g.level = LEVEL_COUNT - 1;
    g.advance_level(); // past last -> AllClear
    TGE_ASSERT(g.state == AllClear, "past last level -> AllClear");
    TGE_Destroy(app);
}

TGE_TEST(clone_renders_tiles) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    for (int i = 0; i < 3; i++) {
        advance_time(m, 0.1);
        TGE_Step(app);
    }
    TGE_ASSERT(cell_has(app, (uint32_t)'#'), "wall/border drawn");
    TGE_ASSERT(cell_has(app, (uint32_t)'@'), "player drawn");
    TGE_ASSERT(cell_has(app, (uint32_t)'$'), "box drawn");
    TGE_Destroy(app);
}

int main() {
    test_clone_loads_level0_roles();
    test_clone_push_moves_box();
    test_clone_cannot_move_into_wall();
    test_clone_undo_restores();
    test_clone_win_on_box_on_goal();
    test_clone_advance_level_after_win();
    test_clone_renders_tiles();
    return tge_test_report();
}
