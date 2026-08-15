#pragma once

/* tge:: Minesweeper — C++ clone of examples/games/13_minesweeper.c.
 *
 * Same game, same behaviour, but expressed over the tge:: wrappers. This probe
 * is a POST-FREEZE VALIDATION of the frozen 1.0 surface (documented in
 * IMPLEMENTATION_PLAN.md §6.4 / §6.8): it consumes the API as-is and reports
 * friction as 1.1 candidates. It does NOT add or modify any wrapper.
 *
 * New surface exercised here (never used by Snake/Sokoban/Pac-Man):
 *   - mouse events (TGE_EVENT_MOUSEDOWN) -> cell of the grid,
 *   - TGE_TileMap + TGE_TileSet as a pure rendering layer (state lives in the
 *     world arrays, like the C source),
 *   - a deterministic xorshift32 RNG kept in the game (no TGE RNG dependency).
 *
 * Post-freeze gaps discovered while translating (registered, NOT fixed here):
 *   M-1  tge::Event has no mouse accessors. `e.type()` reports MouseDown, but
 *        `e.raw.data.mouse.x/y/button` is the only way to read the cursor /
 *        button. Candidate 1.1: Event::mouse_x()/mouse_y()/mouse_button().
 *   C-1  mouse_to_cell needs the grid origin, reachable only via
 *        `playfield.grid_view().grid.ox/oy` (the documented Playfield::draw_
 *        tilemap escape hatch from §6.8). Re-confirmed by this probe.
 *
 * The game logic uses only tge:: types; the raw-C surface is limited to:
 *   (a) the 3 TGE_Run callbacks (update/draw/event bridges),
 *   (b) the resize bridge passed to playfield.sync,
 *   (c) `e.raw.data.mouse.*` (M-1) and `grid_view().grid.ox/oy` (C-1).
 *
 * Like the other probes, the title Scene from the C source is dropped (Scene/
 * Game are out of scope per §6.4); the game starts directly in Playing. */

#include "tge/app.hpp"
#include "tge/canvas.hpp"
#include "tge/playfield.hpp"
#include "tge/tilemap.hpp"
#include "tge/vec2i.hpp"
#include "tge/direction.hpp"
#include "tge/input.hpp"
#include "tge/sprite.hpp"
#include "tge/color.hpp"

#include <array>
#include <cstring>

namespace tge_minesweeper {

using tge::Vec2i;
using tge::Direction;
using tge::Sprite;
using tge::Color;
using tge::Playfield;
using tge::GridTheme;
using tge::TileMap;
using tge::TileSet;
using tge::Canvas;

enum Cell {
    CELL_HIDDEN = 0,
    CELL_FLAG,
    CELL_MINE,
    CELL_0, CELL_1, CELL_2, CELL_3, CELL_4,
    CELL_5, CELL_6, CELL_7, CELL_8
};
enum State { Playing, Won, Lost };

static const int MS_W = 16;
static const int MS_H = 16;
static const int MS_MINES = 40;
static const int MS_BTN_LEFT = 0;
static const int MS_BTN_RIGHT = 2;

static const int MS_DX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
static const int MS_DY[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

namespace {
const Sprite SPRITE_EMPTY(1, 1, " ", NULL);
const Sprite SPRITE_HIDDEN(1, 1, "\xe2\x96\x92", ".");
const Sprite SPRITE_FLAG(1, 1, "F", "F");
const Sprite SPRITE_MINE(1, 1, "*", "*");
const Sprite SPRITE_N0(1, 1, " ", " ");
const Sprite SPRITE_N1(1, 1, "1", "1");
const Sprite SPRITE_N2(1, 1, "2", "2");
const Sprite SPRITE_N3(1, 1, "3", "3");
const Sprite SPRITE_N4(1, 1, "4", "4");
const Sprite SPRITE_N5(1, 1, "5", "5");
const Sprite SPRITE_N6(1, 1, "6", "6");
const Sprite SPRITE_N7(1, 1, "7", "7");
const Sprite SPRITE_N8(1, 1, "8", "8");

/* Owning GridTheme: the grid is drawn via TileMap, so empty/default/border are
 * all blank (the border is drawn separately by draw_border). */
const GridTheme MS_THEME(SPRITE_EMPTY, SPRITE_EMPTY, SPRITE_EMPTY);
} // namespace

/* Deterministic xorshift32 (mirrors mine_rng_next in 13_minesweeper.c). */
static uint32_t mine_rng_next(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* Owning role->representation palette (mirrors minesweeper_palette in C). */
static TileSet build_palette() {
    TileSet pal;
    pal.set(CELL_HIDDEN, SPRITE_HIDDEN, Color::cyan(), Color::DEFAULT());
    pal.set(CELL_FLAG, SPRITE_FLAG, Color::red(), Color::DEFAULT());
    pal.set(CELL_MINE, SPRITE_MINE, Color::red(), Color::DEFAULT());
    pal.set(CELL_0, SPRITE_N0, Color::DEFAULT(), Color::DEFAULT());
    pal.set(CELL_1, SPRITE_N1, Color::blue(), Color::DEFAULT());
    pal.set(CELL_2, SPRITE_N2, Color::green(), Color::DEFAULT());
    pal.set(CELL_3, SPRITE_N3, Color::red(), Color::DEFAULT());
    pal.set(CELL_4, SPRITE_N4, Color::magenta(), Color::DEFAULT());
    pal.set(CELL_5, SPRITE_N5, Color::yellow(), Color::DEFAULT());
    pal.set(CELL_6, SPRITE_N6, Color::cyan(), Color::DEFAULT());
    pal.set(CELL_7, SPRITE_N7, Color::white(), Color::DEFAULT());
    pal.set(CELL_8, SPRITE_N8, Color::DEFAULT(), Color::DEFAULT());
    return pal;
}

struct World {
    TileMap map;
    TileSet tiles;
    int w = MS_W, h = MS_H, mines = MS_MINES;
    bool mine[MS_H][MS_W];
    int adj[MS_H][MS_W];
    uint32_t rng = 0;
    int first_click = 0;
    int flags = 0;
    int revealed_count = 0;
    State state = Playing;
    Vec2i cur{MS_W / 2, MS_H / 2};

    void init() {
        tiles = build_palette();
        w = MS_W;
        h = MS_H;
        mines = MS_MINES;
        reset();
    }

    void reset() { new_game(rng + 1); }

    void new_game(uint32_t seed) {
        rng = seed ? seed : 0x9E3779B9u;
        first_click = 0;
        flags = 0;
        revealed_count = 0;
        state = Playing;
        cur = Vec2i(w / 2, h / 2);

        map.init(w, h);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                mine[y][x] = false;
                adj[y][x] = 0;
                map.set(x, y, CELL_HIDDEN);
            }
        }
        place_mines();
        compute_adj();
    }

    int remaining() const { return mines - flags; }

    void place_mines() {
        int total = w * h;
        int order[MS_W * MS_H];
        for (int i = 0; i < total; i++)
            order[i] = i;
        for (int i = total - 1; i > 0; i--) {
            int j = (int)(mine_rng_next(&rng) % (uint32_t)(i + 1));
            int t = order[i];
            order[i] = order[j];
            order[j] = t;
        }
        for (int k = 0; k < mines; k++) {
            int idx = order[k];
            mine[idx / w][idx % w] = true;
        }
    }

    void compute_adj() {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (mine[y][x]) {
                    adj[y][x] = 0;
                    continue;
                }
                int c = 0;
                for (int d = 0; d < 8; d++) {
                    int nx = x + MS_DX[d], ny = y + MS_DY[d];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    if (mine[ny][nx])
                        c++;
                }
                adj[y][x] = c;
            }
        }
    }

    void reveal(int sx, int sy) {
        if (state != Playing)
            return;
        if (sx < 0 || sy < 0 || sx >= w || sy >= h)
            return;
        uint8_t r0 = map.get(sx, sy);
        if (r0 == CELL_FLAG || r0 != CELL_HIDDEN)
            return;

        if (!first_click) {
            first_click = 1;
            if (mine[sy][sx]) {
                int total = w * h;
                int r;
                do {
                    r = (int)(mine_rng_next(&rng) % (uint32_t)total);
                } while (mine[r / w][r % w]);
                int rx = r % w, ry = r / w;
                mine[sy][sx] = false;
                mine[ry][rx] = true;
                compute_adj();
            }
        }

        if (mine[sy][sx]) {
            map.set(sx, sy, CELL_MINE);
            state = Lost;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if (mine[y][x])
                        map.set(x, y, CELL_MINE);
            return;
        }

        int queue[MS_W * MS_H];
        bool queued[MS_H][MS_W];
        int qh = 0, qt = 0;
        queue[qt++] = sy * w + sx;
        queued[sy][sx] = true;
        while (qh < qt) {
            int idx = queue[qh++];
            int x = idx % w, y = idx / w;
            queued[y][x] = false;
            if (mine[y][x])
                continue;
            if (map.get(x, y) != CELL_HIDDEN)
                continue;
            map.set(x, y, (uint8_t)(CELL_0 + adj[y][x]));
            revealed_count++;
            if (adj[y][x] == 0) {
                for (int d = 0; d < 8; d++) {
                    int nx = x + MS_DX[d], ny = y + MS_DY[d];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                        continue;
                    if (mine[ny][nx])
                        continue;
                    if (!queued[ny][nx] &&
                        map.get(nx, ny) == CELL_HIDDEN) {
                        queued[ny][nx] = true;
                        queue[qt++] = ny * w + nx;
                    }
                }
            }
        }

        if (revealed_count == w * h - mines)
            state = Won;
    }

    void toggle_flag(int x, int y) {
        if (state != Playing)
            return;
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        uint8_t r = map.get(x, y);
        if (r == CELL_HIDDEN) {
            map.set(x, y, CELL_FLAG);
            flags++;
        } else if (r == CELL_FLAG) {
            map.set(x, y, CELL_HIDDEN);
            flags--;
        }
    }
};

class Game {
public:
    tge::App &app;
    Playfield playfield;
    World world;

    explicit Game(tge::App &a) : app(a) {
        playfield.init(MS_THEME, tge::GridScale::Scale1X1, MS_W, MS_H);
        playfield.set_origin(0, 1); // header row above the grid
        app.set_userdata(this);
        world.init();
    }

    void update(float dt) {
        Canvas cv = app.canvas();
        playfield.sync(cv.width(), cv.height(), resize_bridge, this);
        (void)dt;
    }

    void draw(Canvas cv) {
        playfield.attach(cv);
        cv.clear(' ', Color::black(), Color::DEFAULT());

        int remaining = world.remaining();
        const char *status = (world.state == Won)    ? " YOU WIN! "
                           : (world.state == Lost)   ? " BOOM! "
                                                     : " MINESWEEPER ";
        cv.print(1, 0, Color::yellow(), Color::DEFAULT(),
                 " %s  mines left: %d ", status, remaining);
        playfield.draw_border(Color::cyan(), Color::DEFAULT());

        if (!playfield.valid()) {
            cv.draw_centered_text(cv.height() / 2, " too small ",
                                 Color::red(), Color::DEFAULT());
            return;
        }

        world.map.draw(&playfield.grid_view().grid, playfield.origin_x(),
                       playfield.origin_y(), world.tiles);

        uint8_t cr = world.map.get(world.cur.x(), world.cur.y());
        if (cr == CELL_HIDDEN || cr == CELL_FLAG)
            playfield.put_local(world.cur, SPRITE_HIDDEN, Color::white(),
                                Color::blue());

        if (world.state == Won)
            cv.draw_modal(" YOU WIN! ",
                          " [ENTER] new game   [ESC] menu ", Color::green());
        else if (world.state == Lost)
            cv.draw_modal(" BOOM! ",
                          " [ENTER] new game   [ESC] menu ", Color::red());
    }

    void on_event(const tge::Event &e) {
        if (e.type() == tge::EventType::Resize)
            return; // playfield.sync in update() handles canvas changes
        if (e.quit()) {
            app.quit();
            return;
        }
        if (e.cancel()) {
            app.quit();
            return;
        }
        if (world.state == Won || world.state == Lost) {
            if (e.confirm())
                world.reset();
            return;
        }
        world_handle_input(e);
    }

    bool world_handle_input(const tge::Event &e) {
        if (e.type() == tge::EventType::MouseDown) {
            int cx, cy;
            if (!mouse_to_cell(e, &cx, &cy))
                return true;
            if (e.raw.data.mouse.button == MS_BTN_RIGHT) // M-1: raw mouse access
                world.toggle_flag(cx, cy);
            else
                world.reveal(cx, cy);
            return true;
        }

        Direction d = Direction::from_event(e);
        if (d != Direction::None) {
            Vec2i step = d.to_vec();
            world.cur = Vec2i((world.cur.x() + step.x() + world.w) % world.w,
                              (world.cur.y() + step.y() + world.h) % world.h);
            return true;
        }
        if (e.type() == tge::EventType::KeyDown &&
            e.keycode() == TGE_KEY_SPACE) {
            world.reveal(world.cur.x(), world.cur.y());
            return true;
        }
        if (e.type() == tge::EventType::Text) {
            uint32_t c = e.codepoint();
            if (c == 'f' || c == 'F') {
                world.toggle_flag(world.cur.x(), world.cur.y());
                return true;
            }
        }
        return false;
    }

    /* Map a mouse event to a grid cell. Returns false if outside the playfield
     * or on a header/border row the grid doesn't cover. C-1: the grid origin is
     * only reachable via the raw grid_view().grid.ox/oy. */
    bool mouse_to_cell(const tge::Event &e, int *cx, int *cy) {
        int lx = e.raw.data.mouse.x - 1 - playfield.view().area.x;
        int ly = e.raw.data.mouse.y - 1 - playfield.view().area.y;
        if (!playfield.contains(Vec2i(lx, ly)))
            return false;
        *cx = lx - playfield.grid_view().grid.ox;
        *cy = ly - playfield.grid_view().grid.oy;
        if (*cx < 0 || *cy < 0 || *cx >= playfield.width() ||
            *cy >= playfield.height())
            return false;
        return true;
    }

    void run() {
        app.run(nullptr, update_bridge, draw_bridge, event_bridge);
    }

public:
    /* TGE_Run / sync boundary: the only place raw TGE_App* / TGE_Canvas* /
     * TGE_Event* (and, for the resize bridge, void* userdata) appear. Public so
     * the harness can wire callbacks directly. */
    static void update_bridge(TGE_App *app, float dt) {
        static_cast<Game *>(TGE_GetUserData(app))->update(dt);
    }
    static void draw_bridge(TGE_App *app, TGE_Canvas *cv) {
        static_cast<Game *>(TGE_GetUserData(app))->draw(Canvas{cv});
    }
    static void event_bridge(TGE_App *app, TGE_Event *ev) {
        static_cast<Game *>(TGE_GetUserData(app))->on_event(tge::Event(*ev));
    }
    static void resize_bridge(void *ud, int gw, int gh) {
        static_cast<Game *>(ud)->world_resize(gw, gh);
    }

private:
    void world_resize(int gw, int gh) {
        switch (playfield.update_view(gw, gh)) {
        case tge::ViewUpdate::FirstValid:
            world.reset();
            break;
        default:
            break;
        }
    }
};

} // namespace tge_minesweeper
