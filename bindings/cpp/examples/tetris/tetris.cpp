#include "tetris_game.hpp"

int main() {
    // Requested size is the minimum/fallback; the core uses the real terminal
    // size when it can query it (TIOCGWINSZ).
    tge::App app(tge_tetris::MIN_FW + 2, tge_tetris::MIN_FH + 2, "TGE Tetris");
    tge_tetris::TetrisGame game(app);
    game.run();
    return 0;
}
