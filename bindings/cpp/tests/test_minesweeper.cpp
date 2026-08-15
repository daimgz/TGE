/* Headless tests for the tge:: Minesweeper clone (gameplay port of
 * 13_minesweeper.c). No real terminal: drives the sample via the mock backend
 * and reads drawn cells out of app->previous->cells. Mirrors the Sokoban probe
 * harness in test_sokoban.cpp, but adds mouse-event construction to exercise the
 * post-freeze M-1 gap (tge::Event has no mouse accessors -> e.raw.data.mouse.*).
 */

#include "minesweeper_game.hpp"

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

using namespace tge_minesweeper;

static TGE_App *make_app(MockData **md, int w = 40, int h = 24) {
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
 * runs playfield.sync() unconditionally. */
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

static tge::Event key_event(int keycode) {
    tge::Event e;
    e.raw.type = TGE_EVENT_KEYDOWN;
    e.raw.data.key.keycode = keycode;
    return e;
}
static tge::Event text_event(uint32_t cp) {
    tge::Event e;
    e.raw.type = TGE_EVENT_TEXT;
    e.raw.data.text.codepoint = cp;
    return e;
}
static tge::Event mouse_event(int x, int y, int button) {
    tge::Event e;
    e.raw.type = TGE_EVENT_MOUSEDOWN;
    e.raw.data.mouse.x = x;
    e.raw.data.mouse.y = y;
    e.raw.data.mouse.button = button;
    return e;
}

static bool find_mine(const Game &g, int &mx, int &my) {
    for (int y = 0; y < g.world.h; y++)
        for (int x = 0; x < g.world.w; x++)
            if (g.world.mine[y][x]) { mx = x; my = y; return true; }
    return false;
}
static bool find_safe(const Game &g, int &sx, int &sy) {
    for (int y = 0; y < g.world.h; y++)
        for (int x = 0; x < g.world.w; x++)
            if (!g.world.mine[y][x]) { sx = x; sy = y; return true; }
    return false;
}
static int count_mines(const Game &g) {
    int n = 0;
    for (int y = 0; y < g.world.h; y++)
        for (int x = 0; x < g.world.w; x++)
            if (g.world.mine[y][x]) n++;
    return n;
}
static int count_revealed_roles(const Game &g) {
    int n = 0;
    for (int r = CELL_0; r <= CELL_8; r++)
        n += g.world.map.count((uint8_t)r);
    return n;
}

TGE_TEST(clone_initial_state) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    TGE_ASSERT(g.world.state == Playing, "starts Playing");
    TGE_ASSERT(g.world.revealed_count == 0, "nothing revealed yet");
    TGE_ASSERT(g.world.flags == 0, "no flags yet");
    TGE_ASSERT(g.world.map.count(CELL_HIDDEN) == MS_W * MS_H, "all cells hidden");
    TGE_ASSERT(g.world.cur == Vec2i(8, 8), "cursor at center (8,8)");
    TGE_ASSERT(g.world.remaining() == MS_MINES, "all mines unflagged");
    TGE_Destroy(app);
}

TGE_TEST(clone_reveal_marks_cells) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world.reveal(0, 0);
    TGE_ASSERT(g.world.map.get(0, 0) != CELL_HIDDEN, "(0,0) revealed");
    TGE_ASSERT(g.world.map.get(0, 0) != CELL_FLAG, "(0,0) not flagged");
    TGE_ASSERT(g.world.revealed_count >= 1, "at least one cell revealed");
    TGE_ASSERT(count_revealed_roles(g) == g.world.revealed_count,
               "revealed_count matches drawn roles");
    TGE_Destroy(app);
}

TGE_TEST(clone_flag_toggle) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world.toggle_flag(3, 3);
    TGE_ASSERT(g.world.map.get(3, 3) == CELL_FLAG, "flag placed at (3,3)");
    TGE_ASSERT(g.world.flags == 1, "flag count 1");
    TGE_ASSERT(g.world.remaining() == MS_MINES - 1, "one mine left");
    g.world.toggle_flag(3, 3);
    TGE_ASSERT(g.world.map.get(3, 3) == CELL_HIDDEN, "flag removed");
    TGE_ASSERT(g.world.flags == 0, "flag count back to 0");
    TGE_ASSERT(g.world.remaining() == MS_MINES, "remaining restored");
    TGE_Destroy(app);
}

TGE_TEST(clone_cannot_flag_revealed) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world.reveal(0, 0);
    g.world.toggle_flag(0, 0);
    TGE_ASSERT(g.world.map.get(0, 0) != CELL_FLAG, "revealed cell stays revealed");
    TGE_ASSERT(g.world.flags == 0, "no flag counted on revealed cell");
    TGE_Destroy(app);
}

TGE_TEST(clone_safe_first_click) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int mx, my;
    TGE_ASSERT(find_mine(g, mx, my), "a mine exists");
    int before = count_mines(g);
    g.world.reveal(mx, my); /* first click -> guaranteed safe */
    TGE_ASSERT(g.world.state == Playing, "first click never loses");
    TGE_ASSERT(g.world.map.get(mx, my) != CELL_MINE, "mine moved off clicked cell");
    TGE_ASSERT(!g.world.mine[my][mx], "mine array updated");
    TGE_ASSERT(count_mines(g) == before, "mine count preserved (40)");
    TGE_Destroy(app);
}

TGE_TEST(clone_lose_on_mine) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int sx, sy, mx, my;
    TGE_ASSERT(find_safe(g, sx, sy), "a safe cell exists");
    TGE_ASSERT(find_mine(g, mx, my), "a mine exists");
    g.world.reveal(sx, sy); /* consume first_click on a safe cell */
    TGE_ASSERT(g.world.state == Playing, "safe click keeps playing");
    g.world.reveal(mx, my); /* now a real mine -> loss */
    TGE_ASSERT(g.world.state == Lost, "revealing a mine loses");
    TGE_ASSERT(g.world.map.count(CELL_MINE) == MS_MINES, "all mines shown");
    TGE_Destroy(app);
}

TGE_TEST(clone_win_reveals_all_safe) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int sx, sy;
    TGE_ASSERT(find_safe(g, sx, sy), "a safe cell exists");
    g.world.reveal(sx, sy); /* first click on a known-safe cell: no mine moves */
    for (int y = 0; y < g.world.h; y++)
        for (int x = 0; x < g.world.w; x++)
            if (!g.world.mine[y][x])
                g.world.reveal(x, y);
    TGE_ASSERT(g.world.state == Won, "revealing every non-mine wins");
    TGE_ASSERT(g.world.revealed_count == MS_W * MS_H - MS_MINES,
               "all 216 safe cells revealed");
    TGE_Destroy(app);
}

TGE_TEST(clone_keyboard_cursor) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.on_event(key_event(TGE_KEY_UP));
    TGE_ASSERT(g.world.cur == Vec2i(8, 7), "UP moves cursor to (8,7)");
    g.world.cur = Vec2i(0, 0);
    g.on_event(key_event(TGE_KEY_LEFT));
    TGE_ASSERT(g.world.cur == Vec2i(15, 0), "LEFT wraps to (15,0)");
    TGE_Destroy(app);
}

TGE_TEST(clone_space_reveals_cursor) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world.cur = Vec2i(0, 0);
    g.on_event(key_event(TGE_KEY_SPACE));
    TGE_ASSERT(g.world.map.get(0, 0) != CELL_HIDDEN, "space reveals cursor cell");
    TGE_ASSERT(g.world.map.get(0, 0) != CELL_FLAG, "cursor cell not flagged");
    TGE_Destroy(app);
}

TGE_TEST(clone_text_f_flags) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    g.world.cur = Vec2i(3, 3);
    g.on_event(text_event('f'));
    TGE_ASSERT(g.world.map.get(3, 3) == CELL_FLAG, "'f' flags cursor cell");
    TGE_ASSERT(g.world.flags == 1, "one flag after 'f'");
    g.on_event(text_event('f'));
    TGE_ASSERT(g.world.map.get(3, 3) == CELL_HIDDEN, "'f' again clears flag");
    TGE_Destroy(app);
}

TGE_TEST(clone_mouse_to_cell_roundtrip) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int cx = 5, cy = 7;
    int ox = g.playfield.grid_view().grid.ox;
    int oy = g.playfield.grid_view().grid.oy;
    int mx = g.playfield.origin_x() + 1 + ox + cx;
    int my = g.playfield.origin_y() + 1 + oy + cy;
    int gx, gy;
    bool ok = g.mouse_to_cell(mouse_event(mx, my, MS_BTN_LEFT), &gx, &gy);
    TGE_ASSERT(ok, "click inside grid maps");
    TGE_ASSERT(gx == cx && gy == cy, "mouse maps to expected cell");
    TGE_Destroy(app);
}

TGE_TEST(clone_mouse_outside_returns_false) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int gx, gy;
    bool ok = g.mouse_to_cell(mouse_event(1000, 1000, MS_BTN_LEFT), &gx, &gy);
    TGE_ASSERT(!ok, "click outside the playfield is rejected");
    TGE_Destroy(app);
}

TGE_TEST(clone_mouse_left_reveals) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int sx, sy;
    TGE_ASSERT(find_safe(g, sx, sy), "a safe cell exists");
    int ox = g.playfield.grid_view().grid.ox;
    int oy = g.playfield.grid_view().grid.oy;
    int mx = g.playfield.origin_x() + 1 + ox + sx;
    int my = g.playfield.origin_y() + 1 + oy + sy;
    g.on_event(mouse_event(mx, my, MS_BTN_LEFT));
    TGE_ASSERT(g.world.map.get(sx, sy) != CELL_HIDDEN, "left click reveals cell");
    TGE_ASSERT(g.world.map.get(sx, sy) != CELL_FLAG, "left click does not flag");
    TGE_Destroy(app);
}

TGE_TEST(clone_mouse_right_flags) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    size_playfield(g);
    int ox = g.playfield.grid_view().grid.ox;
    int oy = g.playfield.grid_view().grid.oy;
    int cx = 4, cy = 4;
    int mx = g.playfield.origin_x() + 1 + ox + cx;
    int my = g.playfield.origin_y() + 1 + oy + cy;
    g.on_event(mouse_event(mx, my, MS_BTN_RIGHT));
    TGE_ASSERT(g.world.map.get(cx, cy) == CELL_FLAG, "right click flags cell");
    TGE_ASSERT(g.world.flags == 1, "right click increments flags");
    TGE_Destroy(app);
}

TGE_TEST(clone_render_status) {
    MockData *m;
    TGE_App *app = make_app(&m);
    tge::App wrapped(app);
    Game g(wrapped);
    for (int i = 0; i < 3; i++) {
        advance_time(m, 0.1);
        TGE_Step(app);
    }
    TGE_ASSERT(cell_has(app, (uint32_t)'M'), "header MINESWEEPER drawn");
    TGE_Destroy(app);
}

int main() {
    test_clone_initial_state();
    test_clone_reveal_marks_cells();
    test_clone_flag_toggle();
    test_clone_cannot_flag_revealed();
    test_clone_safe_first_click();
    test_clone_lose_on_mine();
    test_clone_win_reveals_all_safe();
    test_clone_keyboard_cursor();
    test_clone_space_reveals_cursor();
    test_clone_text_f_flags();
    test_clone_mouse_to_cell_roundtrip();
    test_clone_mouse_outside_returns_false();
    test_clone_mouse_left_reveals();
    test_clone_mouse_right_flags();
    test_clone_render_status();
    return tge_test_report();
}
