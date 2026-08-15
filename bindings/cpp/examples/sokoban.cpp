#include "sokoban_game.hpp"

int main() {
    // 12_sokoban.c requests SOKO_W+2 x SOKO_H+2; the core scales to the real
    // terminal when it can query it.
    tge::App app(tge_sokoban::SOKO_W + 2, tge_sokoban::SOKO_H + 2,
                 "tge:: Sokoban (clone of 12_sokoban)");
    tge_sokoban::Game game(app);
    game.run();
    return 0;
}
