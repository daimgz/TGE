/* tge:: Snake — minimal but real probe example.
 *
 * Uses the canonical TGE loop (app.run with init/update/draw/event callbacks),
 * which is the supported rendering path: TGE_Step presents the canvas inside
 * the frame, so a caller that draws *after* step() would never be shown. The
 * example still exercises the seven wrapped pieces (App, Playfield, TileMap,
 * Actor, Vec2i, Direction, Input/Event) and the C++ ergonomics:
 *   actor.set_position({x, y}), dir.to_vec(), Direction::from_event(e),
 *   no visible TGE_App* / TGE_Event. */

#include "tge/tge.h"
#include "tge/app.hpp"
#include "tge/playfield.hpp"
#include "tge/tilemap.hpp"
#include "tge/actor.hpp"
#include "tge/vec2i.hpp"
#include "tge/direction.hpp"
#include "tge/input.hpp"

namespace {

const TGE_Sprite SNAKE_SPRITE = TGE_SPRITE(1, 1, "@", "@");
const TGE_Sprite WALL_SPRITE  = TGE_SPRITE(1, 1, "#", "#");

struct Game {
    tge::Vec2i head{1, 1};
    tge::Direction dir = tge::Direction::Right;
    tge::TileMap map;
    tge::Playfield pf;
    tge::Actor snake;
    TGE_TileSet wall_set{};

    Game() : snake(tge::Vec2i(1, 1), &SNAKE_SPRITE) {
        wall_set.tiles[1].sprite = &WALL_SPRITE;
        wall_set.tiles[1].fg = tge::Color::cyan();
        wall_set.tiles[1].bg = tge::Color::DEFAULT();
        pf.init(&TGE_GRID_THEME_BLOCKS, TGE_GRID_SCALE_2X1, 20, 10);
    }
};

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

Game *game_of(TGE_App *app) {
    return static_cast<Game *>(TGE_GetUserData(app));
}

void event_cb(TGE_App *app, TGE_Event *ev) {
    tge::Event e(*ev);
    if (e.type() == tge::EventType::Quit || e.quit()) {
        TGE_Quit(app);
        return;
    }
    tge::Direction d = tge::Direction::from_event(e);
    if (d != tge::Direction::None) game_of(app)->dir = d;
}

void update_cb(TGE_App *app, float) {
    Game *g = game_of(app);
    TGE_Canvas *cv = TGE_GetCanvas(app);
    int gw = 0, gh = 0;
    tge_grid_view_size_for(&g->pf.grid_view(), tge_canvas_width(cv),
                           tge_canvas_height(cv), &gw, &gh);
    g->pf.sync(gw, gh, on_resize, g);

    tge::Vec2i next = g->head + g->dir.to_vec();
    if (!tge_view_contains(&g->pf.view(), next)) {
        g->dir = g->dir.opposite();
        next = g->head + g->dir.to_vec();
    }
    g->head = next;
    g->snake.set_position(g->head);
}

void draw_cb(TGE_App *app, TGE_Canvas *cv) {
    Game *g = game_of(app);
    g->pf.attach(cv);
    tge_clear(cv, ' ', tge::Color::DEFAULT(), tge::Color::DEFAULT());
    g->pf.draw_border(tge::Color::cyan(), tge::Color::DEFAULT());
    g->map.draw(&g->pf.grid_view().grid, 1, 1, &g->wall_set);
    g->snake.draw(&g->pf.grid_view(), &g->pf.view());
    tge_printf(cv, 1, 0, tge::Color::yellow(), tge::Color::DEFAULT(),
               " tge:: Snake ");
}

} // namespace

int main() {
    Game game;
    tge::App app(40, 20, "tge:: Snake (probe)");
    app.set_userdata(&game);
    app.run(/*init=*/nullptr, update_cb, draw_cb, event_cb);
    return 0;
}
