/* tge:: Snake — minimal but real probe example.
 *
 * Exercises the seven wrapped pieces (App, Playfield, TileMap, Actor, Vec2i,
 * Direction, Input/Event) and shows the C++ ergonomics: actor.set_position({x,
 * y}), dir.to_vec(), Direction::from_event(e), no visible TGE_App* / TGE_Event.
 * C-level friction (palette as a raw TGE_TileSet, C callback for resize) is
 * left honest on purpose so the probe can report it. */

#include "tge/tge.h"
#include "tge/app.hpp"
#include "tge/playfield.hpp"
#include "tge/tilemap.hpp"
#include "tge/actor.hpp"
#include "tge/vec2i.hpp"
#include "tge/direction.hpp"
#include "tge/input.hpp"

#include <cstdint>

namespace {

struct Game {
    tge::Vec2i head{1, 1};
    tge::Direction dir = tge::Direction::Right;
    tge::TileMap map;
};

const TGE_Sprite SNAKE_SPRITE = TGE_SPRITE(1, 1, "@", "@");
const TGE_Sprite WALL_SPRITE  = TGE_SPRITE(1, 1, "#", "#");

TGE_TileSet wall_set{};

/* C resize callback (the contract TGE offers: function pointer + userdata). */
void on_resize(void *ud, int gw, int gh) {
    auto *g = static_cast<Game *>(ud);
    int iw = gw > 2 ? gw - 2 : 1;
    int ih = gh > 2 ? gh - 2 : 1;

    g->map.init(iw, ih);
    for (int x = 0; x < iw; ++x) { g->map.set(x, 0, 1); g->map.set(x, ih - 1, 1); }
    for (int y = 0; y < ih; ++y) { g->map.set(0, y, 1); g->map.set(iw - 1, y, 1); }

    if (g->head.x() >= iw) g->head = tge::Vec2i(iw - 1, g->head.y());
    if (g->head.y() >= ih) g->head = tge::Vec2i(g->head.x(), ih - 1);
}

} // namespace

int main() {
    wall_set.tiles[1].sprite = &WALL_SPRITE;
    wall_set.tiles[1].fg = tge::Color::cyan();
    wall_set.tiles[1].bg = tge::Color::DEFAULT();

    tge::App app(40, 20, "tge:: Snake (probe)");
    Game game;

    tge::Playfield pf;
    pf.init(&TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1, 20, 10);

    tge::Actor snake(tge::Vec2i(1, 1), &SNAKE_SPRITE);

    while (app.step()) {
        tge::Event e;
        while (app.poll_event(e)) {
            if (e.type() == tge::EventType::Quit || e.quit()) {
                app.quit();
                break;
            }
            tge::Direction d = tge::Direction::from_event(e);
            if (d != tge::Direction::None) game.dir = d;
        }

        TGE_Canvas *cv = app.canvas();
        int gw = 0, gh = 0;
        tge_grid_view_size_for(&pf.grid_view(), tge_canvas_width(cv),
                               tge_canvas_height(cv), &gw, &gh);
        pf.sync(gw, gh, on_resize, &game);
        pf.attach(cv);

        tge::Vec2i next = game.head + game.dir.to_vec();
        if (!tge_view_contains(&pf.view(), next)) {
            game.dir = game.dir.opposite();
            next = game.head + game.dir.to_vec();
        }
        game.head = next;
        snake.set_position(game.head);

        tge_clear(cv, ' ', tge::Color::DEFAULT(), tge::Color::DEFAULT());
        pf.draw_border(tge::Color::cyan(), tge::Color::DEFAULT());
        game.map.draw(&pf.grid_view().grid, 1, 1, &wall_set);
        snake.draw(&pf.grid_view(), &pf.view());
        tge_printf(cv, 1, 0, tge::Color::yellow(), tge::Color::DEFAULT(),
                   " tge:: Snake ");
    }

    return 0;
}
