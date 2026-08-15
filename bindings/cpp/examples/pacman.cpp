#include "pacman_game.hpp"

int main() {
    // 11_pacman.c requests 2*(MAZE_W+2) x (MAZE_H+3); the core scales to the
    // real terminal when it can query it.
    tge::App app(2 * (tge_pacman::MAZE_W + 2), tge_pacman::MAZE_H + 3,
                 "tge:: Pac-Man (clone of 11_pacman)");
    if (!app.raw)
        return 1;
    tge_pacman::Game game(app);
    game.run();
    return 0;
}
