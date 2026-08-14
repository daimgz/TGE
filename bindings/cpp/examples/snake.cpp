#include "snake_game.hpp"

int main() {
    // 06_snake_grid.c requests 2*(MIN_PLAYFIELD_WIDTH+2) x (MIN_PLAYFIELD_HEIGHT+3)
    // = 24x9; the core scales to the real terminal when it can query it.
    tge::App app(24, 9, "tge:: Snake (clone of 06_snake_grid)");
    tge_snake::Game game(app);
    game.run();
    return 0;
}
