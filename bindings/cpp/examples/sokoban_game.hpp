#pragma once

/* tge:: Sokoban — C++ clone of examples/games/12_sokoban.c.
 *
 * Same game, same behaviour — but expressed over the tge:: wrappers. This probe
 * is the FIRST real consumer of tge::TileMap (load / query / mutate / draw a
 * mutable level), and reuses tge::Playfield / tge::GridTheme / tge::Sprite /
 * tge::Event exactly like the Snake probe, to confirm Playfield generalises
 * beyond Snake. The game is event-driven (no FixedStep / no InputBuffer): a
 * keypress moves immediately, so update(dt) is effectively a no-op.
 *
 * The GAME LOGIC uses only tge:: types; the raw-C surface is limited to:
 * (a) the 3 TGE_Run callbacks (update/draw/event bridges) and (b) the resize
 * bridge passed to playfield.sync. The view size/scale/classification
 * (TGE_View, TGE_GRID_SCALE_*, TGE_VIEW_*) and the TGE_TileSet palette are now
 * wrapped (tge::Playfield width/height/origin_*, tge::GridScale,
 * tge::ViewUpdate, tge::TileSet). The level loader is now a C++ facade
 * (TileMap::load_ascii with an inline legend + marker lambda); the only
 * deliberately-raw surface left is the box-art wall renderer in the Pac-Man
 * probe (tge_utf8_decode + tge_grid_put). */

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

namespace tge_sokoban {

using tge::Vec2i;
using tge::Direction;
using tge::Sprite;
using tge::Color;
using tge::Playfield;
using tge::GridTheme;
using tge::TileMap;
using tge::Canvas;

enum SokoRole { WALL = 0, FLOOR, GOAL, BOX, BOX_ON_GOAL };
enum SokoState { Playing, Won, AllClear };

/* One undoable step, stored as PRE-move roles so undo is a straight restore. */
struct Move {
    Vec2i player_from;
    bool pushed;
    Vec2i box_from;
    uint8_t box_from_role;
    Vec2i box_to;
    uint8_t box_to_role;
};

namespace {
const Sprite SPRITE_EMPTY(1, 1, " ", NULL);
const Sprite SPRITE_WALL(1, 1, "#", NULL);
const Sprite SPRITE_GOAL(1, 1, ".", NULL);
const Sprite SPRITE_BOX(1, 1, "$", NULL);
const Sprite SPRITE_BOX_G(1, 1, "*", NULL);
const Sprite SPRITE_PLAYER(1, 1, "@", NULL);

/* Owning GridTheme (borrowed-sprite pointers rebound to our own sprites). */
const GridTheme SOKOBAN_THEME(SPRITE_EMPTY, SPRITE_EMPTY, SPRITE_EMPTY);
} // namespace

class Game; // forward

static const int SOKO_W = 12;
static const int SOKO_H = 10;
static const int LEVEL_COUNT = 3;
static const int UNDO_MAX = 256;

static const char *const LEVEL0[SOKO_H] = {
    "############",
    "#          #",
    "#@$ .      #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "############",
};
static const char *const LEVEL1[SOKO_H] = {
    "############",
    "#          #",
    "#@$ .  $ . #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "############",
};
static const char *const LEVEL2[SOKO_H] = {
    "############",
    "#          #",
    "#@$ .      #",
    "#          #",
    "#      $ . #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "############",
};

struct SokoLevelDef {
    int w, h;
    const char *const *rows;
};
static const SokoLevelDef LEVELS[LEVEL_COUNT] = {
    {SOKO_W, SOKO_H, LEVEL0},
    {SOKO_W, SOKO_H, LEVEL1},
    {SOKO_W, SOKO_H, LEVEL2},
};

class Game {
public:
    tge::App &app;
    Playfield playfield;
    TileMap map;
    tge::TileSet tiles; // palette (owning wrapper over TGE_TileSet)
    Vec2i player{};
    int level = 0;
    int moves = 0;
    SokoState state = Playing;
    int undo_count = 0;
    std::array<Move, UNDO_MAX> undo{};

    explicit Game(tge::App &a) : app(a) {
        playfield.init(SOKOBAN_THEME, tge::GridScale::Scale1X1, SOKO_W, SOKO_H);
        playfield.set_origin(0, 1); // leave the HUD row (like 12)
        app.set_userdata(this);
        tiles = sokoban_palette();
        world_init();
    }

    /* --- world (SokoWorld in 12) --- */
    void world_init() {
        level = 0;
        world_reset();
    }

    void world_reset() {
        if (!sokoban_load_def(&LEVELS[level]))
            state = AllClear; // only with a malformed shipped level
    }

    void world_resize(int gw, int gh) {
        switch (playfield.update_view(gw, gh)) {
        case tge::ViewUpdate::FirstValid:
            world_reset();
            break;
        default:
            break;
        }
    }

    bool sokoban_load_def(const SokoLevelDef *def) {
        map.init(def->w, def->h);
        int player_count = 0;
        Vec2i player_pos;
        if (!map.load_ascii(def->rows, def->w, def->h,
                {{'#', WALL}, {' ', FLOOR}, {'.', GOAL},
                 {'$', BOX}, {'*', BOX_ON_GOAL}},
                [&](char g, int x, int y) {
                    switch (g) {
                    case '@':
                        player_count++;
                        player_pos = Vec2i(x, y);
                        map.set(x, y, FLOOR);
                        break;
                    case '+':
                        player_count++;
                        player_pos = Vec2i(x, y);
                        map.set(x, y, GOAL);
                        break;
                    }
                }))
            return false;
        int boxes = map.count(BOX) + map.count(BOX_ON_GOAL);
        int goals = map.count(GOAL) + map.count(BOX_ON_GOAL);
        if (player_count != 1 || boxes != goals)
            return false;
        player = player_pos;
        moves = 0;
        undo_count = 0;
        state = Playing;
        return true;
    }

    void world_move(Direction dir) {
        if (state != Playing || dir == Direction::None)
            return;
        Vec2i target = player + dir.to_vec();
        uint8_t t = map.get(target.x(), target.y());
        if (t == WALL)
            return;
        Move m{};
        m.player_from = player;
        if (t == BOX || t == BOX_ON_GOAL) {
            Vec2i beyond = target + dir.to_vec();
            uint8_t b = map.get(beyond.x(), beyond.y());
            if (b == WALL || b == BOX || b == BOX_ON_GOAL)
                return; // cannot push two boxes or into a wall/box
            m.pushed = true;
            m.box_from = target;
            m.box_from_role = t;
            m.box_to = beyond;
            m.box_to_role = b;
            map.set(beyond.x(), beyond.y(), (b == GOAL) ? BOX_ON_GOAL : BOX);
            map.set(target.x(), target.y(), (t == BOX_ON_GOAL) ? GOAL : FLOOR);
        }
        player = target;
        if (undo_count < UNDO_MAX)
            undo[undo_count++] = m;
        moves++;
        if (map.count(BOX) == 0)
            state = Won;
    }

    void world_undo() {
        if (state != Playing || undo_count <= 0)
            return;
        Move m = undo[--undo_count];
        if (m.pushed) {
            map.set(m.box_to.x(), m.box_to.y(), m.box_to_role);
            map.set(m.box_from.x(), m.box_from.y(), m.box_from_role);
        }
        player = m.player_from;
        moves--;
    }

    void advance_level() {
        if (level + 1 < LEVEL_COUNT) {
            level++;
            world_reset();
        } else {
            state = AllClear;
        }
    }

    /* --- update (event-driven: no FixedStep, nothing per frame) --- */
    void update(float dt) {
        Canvas cv = app.canvas();
        playfield.sync(cv.width(), cv.height(), resize_bridge, this);
        (void)dt;
    }

    /* --- renderer (renderer_draw in 12) --- */
    void draw(Canvas cv) {
        playfield.attach(cv);
        cv.clear(' ', Color::black(), Color::DEFAULT());
        cv.print(1, 0, Color::yellow(), Color::DEFAULT(),
                 " MOVES: %d  LEVEL: %d/%d ", moves, level + 1, LEVEL_COUNT);
        playfield.draw_border(Color::cyan(), Color::DEFAULT());

        if (!playfield.valid()) {
            cv.draw_centered_text(cv.height() / 2, " too small ",
                                  Color::red(), Color::DEFAULT());
            return;
        }

        map.draw(&playfield.grid_view().grid, playfield.origin_x(),
                 playfield.origin_y(), tiles);
        playfield.put_local(player, SPRITE_PLAYER, Color::yellow(),
                            Color::DEFAULT());

        if (state == Won)
            cv.draw_modal(" LEVEL CLEAR ",
                          " [ENTER] next   [ESC] menu ", Color::green());
        else if (state == AllClear)
            cv.draw_modal(" YOU WIN! ",
                          " [ENTER] restart   [ESC] menu ", Color::green());
    }

    /* --- input (world_handle_input + game_event in 12) --- */
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
        } // no scene stack -> quit to exit
        if (state == Won) {
            if (e.confirm())
                advance_level();
            return;
        }
        if (state == AllClear) {
            if (e.confirm()) {
                level = 0;
                world_reset();
            }
            return;
        }
        world_handle_input(e);
    }

    void world_handle_input(const tge::Event &e) {
        Direction d = Direction::from_event(e);
        if (d != Direction::None) {
            world_move(d);
            return;
        }
        if (undo_key(e)) {
            world_undo();
            return;
        }
    }

    void run() {
        app.run(nullptr, update_bridge, draw_bridge, event_bridge);
    }

public:
    /* --- TGE_Run / sync boundary: the only place raw TGE_App* / TGE_Canvas* /
     * TGE_Event* (and, for the resize bridge, void* userdata) appear. Public so
     * harness/tests can wire callbacks directly. --- */
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
    /* tge::TileSet owns its sprites (rebind on copy/move, like tge::GridTheme)
     * so the palette never borrows dangling pointers. FLOOR keeps a NULL sprite
     * so the draw helper skips it. */
    static tge::TileSet sokoban_palette() {
        tge::TileSet pal;
        pal.set(WALL, SPRITE_WALL, Color::blue(), Color::DEFAULT());
        pal.clear_sprite(FLOOR); // empty floor: skipped by draw
        pal.set(GOAL, SPRITE_GOAL, Color::yellow(), Color::DEFAULT());
        pal.set(BOX, SPRITE_BOX, Color::white(), Color::DEFAULT());
        pal.set(BOX_ON_GOAL, SPRITE_BOX_G, Color::green(), Color::DEFAULT());
        return pal;
    }

    static bool undo_key(const tge::Event &e) {
        if (e.type() == tge::EventType::KeyDown && e.keycode() == 'u')
            return true;
        if (e.type() == tge::EventType::Text) {
            uint32_t c = e.codepoint();
            if (c == 'u' || c == 'U' || c == 'z' || c == 'Z')
                return true;
        }
        return false;
    }
};

} // namespace tge_sokoban
