#pragma once

/* tge:: Snake — C++ clone of examples/games/06_snake_grid.c.
 *
 * Same game, same behaviour, same world/renderer split — but expressed over
 * the tge:: wrappers. The GAME LOGIC uses only tge:: types; the only raw-C
 * surface is the 4 bridge callbacks (the unavoidable TGE_Run boundary) plus a
 * handful of operations the sonda has not wrapped yet, each marked GAP and
 * registered in IMPLEMENTATION_PLAN.md §6.5.
 */

#include "tge/app.hpp"
#include "tge/canvas.hpp"
#include "tge/playfield.hpp"
#include "tge/vec2i.hpp"
#include "tge/direction.hpp"
#include "tge/input.hpp"
#include "tge/input_buffer.hpp"
#include "tge/fixedstep.hpp"
#include "tge/sprite.hpp"
#include "tge/color.hpp"

#include <vector>

namespace tge_snake {

using tge::Vec2i;
using tge::Direction;
using tge::Sprite;
using tge::Color;
using tge::Playfield;
using tge::GridTheme;
using tge::FixedStep;
using tge::InputBuffer;
using tge::Canvas;

namespace {
// 2x1 sprites (square pixels), matching 06_snake_grid.c's palette.
const Sprite SPRITE_BODY(2, 1, "\xE2\x96\x93\xE2\x96\x93", "..");
const Sprite SPRITE_WALL(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
const Sprite SPRITE_HEAD(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
const Sprite SPRITE_FOOD(2, 1, "\xE2\x96\x93\xE2\x96\x93", "@@");
const GridTheme SNAKE_THEME(
    Sprite(2, 1, "  ", NULL),   // empty
    SPRITE_BODY,                // default (the snake body)
    SPRITE_WALL,                // border / walls
    Sprite());                   // selection (unused)
}

class Game {
public:
    tge::App &app;
    Playfield playfield;
    InputBuffer input{4};
    FixedStep move{0.10f};
    std::vector<Vec2i> body;      // body[0] == head
    Vec2i food{};
    Direction direction = Direction::Right;
    int score = 0;
    bool paused = false;
    enum State { Running, Over } state = Running;

    explicit Game(tge::App &a) : app(a) {
        playfield.init(SNAKE_THEME, TGE_GRID_SCALE_2X1, 10, 6);
        playfield.set_origin(0, 1);            // leave the HUD row (like 06)
        app.set_userdata(this);
        world_reset();
    }

    /* --- world (SnakeWorld in 06) --- */
    void world_reset() {
        const TGE_View &v = playfield.view();
        body.clear();
        body.push_back(Vec2i(v.area.w / 2, v.area.h / 2));
        body.push_back(Vec2i(v.area.w / 2 - 1, v.area.h / 2));
        body.push_back(Vec2i(v.area.w / 2 - 2, v.area.h / 2));
        direction = Direction::Right;
        score = 0;
        state = Running;
        paused = false;
        move.reset();
        input.clear();
        spawn_food();
    }

    void world_resize(int gw, int gh) {
        // GAP: tge_view_update lives in C (no tge::View wrapper); returns the
        // layout-classification enum the clone switches on, exactly like 06.
        switch (playfield.update_view(gw, gh)) {
        case TGE_VIEW_FIRST_VALID:
            world_reset();
            break;
        case TGE_VIEW_RESIZED:
            for (auto &c : body)
                c = playfield.clamp_local(c);
            if (!playfield.contains(food)) {
                if (!spawn_food())
                    state = Over;
            }
            break;
        case TGE_VIEW_INVALID:
        default:
            break;
        }
    }

    bool world_step() {
        Direction d;
        while (input.pop(d)) {
            if (d == direction.opposite() || d == direction)
                continue;
            direction = d;
            break;
        }

        Vec2i next = body[0] + direction.to_vec();

        if (!playfield.contains(next))        // wall -> die (NOT bounce)
            return false;

        bool ate = (next == food);
        int check_to = (int)body.size() - (ate ? 0 : 1);
        for (int i = 0; i < check_to; i++)
            if (body[i] == next)
                return false;                  // self collision -> die

        body.insert(body.begin(), next);
        if (!ate)
            body.pop_back();
        else {
            score += 10;
            if (!spawn_food())
                return false;                  // no space left -> win/lose
        }
        return true;
    }

    bool spawn_food() {
        if (!playfield.valid())
            return false;
        for (int guard = 0; guard < 1000; guard++) {
            Vec2i p = playfield.random_point();
            bool free = true;
            for (const auto &c : body)
                if (c == p) { free = false; break; }
            if (free) { food = p; return true; }
        }
        return false;
    }

    /* --- world_update (TGE_Game update callback path) --- */
    void update(float dt) {
        Canvas cv = app.canvas();
        playfield.sync(cv.width(), cv.height(), resize_bridge, this);

        if (state != Running || paused || !playfield.valid())
            return;
        move.update(dt);
        while (move.next())
            if (!world_step()) { state = Over; break; }
    }

    /* --- renderer (SnakeRenderer in 06) --- */
    void draw(Canvas cv) {
        playfield.attach(cv);
        cv.clear(' ', Color::black(), Color::DEFAULT());

        cv.print(1, 0, Color::yellow(), Color::DEFAULT(), " SCORE: %d ", score);

        playfield.draw_border(Color::cyan(), Color::DEFAULT());
        if (!playfield.valid()) {
            cv.draw_centered_text(cv.height() / 2, " too small ",
                                  Color::red(), Color::DEFAULT());
            return;
        }

        for (size_t i = 1; i < body.size(); i++)
            playfield.set_cell_local(body[i], Color::green(), Color::DEFAULT());
        playfield.put_local(body[0], SPRITE_HEAD, Color::green(), Color::DEFAULT());
        playfield.put_attr_local(food, SPRITE_FOOD, Color::red(),
                                 Color::DEFAULT(), Canvas::attr_bold);

        if (state == Over) {
            cv.draw_modal(" GAME OVER ", " [ENTER] restart  [ESC] menu  [Q] quit ",
                          Color::red());
        } else if (paused) {
            cv.draw_modal(" PAUSED ", " [P] resume ", Color::yellow());
        }
    }

    /* --- input (world_handle_input in 06) --- */
    void on_event(const tge::Event &e) {
        if (e.type() == tge::EventType::Resize)
            return; // playfield.sync in update() handles canvas changes
        if (e.pause()) {
            if (state != Over) paused = !paused;
            return;
        }
        if (paused && !e.cancel())
            return;
        Direction d = Direction::from_event(e);
        if (d != Direction::None) { input.push(d); return; }
        if (e.quit()) { if (state == Over) app.quit(); return; }
        if (e.confirm()) {
            if (state == Over && playfield.valid()) world_reset();
            return;
        }
        if (e.cancel()) { app.quit(); } // no scene stack -> quit to exit
    }

    void run() {
        app.run(nullptr, update_bridge, draw_bridge, event_bridge);
    }

public:
    /* --- TGE_Run boundary: the only place raw TGE_App* / TGE_Canvas* / TGE_Event*
     * appear. Public so harness/tests can wire callbacks directly. --- */
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
};

} // namespace tge_snake
