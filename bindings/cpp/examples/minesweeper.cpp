#include "minesweeper_game.hpp"

using namespace tge_minesweeper;

int main() {
    // 13_minesweeper.c: 16x16 grid + 2-col border + 1-row header.
    tge::App app(MS_W + 2, MS_H + 3, "tge:: Minesweeper (clone of 13_minesweeper)");
    tge_minesweeper::Game game(app);
    game.run();
    return 0;
}
