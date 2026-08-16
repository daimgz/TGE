/* Headless regression test for the tge:: Tetris C++ baseline (Fase 0).
 *
 * Drives the real game through the public bridge API under the mock backend:
 * the same trampolines tge::App::run wires (update_bridge / draw_bridge /
 * event_bridge). Locks the migration as a regression fixture without touching
 * the game logic. Mirrors tests/test_snake.cpp. */

#include "tetris/tetris_game.hpp"

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

using namespace tge_tetris;

static TGE_App *make_app(MockData **md, int w = 80, int h = 24) {
    TGE_Backend *b = mock_backend_create(md);
    TGE_Runtime *rt = tge_runtime_create_with_backend(b, w, h);
    TGE_App *app = tge_app_create_with_runtime(rt, w, h);
    app->fps = 0;
    app->update_cb = TetrisGame::update_bridge;
    app->draw_cb = TetrisGame::draw_bridge;
    app->event_cb = TetrisGame::event_bridge;
    return app;
}

static tge::Event key_ev(int key) {
    TGE_Event e;
    memset(&e, 0, sizeof(e));
    e.type = TGE_EVENT_KEYDOWN;
    e.data.key.keycode = key;
    return tge::Event(e);
}

TGE_TEST(starts_in_title_then_plays) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    TetrisGame g(wrapped);
    TGE_ASSERT(g.state == State::Title, "fresh game starts on the title screen");
    g.on_event(key_ev(TGE_KEY_ENTER));
    TGE_ASSERT(g.state == State::Playing, "ENTER starts the game");
    TGE_Destroy(app);
}

TGE_TEST(input_moves_piece) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    TetrisGame g(wrapped);
    g.on_event(key_ev(TGE_KEY_ENTER));
    int x0 = g.pos.x();
    g.on_event(key_ev(TGE_KEY_LEFT));
    TGE_ASSERT(g.pos.x() == x0 - 1, "left arrow shifts the active piece");
    TGE_Destroy(app);
}

TGE_TEST(gravity_drops_piece) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    TetrisGame g(wrapped);
    g.on_event(key_ev(TGE_KEY_ENTER));
    int y0 = g.pos.y();
    g.update(1.0f); /* well over one gravity interval at level 1 */
    TGE_ASSERT(g.pos.y() > y0, "gravity moves the piece down");
    TGE_Destroy(app);
}

TGE_TEST(ghost_matches_hard_drop) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    TetrisGame g(wrapped);
    g.on_event(key_ev(TGE_KEY_ENTER));
    Piece old = g.cur;
    Vec2i gp = g.ghostPos();
    g.hardDrop();
    bool landed = false;
    for (int y = 0; y < old.n; y++)
        for (int x = 0; x < old.n; x++)
            if (old.cells[y][x])
                if (g.board.cells[gp.y() + y][gp.x() + x] != 0)
                    landed = true;
    TGE_ASSERT(landed, "hard drop locks exactly where the ghost projects");
    TGE_Destroy(app);
}

TGE_TEST(seven_bag_is_permutation) {
    Bag b;
    for (int rep = 0; rep < 3; rep++) {
        b.refill();
        bool seen[7] = {};
        for (int i = 0; i < 7; i++) {
            int t = b.next();
            TGE_ASSERT(!seen[t], "no piece repeats within a 7-bag");
            seen[t] = true;
        }
        for (int t = 0; t < 7; t++)
            TGE_ASSERT(seen[t], "all seven pieces appear in a 7-bag");
    }
}

TGE_TEST(board_clear_lines) {
    Board b;
    for (int x = 0; x < COLS; x++)
        b.cells[ROWS - 1][x] = 1;
    b.cells[ROWS - 2][0] = 1; /* extra cell above to verify shift-down */
    int n = b.clearLines();
    TGE_ASSERT(n == 1, "one full row is cleared");
    TGE_ASSERT(b.cells[ROWS - 1][0] == 1, "row above shifted down");
}

TGE_TEST(lock_scores_and_levels) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    TetrisGame g(wrapped);
    g.on_event(key_ev(TGE_KEY_ENTER));
    int s0 = g.score.score;
    g.hardDrop();
    TGE_ASSERT(g.score.score > s0, "hard drop adds to the score");
    /* Fill the board and keep dropping until it tops out. */
    for (int i = 0; i < 300 && g.state == State::Playing; i++)
        g.hardDrop();
    TGE_ASSERT(g.state == State::GameOver, "board fills up to game over");
    TGE_Destroy(app);
}

TGE_TEST(renders_frames_without_crashing) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    TetrisGame g(wrapped);
    TGE_Step(app); /* title frame */
    TGE_ASSERT(app->previous != nullptr, "title frame rendered");
    g.on_event(key_ev(TGE_KEY_ENTER));
    for (int i = 0; i < 3; i++)
        TGE_Step(app); /* playing frames (board + ghost + piece + NEXT) */
    TGE_ASSERT(app->previous != nullptr, "playing frame rendered");
    TGE_Destroy(app);
}

int main() {
    test_starts_in_title_then_plays();
    test_input_moves_piece();
    test_gravity_drops_piece();
    test_ghost_matches_hard_drop();
    test_seven_bag_is_permutation();
    test_board_clear_lines();
    test_lock_scores_and_levels();
    test_renders_frames_without_crashing();
    return tge_test_report();
}
