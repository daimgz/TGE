#pragma once

/* tge:: Tetris — C++ reference Tetris built exclusively on TGE (Fase 0 baseline).
 *
 * Faithful port of examples/games/03_tetris.c behaviour, restructured onto the
 * wrapped tge:: surface only (no raw tge_grid_ / tge_view_ / tge_runtime_):
 *
 *   - board + NEXT preview : tge::Grid (fixed 10x20 / 5x5, does NOT expand)
 *   - window "too small" gate: tge::Playfield::update_view / valid (View only)
 *   - gravity              : internal fixed-step accumulator (was scheduler)
 *   - rotation debounce    : internal cooldown (was TGE_Timer)
 *   - title/gameover/pause : an in-game State (was the scene stack)
 *   - ghost piece, 7-bag    : kept (gameplay state, no engine API)
 *
 * See examples/tetris/EQUIVALENCE.md for the C -> C++ mapping and
 * examples/tetris/DESIGN.md for the frozen internal contract. */

#include "tge/app.hpp"
#include "tge/canvas.hpp"
#include "tge/playfield.hpp"
#include "tge/grid.hpp"
#include "tge/vec2i.hpp"
#include "tge/direction.hpp"
#include "tge/input.hpp"
#include "tge/input_buffer.hpp"
#include "tge/fixedstep.hpp"
#include "tge/sprite.hpp"
#include "tge/color.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace tge_tetris {

using tge::Vec2i;
using tge::Color;
using tge::Sprite;
using tge::Grid;
using tge::Playfield;
using tge::GridTheme;
using tge::FixedStep;
using tge::Canvas;
using tge::App;

constexpr int COLS = 10;
constexpr int ROWS = 20;
constexpr int OX = 2;
constexpr int OY = 2;
constexpr int MIN_FW = 34;
constexpr int MIN_FH = 22;
constexpr float ROTATE_DEBOUNCE = 0.20f;
constexpr int POINTS_L1 = 100, POINTS_L2 = 300, POINTS_L3 = 500,
              POINTS_L4 = 800;

enum class State { Title, Playing, Paused, GameOver };

/* A tetromino in its current orientation. */
struct Piece {
    int n = 0;
    uint8_t cells[4][4] = {};
    int type = 0;
    uint8_t color = 0;
};

/* 7-bag randomizer: every block of seven pieces is a permutation of all seven
 * tetromino indices, so each appears exactly once before any repeats. Pure
 * gameplay state - lives in the game, not in TGE. */
struct Bag {
    int pieces[7] = {};
    int index = 0;
    void refill() {
        for (int i = 0; i < 7; ++i)
            pieces[i] = i;
        for (int i = 6; i > 0; --i) {
            int j = rand() % (i + 1);
            int t = pieces[i];
            pieces[i] = pieces[j];
            pieces[j] = t;
        }
        index = 0;
    }
    int next() {
        if (index >= 7)
            refill();
        return pieces[index++];
    }
};

/* The stack: collision test, lock and line clear. Logical 10x20 board. */
struct Board {
    int cells[ROWS][COLS] = {};

    bool fits(const Piece &p, Vec2i pos) const {
        for (int y = 0; y < p.n; y++)
            for (int x = 0; x < p.n; x++)
                if (p.cells[y][x]) {
                    int bx = pos.x() + x, by = pos.y() + y;
                    if (bx < 0 || bx >= COLS || by < 0 || by >= ROWS)
                        return false;
                    if (cells[by][bx])
                        return false;
                }
        return true;
    }

    void lock(const Piece &p, Vec2i pos) {
        for (int y = 0; y < p.n; y++)
            for (int x = 0; x < p.n; x++)
                if (p.cells[y][x])
                    cells[pos.y() + y][pos.x() + x] = p.type + 1;
    }

    int clearLines() {
        int n = 0;
        for (int y = ROWS - 1; y >= 0; y--) {
            bool full = true;
            for (int x = 0; x < COLS; x++)
                if (!cells[y][x]) {
                    full = false;
                    break;
                }
            if (!full)
                continue;
            n++;
            for (int yy = y; yy > 0; yy--)
                memcpy(cells[yy], cells[yy - 1], sizeof(cells[0]));
            memset(cells[0], 0, sizeof(cells[0]));
            y++;
        }
        return n;
    }
};

/* Score / level / lines. Level rises every 10 cleared lines. */
struct Scoring {
    int score = 0, level = 1, lines = 0;
    void onClear(int n) {
        static const int pts[5] = {0, POINTS_L1, POINTS_L2, POINTS_L3,
                                    POINTS_L4};
        score += pts[n] * level;
        lines += n;
        int lvl = lines / 10 + 1;
        if (lvl != level)
            level = lvl;
    }
};

/* Basic rotation (clockwise) + horizontal kick table. SRS + kicks land in
 * Fase 2; this keeps the baseline behaviour identical to 03_tetris.c. */
struct RotationSystem {
    static void rotateCW(Piece &p) {
        uint8_t tmp[4][4] = {};
        for (int y = 0; y < p.n; y++)
            for (int x = 0; x < p.n; x++)
                tmp[x][p.n - 1 - y] = p.cells[y][x];
        memcpy(p.cells, tmp, sizeof(p.cells));
    }
    static bool tryRotate(Piece &p, const Board &b, Vec2i &pos) {
        Piece rot = p;
        rotateCW(rot);
        static const int kicks[5] = {0, -1, 1, -2, 2};
        for (int i = 0; i < 5; i++) {
            Vec2i np = pos + Vec2i(kicks[i], 0);
            if (b.fits(rot, np)) {
                p = rot;
                pos = np;
                return true;
            }
        }
        return false;
    }
};

/* Seven canonical tetrominoes (index == type). Colors match 03_tetris.c. */
const Piece kPieces[7] = {
    Piece{4, {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 0, 6},
    Piece{2, {{1, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 1, 3},
    Piece{3, {{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 2, 5},
    Piece{3, {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 3, 2},
    Piece{3, {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 4, 1},
    Piece{3, {{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 5, 4},
    Piece{3, {{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}, 6, 7},
};

namespace {
/* Square-pixel blocks theme for the view-gate playfield (only valid()/update_view
 * are used, so the glyphs are irrelevant). */
const Sprite SPR_EMPTY(2, 1, "  ", nullptr);
const Sprite SPR_BLOCK(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
const Sprite SPR_BORDER(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
const GridTheme TETRIS_THEME(SPR_EMPTY, SPR_BLOCK, SPR_BORDER, Sprite());
}

class TetrisGame {
public:
    App &app;
    Grid board_grid;  /* 10x20 board at (OX, OY) */
    Grid preview_grid; /* 5x5 NEXT preview at (24, 2) */
    Playfield view_gate; /* window "too small" gate only (View) */

    Board board;
    Bag bag;
    Scoring score;
    Piece cur;
    Vec2i pos{0, 0};
    int next_type = 0;
    State state = State::Title;
    bool paused = false;

    FixedStep gravity{1.0f};
    FixedStep rot{ROTATE_DEBOUNCE};
    int last_level = 1;

    explicit TetrisGame(App &a) : app(a) {
        board_grid.init(&TGE_GRID_THEME_BLOCKS);
        board_grid.square_pixels();
        board_grid.set_origin(OX, OY);

        preview_grid.init(&TGE_GRID_THEME_BLOCKS);
        preview_grid.square_pixels();
        preview_grid.set_origin(24, 2);

        view_gate.init(TETRIS_THEME, tge::GridScale::Scale2X1, MIN_FW, MIN_FH);

        app.set_userdata(this);
        resetWorld();
        state = State::Title;
        gravity = FixedStep(gravityInterval());
        rot.update(ROTATE_DEBOUNCE); /* preload: first rotation is immediate */
    }

    float gravityInterval() const {
        float s = 0.9f * powf(0.8f, (float)score.level);
        return s < 0.08f ? 0.08f : s;
    }

    void resetWorld() {
        memset(board.cells, 0, sizeof(board.cells));
        score = Scoring{};
        bag.refill();
        next_type = bag.next();
        spawn();
        paused = false;
        gravity = FixedStep(gravityInterval());
        rot.update(ROTATE_DEBOUNCE);
    }

    void newGame() {
        resetWorld();
        state = State::Playing;
    }

    void spawn() {
        cur = kPieces[next_type];
        next_type = bag.next();
        pos = Vec2i((COLS - cur.n) / 2, 0);
        if (!board.fits(cur, pos))
            state = State::GameOver;
    }

    Vec2i ghostPos() const {
        Vec2i gp = pos;
        while (board.fits(cur, gp + Vec2i(0, 1)))
            gp = gp + Vec2i(0, 1);
        return gp;
    }

    /* --- world update --- */
    void gravityStep() {
        if (state != State::Playing)
            return;
        Vec2i np = pos + Vec2i(0, 1);
        if (board.fits(cur, np))
            pos = np;
        else
            lockPiece();
    }

    void lockPiece() {
        board.lock(cur, pos);
        int n = board.clearLines();
        if (n)
            score.onClear(n);
        if (state == State::Playing)
            spawn();
    }

    void moveCur(int dx) {
        if (state != State::Playing)
            return;
        Vec2i np = pos + Vec2i(dx, 0);
        if (board.fits(cur, np))
            pos = np;
    }

    void rotatePressed() {
        if (state != State::Playing)
            return;
        if (!rot.next())
            return;
        RotationSystem::tryRotate(cur, board, pos);
        rot.reset();
    }

    void softDrop() {
        if (state != State::Playing)
            return;
        Vec2i np = pos + Vec2i(0, 1);
        if (board.fits(cur, np)) {
            pos = np;
            score.score += 1;
        }
    }

    void hardDrop() {
        if (state != State::Playing)
            return;
        Vec2i np = pos;
        while (board.fits(cur, np + Vec2i(0, 1)))
            np = np + Vec2i(0, 1);
        int d = np.y() - pos.y();
        pos = np;
        score.score += 2 * d;
        lockPiece();
    }

    void update(float dt) {
        Canvas cv = app.canvas();
        view_gate.update_view(cv.width(), cv.height());
        if (state != State::Playing || paused)
            return;
        rot.update(dt);
        if (score.level != last_level) {
            gravity = FixedStep(gravityInterval());
            last_level = score.level;
        }
        gravity.update(dt);
        while (gravity.next())
            gravityStep();
    }

    /* --- renderer --- */
    void drawBoard() {
        for (int y = 0; y < ROWS; y++)
            for (int x = 0; x < COLS; x++)
                if (board.cells[y][x])
                    board_grid.set_cell(
                        x, y,
                        Color::indexed(kPieces[board.cells[y][x] - 1].color),
                        Color::DEFAULT());
    }

    void drawPiece() {
        Color col = Color::indexed(cur.color);
        for (int y = 0; y < cur.n; y++)
            for (int x = 0; x < cur.n; x++)
                if (cur.cells[y][x])
                    board_grid.set_cell(pos.x() + x, pos.y() + y, col,
                                        Color::DEFAULT());
    }

    /* Landing projection (ghost): where the piece rests on hard drop, drawn
     * dimmed. Same collision test as the real drop, so it never overwrites
     * locked cells. */
    void drawGhost() {
        if (state != State::Playing)
            return;
        Vec2i gp = ghostPos();
        Color col = Color::indexed(cur.color);
        for (int y = 0; y < cur.n; y++)
            for (int x = 0; x < cur.n; x++)
                if (cur.cells[y][x])
                    board_grid.put_attr(gp.x() + x, gp.y() + y,
                                        TGE_GRID_THEME_BLOCKS.default_sprite,
                                        col, Color::DEFAULT(),
                                        TGE_CELL_ATTR_DIM);
    }

    void drawNext(Canvas cv) {
        tge_draw_region(cv, (TGE_Rect){24, 2, 10, 5}, " NEXT ",
                        tge::Color::yellow());
        const Piece &nx = kPieces[next_type];
        int npx = (5 - nx.n) / 2, npy = (5 - nx.n) / 2;
        for (int y = 0; y < nx.n; y++)
            for (int x = 0; x < nx.n; x++)
                if (nx.cells[y][x])
                    preview_grid.set_cell(npx + x, npy + y,
                                          Color::indexed(nx.color),
                                          Color::DEFAULT());
    }

    void drawTitle(Canvas cv, int w, int h) {
        tge_draw_region(cv, (TGE_Rect){0, 0, w, h}, nullptr,
                        tge::Color::cyan());
        cv.draw_centered_text(h / 2 - 3, " TETRIS ", Color::green(),
                              Color::DEFAULT());
        cv.draw_centered_text(h / 2,
                              " Move: Left/Right   Rotate: W/Up ",
                              Color::white(), Color::DEFAULT());
        cv.draw_centered_text(h / 2 + 1,
                              " Down: soft drop    Space: hard drop ",
                              Color::white(), Color::DEFAULT());
        cv.draw_centered_text(h / 2 + 3,
                              " [ENTER] to start  [Q] to quit ",
                              Color::yellow(), Color::DEFAULT());
    }

    void drawGameOver(Canvas cv, int w, int h) {
        (void)h;
        cv.fill_rect(0, OY + ROWS / 2 - 1, w, 3, ' ', Color::DEFAULT(),
                     Color::DEFAULT());
        cv.draw_centered_text(OY + ROWS / 2 - 1, " GAME OVER ",
                              Color::yellow(), Color::DEFAULT());
        cv.draw_centered_text(OY + ROWS / 2 + 1,
                              " [ENTER] retry  [ESC] menu ", Color::white(),
                              Color::DEFAULT());
    }

    void drawPaused(Canvas cv) {
        cv.fill_rect(0, OY + ROWS / 2 - 1, cv.width(), 3, ' ', Color::DEFAULT(),
                     Color::DEFAULT());
        cv.draw_centered_text(OY + ROWS / 2 - 1, " PAUSED ",
                              Color::yellow(), Color::DEFAULT());
        cv.draw_centered_text(OY + ROWS / 2 + 1, " [P] resume ",
                              Color::white(), Color::DEFAULT());
    }

    void draw(Canvas cv) {
        int w = cv.width(), h = cv.height();
        cv.clear(' ', Color::black(), Color::DEFAULT());

        if (state == State::Title) {
            drawTitle(cv, w, h);
            return;
        }

        if (!view_gate.valid()) {
            cv.draw_centered_text(h / 2, " too small ", Color::red(),
                                  Color::DEFAULT());
            return;
        }

        board_grid.attach(cv);
        preview_grid.attach(cv);

        tge_draw_region(cv, (TGE_Rect){OX - 1, OY - 1, COLS * 2 + 2, ROWS + 2},
                        nullptr, tge::Color::cyan());

        drawBoard();
        drawGhost();
        drawPiece();
        drawNext(cv);

        cv.draw_text(25, 9, " SCORE ", Color::yellow(), Color::DEFAULT());
        cv.print(25, 10, Color::white(), Color::DEFAULT(), "%6d", score.score);
        cv.draw_text(25, 12, " LEVEL ", Color::yellow(), Color::DEFAULT());
        cv.print(25, 13, Color::white(), Color::DEFAULT(), "%6d", score.level);
        cv.draw_text(25, 15, " LINES ", Color::yellow(), Color::DEFAULT());
        cv.print(25, 16, Color::white(), Color::DEFAULT(), "%6d", score.lines);

        cv.draw_text(1, h - 1,
                     " <-> move  W/Up rot  Space drop  P pause  ESC ",
                     Color::green(), Color::DEFAULT());

        if (state == State::GameOver)
            drawGameOver(cv, w, h);
        else if (paused)
            drawPaused(cv);
    }

    /* --- input --- */
    void on_event(const tge::Event &e) {
        if (e.type() == tge::EventType::Resize) {
            if (state == State::Playing)
                paused = true;
            return;
        }

        if (state == State::Title) {
            if (e.confirm() ||
                (e.type() == tge::EventType::Text && e.codepoint() == 13))
                newGame();
            else if (e.quit() || (e.type() == tge::EventType::Text &&
                                  (e.codepoint() == 'q' ||
                                   e.codepoint() == 'Q')))
                app.quit();
            return;
        }

        if (state == State::GameOver) {
            if (e.confirm() ||
                (e.type() == tge::EventType::Text && e.codepoint() == 13))
                newGame();
            else if (e.cancel() || e.quit())
                state = State::Title;
            return;
        }

        /* Playing or Paused */
        if (e.type() == tge::EventType::Text) {
            uint32_t cp = e.codepoint();
            if (cp == 'p' || cp == 'P') {
                paused = !paused;
                return;
            }
            if (paused)
                return;
            if (cp == 'w' || cp == 'W')
                rotatePressed();
            else if (cp == ' ')
                hardDrop();
        } else if (e.type() == tge::EventType::KeyDown) {
            int k = e.keycode();
            if (k == TGE_KEY_ESC) {
                paused = false;
                state = State::Title;
                return;
            }
            if (paused)
                return;
            switch (k) {
            case TGE_KEY_LEFT:
                moveCur(-1);
                break;
            case TGE_KEY_RIGHT:
                moveCur(1);
                break;
            case TGE_KEY_DOWN:
                softDrop();
                break;
            case TGE_KEY_UP:
                rotatePressed();
                break;
            case TGE_KEY_SPACE:
                hardDrop();
                break;
            default:
                break;
            }
        }
    }

    void run() {
        app.run(nullptr, update_bridge, draw_bridge, event_bridge);
    }

    /* TGE_Run / sync boundary: the only place raw TGE_App* / TGE_Canvas* /
     * TGE_Event* appear. Public so the headless harness can wire callbacks. */
    static void update_bridge(TGE_App *a, float dt) {
        static_cast<TetrisGame *>(TGE_GetUserData(a))->update(dt);
    }
    static void draw_bridge(TGE_App *a, TGE_Canvas *c) {
        static_cast<TetrisGame *>(TGE_GetUserData(a))->draw(Canvas{c});
    }
    static void event_bridge(TGE_App *a, TGE_Event *e) {
        static_cast<TetrisGame *>(TGE_GetUserData(a))
            ->on_event(tge::Event(*e));
    }
};

} // namespace tge_tetris
