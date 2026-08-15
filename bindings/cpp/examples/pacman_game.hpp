#pragma once

/* tge:: Pac-Man — C++ clone of examples/games/11_pacman.c.
 *
 * Faithful translation of the C source over the EXISTING tge:: wrappers. Every
 * game function mirrors its C counterpart 1:1; the ONLY syntax changes are those
 * forced by consuming a wrapper that already exists. Where the wrapper does NOT
 * yet cover the C API, the C call is kept verbatim and that missing projection
 * is registered as a GAP in IMPLEMENTATION_PLAN.md §6.6 (this is the whole point
 * of the probe: clones reveal GAPs).
 *
 * Explicit scope deviation (declared, not accidental):
 *  - No title TGE_Scene / tge_game_create / TGE_GameContext. The probe
 *    discipline (Snake/Sokoban) uses tge::App + the 3 TGE_Run callbacks and
 *    starts directly into RUNNING. tge::Scene / tge::Game are an OPEN GAP and are
 *    not promoted during a probe, so the title scene is folded away. ENTER-to-
 *    start is the only lost behaviour.
 *
 * Raw-C boundaries kept from the C source. The gameplay-facing wrappers
 * (Vec2i::dist2, TileMap::width/height, tge::Rect, Actor fg/bg,
 * Actor::draw(Playfield&), TileMap::load_ascii(lambda)) were promoted to tge::
 * in the post-probe passes (see IMPLEMENTATION_PLAN.md §6.7). The only remaining
 * deliberately-raw surface is the box-art wall renderer:
 *  - tge_utf8_decode + tge_grid_put (MAZE_VISUAL box-art wall decode/draw) —
 *    deliberate C escape hatch. The raw `&playfield.grid_view().grid` it needs and
 *    the `Color::*.raw` values it passes are part of *this same* hatch, not
 *    separate GAPs: it is one very specific visual-layer operation. */

#include "tge/app.hpp"
#include "tge/canvas.hpp"
#include "tge/actor.hpp"
#include "tge/playfield.hpp"
#include "tge/tilemap.hpp"
#include "tge/vec2i.hpp"
#include "tge/rect.hpp"
#include "tge/direction.hpp"
#include "tge/input.hpp"
#include "tge/input_buffer.hpp"
#include "tge/fixedstep.hpp"
#include "tge/sprite.hpp"
#include "tge/color.hpp"

#include "tge/tge_utf8.h"

#include <climits>
#include <cstdio>
#include <cstring>

namespace tge_pacman {

using tge::Vec2i;
using tge::Rect;
using tge::Direction;
using tge::Sprite;
using tge::Color;
using tge::Playfield;
using tge::GridTheme;
using tge::TileMap;
using tge::TileSet;
using tge::Actor;
using tge::FixedStep;
using tge::InputBuffer;
using tge::Canvas;

enum MazeRole : uint8_t { WALL = 0, DOOR, PELLET, POWER, TUNNEL, FLOOR };
enum PacmanState { Running, Won, Over };
enum GhostMode { SCATTER, CHASE, FRIGHTENED };
enum PacmanGhostState { ACTIVE, EATEN };

static const int MAZE_W = 28;
static const int MAZE_H = 31;
static const int GHOST_COUNT = 4;
static const float STEP_SECONDS = 0.10f;
static const float FRIGHTENED_DURATION = 7.0f;
static const float FLASH_SECONDS = 2.0f;
static const int DIRECTION_QUEUE_SIZE = 4;

/* Level-1 mode phases (Pac-Man Dossier): scatter/chase durations, then the
 * final infinite chase. mode_timer counts down only in SCATTER/CHASE;
 * FRIGHTENED pauses the phase progression. */
static const float MODE_PHASE_SECONDS[] = {
    7.0f, 20.0f, 7.0f, 20.0f, 5.0f, 20.0f, 5.0f, 1e30f,
};
static const GhostMode MODE_PHASE_KIND[] = {
    SCATTER, CHASE, SCATTER, CHASE, SCATTER, CHASE, SCATTER, CHASE,
};
static const int MODE_PHASE_COUNT = 8;

/* Blinky (ghost 0) spawns just outside the house and starts released; the
 * others spawn inside and leave through the door once released. */
static const uint8_t GHOST_COLOR_INDEX[GHOST_COUNT] = {1, 5, 6, 2};

/* The maze is the single source of truth: terrain AND entity metadata.
 * Gameplay terrain glyphs map to MazeRole via the inline legend passed to
 * TileMap::load_ascii; every other glyph (P, 1-4, H, A-D) is a level marker
 * handled by the load lambda, which sets the spawn/target cells and their
 * roles. tge_tilemap_load_ascii validates every row is exactly MAZE_W wide. */
static const char *const MAZE[MAZE_H] = {
    "B##########################A",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.#####.##.#####.######",
    "######.#####.##.#####.######",
    "######.##....1.....##.######",
    "######.##.###==###.##.######",
    "######.##.#3.2H.4#.##.######",
    "-     .   #      #   .     -",
    "######.##.########.##.######",
    "######.##..........##.######",
    "######.##.########.##.######",
    "######.##.########.##.######",
    "######.##.########.##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......P........##..o#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "D##########################C",
};

/* Sprite constants for the maze/actors (kept as raw TGE_Sprite literals). The
 * terrain legend lives inline in world_load_level's TileMap::load_ascii call. */
namespace {
const Sprite SPRITE_EMPTY(2, 1, "  ", NULL);
const Sprite SPRITE_PELLET(2, 1, "<>", "<>");
const Sprite SPRITE_POWER(2, 1, "()", "()");
const Sprite SPRITE_PACMAN(2, 1, "CC", "CC");
const Sprite SPRITE_GHOST(2, 1, "@@", "@@");

const GridTheme PACMAN_THEME(SPRITE_EMPTY, SPRITE_EMPTY, SPRITE_EMPTY);
} // namespace

/* ── Visual maze layer (faithful copy of 11_pacman.c) ─────────────────────────
 * MAZE_VISUAL is the hand-drawn wall art: 2 box-drawing codepoints per logical
 * cell. It is the ONLY source of wall appearance; the renderer draws it verbatim.
 * Gameplay connectivity lives entirely in MAZE / the TileMap. The two layers are
 * independent on purpose. maze_visual_init() decodes MAZE_VISUAL ONCE into
 * visual_sprites[y][x] indexed by logical cell. tge_utf8_decode() (unwrapped:
 * GAP) is used to walk the box-drawing codepoints. */
static const char *const MAZE_VISUAL[MAZE_H] = {
    " ╔════════════════════════╕  ╒════════════════════════╗ ",
    " ║                        │  │                        ║ ",
    " ║  ╭──────╮  ╭────────╮  │  │  ╭────────╮  ╭──────╮  ║ ",
    " ║  │      │  │        │  │  │  │        │  │      │  ║ ",
    " ║  ╰──────╯  ╰────────╯  ╰──╯  ╰────────╯  ╰──────╯  ║ ",
    " ║                                                    ║ ",
    " ║  ╭──────╮  ╭──╮  ╭──────────────╮  ╭──╮  ╭──────╮  ║ ",
    " ║  ╰──────╯  │  │  ╰─────╮  ╭─────╯  │  │  ╰──────╯  ║ ",
    " ║            │  │        │  │        │  │            ║ ",
    " ╚═════════╗  │  ╰─────╮  │  │  ╭─────╯  │  ╔═════════╝ ",
    "           ║  │  ╭─────╯  ╰──╯  ╰─────╮  │  ║           ",
    "           ║  │  │                    │  │  ║           ",
    "           ║  │  │  ╔═════━━━━═════╗  │  │  ║           ",
    "═══════════╝  ╰──╯  ║              ║  ╰──╯  ╚═══════════",
    "                    ║              ║                    ",
    "═══════════╗  ╭──╮  ║              ║  ╭──╮  ╔═══════════",
    "           ║  │  │  ╚══════════════╝  │  │  ║           ",
    "           ║  │  │                    │  │  ║           ",
    "           ║  │  │  ╭──────────────╮  │  │  ║           ",
    " ╔═════════╝  ╰──╯  ╰─────╮  ╭─────╯  ╰──╯  ╚═════════╗ ",
    " ║                        │  │                        ║ ",
    " ║  ╭──────╮  ╭────────╮  │  │  ╭────────╮  ╭──────╮  ║ ",
    " ║  ╰───╮  │  ╰────────╯  ╰──╯  ╰────────╯  │  ╭───╯  ║ ",
    " ║      │  │                                │  │      ║ ",
    " ╙───╮  │  │  ╭──╮  ╭──────────────╮  ╭──╮  │  │  ╭───╜ ",
    " ╓───╯  ╰──╯  │  │  ╰─────╮  ╭─────╯  │  │  ╰──╯  ╰───╖ ",
    " ║            │  │        │  │        │  │            ║ ",
    " ║  ╭─────────╯  ╰─────╮  │  │  ╭─────╯  ╰─────────╮  ║ ",
    " ║  ╰──────────────────╯  ╰──╯  ╰──────────────────╯  ║ ",
    " ║                                                    ║ ",
    " ╚════════════════════════════════════════════════════╝ ",
};

static char       visual_utf8[MAZE_H][MAZE_W][7];   /* 2 glyphs + NUL */
static TGE_Sprite visual_sprites[MAZE_H][MAZE_W];

static int maze_visual_width(const char *row) {
    int n = 0;
    int remaining = (int)strlen(row);
    const char *p = row;
    uint32_t cp;
    while (remaining > 0) {
        int consumed = tge_utf8_decode(p, remaining, &cp);
        if (consumed <= 0)
            break;
        p += consumed;
        remaining -= consumed;
        n++;
    }
    return n;
}

static bool maze_visual_init(void) {
    static int state = 0; /* 0 = uninitialized, 1 = ok, -1 = bad */
    if (state != 0)
        return state > 0;
    for (int y = 0; y < MAZE_H; y++) {
        int w = maze_visual_width(MAZE_VISUAL[y]);
        if (w != MAZE_W * 2) {
            fprintf(stderr,
                    "pacman: MAZE_VISUAL row %d is %d codepoints, expected %d\n",
                    y, w, MAZE_W * 2);
            state = -1;
            return false;
        }
    }
    for (int y = 0; y < MAZE_H; y++) {
        const char *row = MAZE_VISUAL[y];
        int remaining = (int)strlen(row);
        const char *p = row;
        for (int x = 0; x < MAZE_W; x++) {
            char *d = visual_utf8[y][x];
            for (int k = 0; k < 2; k++) {
                uint32_t cp;
                int consumed = tge_utf8_decode(p, remaining, &cp);
                if (consumed <= 0) {      /* malformed glyph: blank */
                    *d++ = ' ';
                    continue;
                }
                memcpy(d, p, (size_t)consumed);
                d += consumed;
                p += consumed;
                remaining -= consumed;
            }
            *d = 0;
            visual_sprites[y][x] = TGE_SPRITE(2, 1, visual_utf8[y][x], "##");
        }
    }
    state = 1;
    return true;
}

/* The ghost house block of the classic maze (rows 12-14, cols 10-17). Ghosts
 * inside it are dormant: they still move and leave, but cannot harm Pac-Man
 * until they exit. */
static const Rect GHOST_HOUSE = {10, 12, 8, 3};

struct PacmanGhost {
    Actor actor;
    Color color;
    Direction direction;
    PacmanGhostState state;
    int scatter_index;
    uint32_t rng; /* deterministic frightened wander seed */
    bool released;     /* left the house at least once (door crossing) */
    bool just_reversed;/* one-frame bypass so the forced reversal is not
                          immediately overridden by ghost_choose_direction */
};

struct PacmanPac {
    Actor actor;
    Direction direction;
};

/* Level metadata parsed from the maze markers. The maze is the single source
 * of truth: no gameplay coordinate is hardcoded elsewhere. */
struct PacmanLevel {
    Vec2i pac_spawn{};
    Vec2i ghost_spawns[GHOST_COUNT]{};
    Vec2i ghost_home{};
    Vec2i scatter_targets[GHOST_COUNT]{};
    int pellets_left = 0;
};

class Game {
public:
    tge::App &app;
    Playfield playfield;
    TileMap map;
    TileSet tiles; // palette (owning wrapper over TGE_TileSet)
    PacmanLevel level;
    PacmanPac pac;
    PacmanGhost ghosts[GHOST_COUNT];
    int score = 0;
    int lives = 3;
    GhostMode mode = SCATTER;
    float mode_timer = MODE_PHASE_SECONDS[0];
    float frightened_timer = 0.0f;
    int mode_phase = 0;
    int dots_eaten = 0;
    PacmanState state = Running;
    bool paused = false;
    bool playable = false;
    FixedStep step{STEP_SECONDS};
    InputBuffer input{DIRECTION_QUEUE_SIZE};

    explicit Game(tge::App &a) : app(a) {
        playfield.init(PACMAN_THEME, tge::GridScale::Scale2X1, MAZE_W, MAZE_H);
        playfield.set_origin(0, 1); // leave the HUD row (like 11)
        app.set_userdata(this);
        world_init();
    }

    /* --- world (PacmanWorld in 11) --- */
    void world_init() {
        tiles = world_palette();
        /* input + step are member-initialized (FixedStep/InputBuffer ctors). */
    }

    void world_resize(int gw, int gh) {
        tge::ViewUpdate view_update = playfield.update_view(gw, gh);
        playable = playfield.valid();
        switch (view_update) {
        case tge::ViewUpdate::FirstValid:
            world_reset();
            break;
        default:
            break;
        }
    }

    void world_update(float dt) {
        if (state != Running || paused || !playable)
            return;
        step.update(dt);
        while (step.next())
            world_step();
    }

    bool world_handle_input(const tge::Event &e) {
        Direction direction = Direction::from_event(e);
        if (direction == Direction::None)
            return false;
        input.push(direction);
        return true;
    }

    void world_reset() {
        world_load_level();
        world_spawn_positions();
        score = 0;
        lives = 3;
        mode = SCATTER;
        mode_phase = 0;
        mode_timer = MODE_PHASE_SECONDS[0];
        frightened_timer = 0.0f;
        dots_eaten = 0;
        state = Running;
        paused = false;
        step.reset();
        input.clear();
    }

    void world_load_level() {
        /* The loader fills terrain from the inline legend and reports every
         * other glyph (the markers) through the lambda, which sets the
         * spawn/target cells and their roles. tge_tilemap_load_ascii already
         * rejects rows that are not exactly MAZE_W wide. */
        map.init(MAZE_W, MAZE_H);
        int pac = 0, ghosts = 0, home = 0, scatter = 0;
        if (!map.load_ascii(
                MAZE, MAZE_W, MAZE_H,
                {{'#', WALL}, {'=', DOOR}, {'.', PELLET},
                 {'o', POWER}, {'-', TUNNEL}, {' ', FLOOR}},
                [&](char g, int x, int y) {
                    switch (g) {
                    case 'P':
                        level.pac_spawn = Vec2i(x, y);
                        pac++;
                        map.set(x, y, FLOOR);
                        break;
                    case '1': case '2': case '3': case '4':
                        level.ghost_spawns[g - '1'] = Vec2i(x, y);
                        ghosts++;
                        map.set(x, y, FLOOR);
                        break;
                    case 'H':
                        level.ghost_home = Vec2i(x, y);
                        home++;
                        map.set(x, y, FLOOR);
                        break;
                    case 'A': case 'B': case 'C': case 'D':
                        level.scatter_targets[g - 'A'] = Vec2i(x, y);
                        scatter++;
                        map.set(x, y, WALL);
                        break;
                    }
                })) {
            fprintf(stderr, "pacman: failed to load MAZE (row width or marker "
                            "without a legend entry)\n");
            return;
        }
        /* Every marker must appear exactly once: the maze is the level, so a
         * missing or duplicated marker is an authoring error. */
        if (pac != 1 || ghosts != GHOST_COUNT || home != 1 ||
            scatter != GHOST_COUNT) {
            fprintf(stderr,
                    "pacman: expected 1 'P', %d '1'-'4', 1 'H' and %d 'A'-'D' "
                    "markers, found %d/%d/%d/%d\n",
                    GHOST_COUNT, GHOST_COUNT, pac, ghosts, home, scatter);
            return;
        }
        level.pellets_left = map.count(PELLET) + map.count(POWER);
    }

    void world_spawn_positions() {
        pac.actor.set_position(level.pac_spawn);
        pac.actor.set_sprite(SPRITE_PACMAN.ptr());
        pac.actor.set_fg(Color::yellow());
        pac.actor.set_bg(Color::DEFAULT());
        /* Pac-Man idles until the first direction input. */
        pac.direction = Direction::None;

        for (int i = 0; i < GHOST_COUNT; i++) {
            PacmanGhost &g = ghosts[i];
            g.actor.set_position(level.ghost_spawns[i]);
            g.actor.set_sprite(SPRITE_GHOST.ptr());
            g.color = Color::indexed(GHOST_COLOR_INDEX[i]);
            g.actor.set_fg(g.color);
            g.actor.set_bg(Color::DEFAULT());
            g.direction = (i == 0) ? Direction::Left : Direction::Down;
            g.state = ACTIVE;
            g.rng = 0x13579BDFu + (uint32_t)i * 0x9E3779B9u;
            g.scatter_index = i;
            g.released = (i <= 1);
            g.just_reversed = false;
        }
    }

    TileSet world_palette() {
        /* WALL/DOOR sprites stay NULL: they are drawn as connected box-art by
         * renderer_draw_walls, not by the tilemap palette. */
        TileSet pal;
        pal.clear_sprite(WALL);
        pal.clear_sprite(DOOR);
        pal.set(PELLET, SPRITE_PELLET, Color::white(), Color::DEFAULT());
        pal.set(POWER, SPRITE_POWER, Color::white(), Color::DEFAULT());
        return pal;
    }

    /* --- simulation step (world_step in 11) --- */
    void world_step() {
        Direction direction;
        if (input.pop(direction)) {
            /* The queued direction is a turn intent: it only takes effect when
             * the adjacent cell is open. */
            Vec2i want = step_position(map, pac.actor.position(), direction,
                                       true);
            if (!(want == pac.actor.position()))
                pac.direction = direction;
        }

        Vec2i pac_from = pac.actor.position();
        Vec2i next = step_position(map, pac.actor.position(), pac.direction,
                                   true);
        if (!(next == pac.actor.position())) {
            pac.actor.set_position(next);
            eat_cell();
        }

        if (state == Won)
            return;

        Vec2i ghost_from[GHOST_COUNT];
        for (int i = 0; i < GHOST_COUNT; i++) {
            PacmanGhost &g = ghosts[i];
            ghost_from[i] = g.actor.position();
            Direction ghost_direction;
            if (g.just_reversed) {
                /* Honor the forced reversal for exactly one step. */
                ghost_direction = g.direction;
                g.just_reversed = false;
            } else {
                ghost_direction = ghost_choose_direction(g);
            }
            g.direction = ghost_direction;
            g.actor.set_position(
                step_position(map, g.actor.position(), ghost_direction, false));
            if (g.state == EATEN &&
                g.actor.position() == level.ghost_home) {
                /* The eyes made it home: respawn exactly on arrival. */
                g.state = ACTIVE;
                g.direction = Direction::Down;
                g.released = true;
            }
        }

        for (int i = 0; i < GHOST_COUNT; i++) {
            PacmanGhost &g = ghosts[i];
            if (g.state == EATEN)
                continue;
            /* Ghosts inside the ghost house are dormant. */
            if (cell_in_ghost_house(g.actor.position().x(),
                                    g.actor.position().y()))
                continue;
            bool same_cell =
                g.actor.position() == pac.actor.position();
            /* Head-on swap: both moved into each other's previous cell. */
            bool crossing =
                g.actor.position() == pac_from &&
                ghost_from[i] == pac.actor.position();
            if (same_cell || crossing) {
                if (mode == FRIGHTENED)
                    eat_ghost(g);
                else
                    world_die();
                break;
            }
        }

        if (mode == FRIGHTENED) {
            frightened_timer -= STEP_SECONDS;
            if (frightened_timer <= 0.0f) {
                /* Resume the paused Scatter/Chase phase unchanged. */
                mode = MODE_PHASE_KIND[mode_phase];
            }
        } else {
            mode_timer -= STEP_SECONDS;
            if (mode_timer <= 0.0f) {
                if (mode_phase < MODE_PHASE_COUNT - 1) {
                    mode_phase++;
                    mode = MODE_PHASE_KIND[mode_phase];
                    mode_timer = MODE_PHASE_SECONDS[mode_phase];
                    /* Every scatter<->chase transition forces reversal. */
                    force_ghost_reversal();
                } else {
                    mode_timer = MODE_PHASE_SECONDS[mode_phase];
                }
            }
        }
    }

    void eat_cell() {
        Vec2i p = pac.actor.position();
        uint8_t role = map.get(p.x(), p.y());
        if (role == PELLET) {
            map.set(p.x(), p.y(), FLOOR);
            score += 10;
            level.pellets_left--;
            dots_eaten++;
            /* House release keyed off pellets eaten this level. */
            if (dots_eaten >= 30)
                ghosts[2].released = true;
            if (dots_eaten >= 60)
                ghosts[3].released = true;
            if (level.pellets_left == 0)
                state = Won;
        } else if (role == POWER) {
            map.set(p.x(), p.y(), FLOOR);
            score += 50;
            level.pellets_left--;
            if (level.pellets_left == 0) {
                state = Won;
            } else {
                mode = FRIGHTENED;
                frightened_timer = FRIGHTENED_DURATION;
                force_ghost_reversal();
            }
        }
    }

    void eat_ghost(PacmanGhost &g) {
        score += 200;
        g.state = EATEN; /* becomes "eyes", walks home, respawns on arrival */
    }

    void world_die() {
        lives--;
        if (lives <= 0) {
            state = Over;
            return;
        }
        world_spawn_positions();
        mode = SCATTER;
        mode_phase = 0;
        mode_timer = MODE_PHASE_SECONDS[0];
        frightened_timer = 0.0f;
        /* dots_eaten is preserved across a death within the same level. */
    }

    /* xorshift32: deterministic pseudo-random wander for frightened ghosts. */
    static uint32_t ghost_rng_next(PacmanGhost &g) {
        uint32_t x = g.rng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        g.rng = x;
        return x;
    }

    Direction ghost_choose_direction(PacmanGhost &g) {
        /* The arcade tie-break order: up, left, down, right. */
        static const Direction PREF[4] = {
            Direction::Up, Direction::Left, Direction::Down, Direction::Right,
        };

        if (mode == FRIGHTENED && g.state == ACTIVE) {
            /* Frightened ghosts wander: any open direction except the reverse,
             * picked pseudo-randomly instead of chasing a target. */
            Direction open[4];
            int count = 0;
            for (int i = 0; i < 4; i++) {
                Direction candidate = PREF[i];
                if (candidate == g.direction.opposite())
                    continue;
                Vec2i next =
                    g.actor.position() + candidate.to_vec();
                if (ghost_can_enter(g, next))
                    open[count++] = candidate;
            }
            if (count == 0)
                return g.direction;
            return open[ghost_rng_next(g) % count];
        }

        Vec2i target = ghost_target(g);
        Direction best = g.direction;
        int best_distance = INT_MAX;
        for (int i = 0; i < 4; i++) {
            Direction candidate = PREF[i];
            /* No 180-degree turn during normal cornering, EXCEPT inside the
             * ghost house, where a released ghost must be allowed to reverse. */
            if (candidate == g.direction.opposite() &&
                !cell_in_ghost_house(g.actor.position().x(),
                                     g.actor.position().y()))
                continue;
            Vec2i next = g.actor.position() + candidate.to_vec();
            if (ghost_can_enter(g, next)) {
                int distance = next.dist2(target);
                if (distance < best_distance) {
                    best_distance = distance;
                    best = candidate;
                }
            }
        }
        return best;
    }

    Vec2i ghost_target(const PacmanGhost &g) const {
        if (g.state == EATEN)
            return level.ghost_home; /* eyes walk back to the house */

        if (cell_in_ghost_house(g.actor.position().x(),
                                g.actor.position().y())) {
            if (!g.released)
                return level.ghost_home;
            return Vec2i(level.ghost_home.x(), level.ghost_home.y() - 2);
        }

        if (!g.released)
            return level.ghost_home; /* safety: unreleased never chases */

        if (mode == SCATTER)
            return level.scatter_targets[g.scatter_index];

        /* CHASE: per-ghost personality from the Pac-Man Dossier. */
        Vec2i p = pac.actor.position();
        Direction dir = pac.direction;
        switch (g.scatter_index) {
        case 0: /* Blinky: directly on Pac-Man */
            return p;
        case 1: { /* Pinky: four tiles ahead, with the classic UP overflow bug */
            if (dir == Direction::None)
                return p;
            Vec2i t = p;
            for (int i = 0; i < 4; i++)
                t = t + dir.to_vec();
            if (dir == Direction::Up)
                t = Vec2i(p.x() - 4, p.y() - 4);
            return t;
        }
        case 2: { /* Inky: reflection of Blinky through the tile two ahead */
            if (dir == Direction::None)
                return p;
            Vec2i tile2 = p;
            for (int i = 0; i < 2; i++)
                tile2 = tile2 + dir.to_vec();
            Vec2i blinky = ghosts[0].actor.position();
            Vec2i lead = tile2 - blinky;
            return tile2 + lead; /* 2*tile2 - blinky */
        }
        case 3: { /* Clyde: chase when far, flee to corner when close */
            Vec2i rel = p - g.actor.position();
            if (rel.x() * rel.x() + rel.y() * rel.y() > 64)
                return p;
            return level.scatter_targets[3];
        }
        }
        return p;
    }

    bool ghost_can_enter(const PacmanGhost &g, Vec2i cell) const {
        if (cell.x() < 0 || cell.y() < 0 || cell.x() >= map.width() ||
            cell.y() >= map.height())
            return false;
        uint8_t role = map.get(cell.x(), cell.y());
        if (role == WALL)
            return false; /* walls block everyone */
        if (role == DOOR) {
            /* The door blocks unreleased ghosts; eyes and released cross. */
            if (g.state == EATEN)
                return true;
            if (g.state == ACTIVE && g.released)
                return true;
            return false;
        }
        return true; /* floor, tunnel, pellet, power */
    }

    void force_ghost_reversal() {
        for (int i = 0; i < GHOST_COUNT; i++) {
            PacmanGhost &g = ghosts[i];
            if (g.state == ACTIVE) {
                g.direction = g.direction.opposite();
                g.just_reversed = true;
            }
        }
    }

    static bool cell_in_ghost_house(int x, int y) {
        return GHOST_HOUSE.contains(x, y);
    }

    Vec2i step_position(const TileMap &m, Vec2i p, Direction direction,
                        bool pacman) const {
        Vec2i next = p + direction.to_vec();

        /* The tunnel wraps horizontally. Any other out-of-bounds move is a
         * wall. */
        if (next.x() < 0 || next.x() >= m.width()) {
            if (m.get(p.x(), p.y()) == TUNNEL) {
                next = Vec2i(next.x() < 0 ? m.width() - 1 : 0, next.y());
            } else {
                return p;
            }
        }
        if (next.y() < 0 || next.y() >= m.height())
            return p;

        uint8_t role = m.get(next.x(), next.y());
        if (role == WALL)
            return p;
        /* The door blocks Pac-Man from re-entering the house but lets him leave
         * through it (moving up). Ghosts ignore it entirely. */
        if (pacman && role == DOOR && direction != Direction::Up)
            return p;
        return next;
    }

    /* --- update / draw / event (the 3 TGE_Run callbacks, via bridges) --- */
    void update(float dt) {
        Canvas cv = app.canvas();
        playfield.sync(cv.width(), cv.height(), resize_bridge, this);
        world_update(dt);
    }

    void draw(Canvas cv) {
        playfield.attach(cv);
        cv.clear(' ', Color::black(), Color::DEFAULT());
        cv.print(1, 0, Color::yellow(), Color::DEFAULT(),
                 " SCORE: %d  LIVES: %d ", score, lives);
        playfield.draw_border(Color::cyan(), Color::DEFAULT());

        if (!playfield.valid()) {
            cv.draw_centered_text(cv.height() / 2, " too small ",
                                  Color::red(), Color::DEFAULT());
            return;
        }

        map.draw(&playfield.grid_view().grid, playfield.origin_x(),
                 playfield.origin_y(), tiles);
        renderer_draw_walls();
        renderer_draw_actors(cv);

        if (state == Over) {
            cv.draw_modal(" GAME OVER ",
                          " [ENTER] restart  [ESC] menu  [Q] quit ",
                          Color::red());
        } else if (state == Won) {
            cv.draw_modal(" YOU WIN! ",
                          " [ENTER] restart  [ESC] menu  [Q] quit ",
                          Color::green());
        } else if (paused) {
            cv.draw_modal(" PAUSED ", " [P] resume ", Color::yellow());
        }
    }

    void on_event(const tge::Event &e) {
        if (e.type() == tge::EventType::Resize) {
            if (state != Over)
                paused = true; // pause after a valid resize (like 11)
            return;            // playfield.sync in update() handles canvas changes
        }
        if (e.pause()) {
            if (state != Over)
                paused = !paused;
            return;
        }
        if (paused && !e.cancel())
            return;
        if (world_handle_input(e))
            return;
        if (e.quit()) {
            if (state == Over)
                app.quit();
            return;
        }
        if (e.confirm()) {
            if (state == Over || state == Won)
                world_reset();
            return;
        }
        if (e.cancel())
            app.quit(); // no scene stack -> quit to exit
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
    void renderer_draw_walls() {
        if (!maze_visual_init())
            return;
        for (int y = 0; y < map.height(); y++) {
            for (int x = 0; x < map.width(); x++) {
                uint8_t role = map.get(x, y);
                if (role != WALL && role != DOOR)
                    continue;
                /* GAP: tge_grid_put is not wrapped; C draws walls verbatim. */
                tge_grid_put(&playfield.grid_view().grid,
                             playfield.origin_x() + x, playfield.origin_y() + y,
                             &visual_sprites[y][x], Color::blue().raw,
                             Color::DEFAULT().raw);
            }
        }
    }

    void renderer_draw_actors(Canvas &cv) {
        (void)cv;
        bool frightened = mode == FRIGHTENED;
        bool flash = frightened && frightened_timer < FLASH_SECONDS &&
                     ((int)(frightened_timer * 4) % 2 == 0);

        pac.actor.draw(playfield);

        for (int i = 0; i < GHOST_COUNT; i++) {
            const PacmanGhost &g = ghosts[i];
            Actor copy = g.actor;
            if (g.state == EATEN) {
                copy.set_fg(Color::white()); /* eyes, walking home */
            } else if (frightened) {
                copy.set_fg(flash ? Color::white() : Color::blue());
            }
            copy.draw(playfield);
        }
    }
};

} // namespace tge_pacman
