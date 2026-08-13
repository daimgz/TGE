#include "tge/tge.h"
#include "tge/tge_math.h"
#include "tge/tge_utf8.h"

#include "tge-extra/actor.h"
#include "tge-extra/direction.h"
#include "tge-extra/fixedstep.h"
#include "tge-extra/game.h"
#include "tge-extra/input.h"
#include "tge-extra/input_buffer.h"
#include "tge-extra/playfield.h"
#include "tge-extra/tilemap.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAZE_W 28
#define MAZE_H 31
#define GHOST_COUNT 4
#define STEP_SECONDS 0.10f
#define FRIGHTENED_DURATION 7.0f
#define FLASH_SECONDS 2.0f
#define DIRECTION_QUEUE_SIZE 4

typedef enum {
    ROLE_WALL = 0,
    ROLE_DOOR,
    ROLE_PELLET,
    ROLE_POWER,
    ROLE_TUNNEL,
    ROLE_FLOOR
} MazeRole;

/* The maze is the single source of truth for the level: terrain AND entity
 * metadata. Each row is one string; the legend maps terrain glyphs to roles:
 *   '#' wall (blocks everyone)
 *   '=' ghost-house door (blocks Pac-Man unless he is leaving the house)
 *   '.' pellet, 'o' power pellet
 *   '-' tunnel (walkable, the playfield edges wrap through it)
 *   ' ' floor (exterior and the ghost house interior, no pellet)
 * Everything else is a level marker (see the PACMAN_LEGEND / level_marker
 * notes): 'P' Pac-Man spawn, '1'-'4' ghost spawns, 'H' ghost home, and
 * 'A'-'D' scatter targets on the frame corners. tge_tilemap_load_ascii()
 * validates that every row is exactly MAZE_W wide, so a maze edit that breaks
 * the dimensions fails loudly instead of corrupting the game.
 *
 *
  ╔════════════════════════╕  ╒════════════════════════╗
 ║                        │  │                        ║
 ║  ╭──────╮  ╭────────╮  │  │  ╭────────╮  ╭──────╮  ║
 ║  │      │  │        │  │  │  │        │  │      │  ║
 ║  ╰──────╯  ╰────────╯  ╰──╯  ╰────────╯  ╰──────╯  ║
 ║                                                    ║
 ║  ╭──────╮  ╭──╮  ╭──────────────╮  ╭──╮  ╭──────╮  ║
 ║  ╰──────╯  │  │  ╰─────╮  ╭─────╯  │  │  ╰──────╯  ║
 ║            │  │        │  │        │  │            ║
 ╚═════════╗  │  ╰─────╮  │  │  ╭─────╯  │  ╔═════════╝
           ║  │  ╭─────╯  ╰──╯  ╰─────╮  │  ║
           ║  │  │                    │  │  ║
           ║  │  │  ╔═════━━━━═════╗  │  │  ║
═══════════╝  ╰──╯  ║              ║  ╰──╯  ╚═══════════
                    ║              ║
═══════════╗  ╭──╮  ║              ║  ╭──╮  ╔═══════════
           ║  │  │  ╚══════════════╝  │  │  ║
           ║  │  │                    │  │  ║
           ║  │  │  ╭──────────────╮  │  │  ║
 ╔═════════╝  ╰──╯  ╰─────╮  ╭─────╯  ╰──╯  ╚═════════╗
 ║                        │  │                        ║
 ║  ╭──────╮  ╭────────╮  │  │  ╭────────╮  ╭──────╮  ║
 ║  ╰───╮  │  ╰────────╯  ╰──╯  ╰────────╯  │  ╭───╯  ║
 ║      │  │                                │  │      ║
 ╙───╮  │  │  ╭──╮  ╭──────────────╮  ╭──╮  │  │  ╭───╜
 ╓───╯  ╰──╯  │  │  ╰─────╮  ╭─────╯  │  │  ╰──╯  ╰───╖
 ║            │  │        │  │        │  │            ║
 ║  ╭─────────╯  ╰─────╮  │  │  ╭─────╯  ╰─────────╮  ║
 ║  ╰──────────────────╯  ╰──╯  ╰──────────────────╯  ║
 ║                                                    ║
 ╚════════════════════════════════════════════════════╝

 */
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
    "D##########################C"
};

/* Gameplay terrain legend. Characters not listed here are level markers
 * handled by level_marker() (P, 1-4, H, A-D). The tilemap stays game-agnostic.
 * MAZE is the sole source of gameplay state; MAZE_VISUAL is completely
 * independent and exists only for rendering. */
static const TGE_TileLegend PACMAN_LEGEND[] = {
    { '#', ROLE_WALL },
    { '=', ROLE_DOOR },
    { '.', ROLE_PELLET },
    { 'o', ROLE_POWER },
    { '-', ROLE_TUNNEL },
    { ' ', ROLE_FLOOR },
};

typedef enum { PACMAN_RUNNING, PACMAN_WON, PACMAN_OVER } PacmanState;

typedef enum { GHOST_SCATTER, GHOST_CHASE, GHOST_FRIGHTENED } GhostMode;

/* Level-1 mode phases (Pac-Man Dossier): scatter/chase durations, then the
 * final infinite chase. mode_timer counts down only in SCATTER/CHASE; FRIGHTENED
 * pauses the phase progression. */
static const float MODE_PHASE_SECONDS[] = {
    7.0f, 20.0f, 7.0f, 20.0f, 5.0f, 20.0f, 5.0f, 1e30f,
};
static const GhostMode MODE_PHASE_KIND[] = {
    GHOST_SCATTER, GHOST_CHASE, GHOST_SCATTER, GHOST_CHASE,
    GHOST_SCATTER, GHOST_CHASE, GHOST_SCATTER, GHOST_CHASE,
};
#define MODE_PHASE_COUNT 8

typedef enum { GHOST_ACTIVE, GHOST_EATEN } PacmanGhostState;

typedef struct {
    TGE_Actor actor;
    TGE_Color color;
    TGE_Direction direction;
    PacmanGhostState state; /* ACTIVE, or EATEN walking back to the house */
    int scatter_index;
    uint32_t rng; /* deterministic frightened wander seed (ghost_rng_next) */
    bool released;       /* left the house at least once (door crossing) */
    bool just_reversed;  /* one-frame bypass so the forced reversal is not
                            immediately overridden by ghost_choose_direction */
} PacmanGhost;

typedef struct {
    TGE_Actor actor;
    TGE_Direction direction;
} PacmanPac;

/* Level metadata parsed from the maze markers. The maze is the single source
 * of truth: no gameplay coordinate is hardcoded elsewhere. */
typedef struct {
    TGE_Vec2i pac_spawn;
    TGE_Vec2i ghost_spawns[GHOST_COUNT];
    TGE_Vec2i ghost_home;
    TGE_Vec2i scatter_targets[GHOST_COUNT];
    int pellets_left;
} PacmanLevel;

typedef struct {
    TGE_TileMap map;
    TGE_TileSet tiles;
    PacmanLevel level;
    PacmanPac pac;
    PacmanGhost ghosts[GHOST_COUNT];
    int score;
    int lives;
    GhostMode mode;
    float mode_timer;       /* remaining time in the current SCATTER/CHASE phase */
    float frightened_timer;  /* remaining time in FRIGHTENED (pauses mode_timer) */
    int mode_phase;   /* index into MODE_PHASE_SECONDS / MODE_PHASE_KIND */
    int dots_eaten;   /* pellets eaten this level (drives house release) */
    PacmanState state;
    bool paused;
    bool playable; /* view is valid (see world_resize) */
    TGE_FixedStep step;
    TGE_InputBuffer input;
} PacmanWorld;

typedef struct {
    TGE_GameContext ctx;
    TGE_Playfield pf; /* the shared Snake/Breakout infra: view + grid + layout */
    PacmanWorld world;
} PacmanGame;

static TGE_App *g_app = NULL;

static void game_event  (TGE_GameContext *ctx, TGE_Event *event);
static void game_update (TGE_GameContext *ctx, float delta_time);
static void game_draw   (TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_destroy(TGE_GameContext *ctx);
static void game_world_resize(void *userdata, int grid_width,
                              int grid_height);

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *event);

static void world_init    (PacmanWorld *world);
static void world_reset   (PacmanWorld *world);
static void world_resize  (PacmanWorld *world, TGE_View *view, int grid_width,
                           int grid_height);
static void world_update  (PacmanWorld *world, float delta_time);
static void world_step    (PacmanWorld *world);
static bool world_handle_input(PacmanWorld *world, TGE_Event *event);
static void world_load_level    (PacmanWorld *world);
static void level_marker(void *userdata, char marker, int x, int y);
static bool cell_in_ghost_house(int x, int y);
static void world_spawn_positions(PacmanWorld *world);
static void world_die         (PacmanWorld *world);
static TGE_TileSet world_palette(void);
static void eat_cell  (PacmanWorld *world);
static void eat_ghost (PacmanWorld *world, PacmanGhost *g);
static TGE_Direction ghost_choose_direction(PacmanWorld *world,
                                            PacmanGhost *g);
static TGE_Vec2i ghost_target(const PacmanWorld *world, const PacmanGhost *g);
static bool ghost_can_enter(const PacmanWorld *world, const PacmanGhost *g,
                            TGE_Vec2i cell);
static void force_ghost_reversal(PacmanWorld *world);
static TGE_Vec2i step_position(const TGE_TileMap *map, TGE_Vec2i p,
                               TGE_Direction direction, bool pacman);

static void renderer_draw(TGE_Playfield *pf, const PacmanWorld *world,
                          TGE_Canvas *canvas);
static void renderer_draw_actors(TGE_Playfield *pf, const PacmanWorld *world);

static const TGE_Sprite SPRITE_EMPTY   = TGE_SPRITE(2, 1, "  ", NULL);
static const TGE_Sprite SPRITE_PELLET  = TGE_SPRITE(2, 1, "<>", "<>");
static const TGE_Sprite SPRITE_POWER   = TGE_SPRITE(2, 1, "()", "()");
static const TGE_Sprite SPRITE_PACMAN  = TGE_SPRITE(2, 1, "CC", "CC");
static const TGE_Sprite SPRITE_GHOST   = TGE_SPRITE(2, 1, "@@", "@@");

/* ── Visual maze layer ───────────────────────────────────────────────────────
 * MAZE_VISUAL is the hand-drawn wall art: 2 box-drawing codepoints per logical
 * cell, MAZE_W*2 display columns per row. It is the ONLY source of wall
 * appearance; the renderer draws it verbatim. Gameplay connectivity lives
 * entirely in MAZE_LOGIC / the TileMap (roles queried via tge_tilemap_get). The
 * two layers are
 * independent on purpose: a logical wall may be drawn as a gap, and the
 * renderer never questions the art.
 *
 * maze_visual_init() decodes MAZE_VISUAL ONCE into visual_sprites[y][x],
 * indexed by logical cell (never by raw byte offset), so the hot render loop
 * does no UTF-8 walking. MAZE_VISUAL is the hand-drawn, column-exact 28x56 wall
 * art: each row contains exactly 56 Unicode codepoints (two display glyphs per
 * logical maze cell). */

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

/* ── MAZE_VISUAL decode (box-drawing codepoints are 3 bytes) ─────────────────
 * We rely on the library's tge_utf8_decode() instead of a private decoder. The
 * raw glyph bytes are copied verbatim into visual_utf8[y][x] (no re-encode): the
 * art is already valid UTF-8, and tge_utf8_decode() returns -1 on malformed
 * input, which we treat as a blank cell. */

static char       visual_utf8[MAZE_H][MAZE_W][7];   /* 2 glyphs + NUL */
static TGE_Sprite visual_sprites[MAZE_H][MAZE_W];

static int maze_visual_width(const char *row)
{
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

static bool maze_visual_init(void)
{
    static int state = 0;   /* 0 = uninitialized, 1 = ok, -1 = bad */
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
                if (consumed <= 0) {       /* malformed glyph: blank */
                    *d++ = ' ';
                    continue;
                }
                memcpy(d, p, (size_t)consumed);
                d += consumed;
                p += consumed;
                remaining -= consumed;
            }
            *d = 0;
            visual_sprites[y][x] = (TGE_Sprite){ 2, 1, visual_utf8[y][x], "##" };
        }
    }
    state = 1;
    return true;
}

static void renderer_draw_walls(TGE_Playfield *pf, const PacmanWorld *world)
{
    if (!maze_visual_init())
        return;
    for (int y = 0; y < world->map.height; y++) {
        for (int x = 0; x < world->map.width; x++) {
            uint8_t role = tge_tilemap_get(&world->map, x, y);
            if (role != ROLE_WALL && role != ROLE_DOOR)
                continue;
            tge_grid_put(&pf->grid_view.grid, pf->view.area.x + x,
                         pf->view.area.y + y, &visual_sprites[y][x],
                         TGE_COLOR_BLUE, TGE_COLOR_DEFAULT);
        }
    }
}

static const TGE_GridTheme PACMAN_THEME = {
    .empty          = &SPRITE_EMPTY,
    .default_sprite = &SPRITE_EMPTY,
    .border         = &SPRITE_EMPTY,
};

static const TGE_GameCallbacks pacman_callbacks = {
    .update  = game_update,
    .draw    = game_draw,
    .event   = game_event,
    .destroy = game_destroy,
};

/* Blinky (ghost 0) spawns just outside the house, above the door, and starts
 * released; the others spawn inside and leave through the door once released
 * (see ghost_can_enter / dots_eaten). Inside-house ghosts are dormant: they
 * cannot harm Pac-Man until they exit (see cell_in_ghost_house). All positions
 * come from the maze markers via level_marker(). */
static const uint8_t GHOST_COLOR_INDEX[GHOST_COUNT] = { 1, 5, 6, 2 };

#if defined(TGE_PACMAN_TEST)
static int pacman_main(void)
#else
int main(void)
#endif
{
    TGE_App *app = TGE_Create(
        2 * (MAZE_W + 2),
        MAZE_H + 3,
        "TGE Pac-Man"
    );
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *title = NULL;
    tge_scene_create(&title, 0, NULL, title_draw, title_event, NULL);
    title->opaque = false;
    TGE_PushScene(app, title);
}

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int canvas_width = tge_canvas_width(canvas);
    int canvas_height = tge_canvas_height(canvas);

    const char *title    = " PAC-MAN ";
    const char *subtitle = " tilemap maze, ghost actors, playfield grid ";
    const char *controls = " Arrows/WASD move  [P] pause ";
    const char *start    = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, canvas_width, canvas_height   , TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 - 4, title   , TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 - 2, subtitle, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 + 1, controls, TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 + 3, start   , TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);

    TGE_GridView grid_view;
    tge_grid_view_init(&grid_view, &PACMAN_THEME, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&grid_view, canvas);
    tge_grid_set_origin(&grid_view.grid, 0, 15);
    tge_grid_view_put(&grid_view, 7, 0, &SPRITE_PACMAN, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_grid_view_put(&grid_view, 11, 0, &SPRITE_GHOST, TGE_COLOR_RED    , TGE_COLOR_DEFAULT);
}

static void title_event(TGE_Scene *scene, TGE_Event *event)
{
    (void)scene;
    if (tge_input_cancel(event) || tge_input_quit(event)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(event)) {
        PacmanGame *game = (PacmanGame *)tge_game_create(
            g_app, sizeof(PacmanGame), &pacman_callbacks);
        if (!game)
            return;
        tge_playfield_init(&game->pf, &PACMAN_THEME, TGE_GRID_SCALE_2X1,
                           MAZE_W, MAZE_H);
        tge_grid_set_origin(&game->pf.grid_view.grid, 0, 1);
        world_init(&game->world);
    }
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    PacmanGame *game = (PacmanGame *)tge_game_instance(ctx);
    tge_playfield_sync(&game->pf, tge_canvas_width(canvas),
                       tge_canvas_height(canvas), game_world_resize, game);
    renderer_draw(&game->pf, &game->world, canvas);
}

static void game_update(TGE_GameContext *ctx, float delta_time)
{
    PacmanGame *game = (PacmanGame *)tge_game_instance(ctx);
    world_update(&game->world, delta_time);
}

static void game_event(TGE_GameContext *ctx, TGE_Event *event)
{
    PacmanGame *game = (PacmanGame *)tge_game_instance(ctx);
    PacmanWorld *world = &game->world;

    if (event->type == TGE_EVENT_RESIZE) {
        tge_playfield_sync(&game->pf, event->data.resize.w,
                           event->data.resize.h, game_world_resize, game);
        if (world->state != PACMAN_OVER)
            world->paused = true;
        return;
    }
    if (tge_input_pause(event)) {
        if (world->state != PACMAN_OVER)
            world->paused = !world->paused;
        return;
    }
    if (world->paused && !tge_input_cancel(event))
        return;
    if (world_handle_input(world, event))
        return;
    if (tge_input_quit(event)) {
        if (world->state == PACMAN_OVER)
            TGE_Quit(ctx->app);
        return;
    }
    if (tge_input_confirm(event)) {
        if (world->state == PACMAN_OVER || world->state == PACMAN_WON)
            world_reset(world);
        return;
    }
    if (tge_input_cancel(event))
        TGE_PopScene(ctx->app);
}

static void game_destroy(TGE_GameContext *ctx)
{
    (void)ctx; /* the world is all embedded; nothing was allocated */
}

static void game_world_resize(void *userdata, int grid_width, int grid_height)
{
    PacmanGame *game = (PacmanGame *)userdata;
    world_resize(&game->world, &game->pf.view, grid_width, grid_height);
}

static void renderer_draw(TGE_Playfield *pf, const PacmanWorld *world,
                          TGE_Canvas *canvas)
{
    tge_playfield_attach(pf, canvas);
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT,
               " SCORE: %d  LIVES: %d ", world->score, world->lives);
    tge_playfield_draw_border(pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);

    if (!pf->view.valid) {
        tge_draw_centered_text(canvas, tge_canvas_height(canvas) / 2,
                               " too small ", TGE_COLOR_RED,
                               TGE_COLOR_DEFAULT);
        return;
    }

    tge_tilemap_draw(&world->map, &pf->grid_view.grid, pf->view.area.x,
                     pf->view.area.y, &world->tiles);
    renderer_draw_walls(pf, world);
    renderer_draw_actors(pf, world);

    if (world->state == PACMAN_OVER) {
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    } else if (world->state == PACMAN_WON) {
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " YOU WIN! ", again, TGE_COLOR_GREEN);
    } else if (world->paused) {
        const char *again = " [P] resume ";
        tge_draw_modal(canvas, " PAUSED ", again, TGE_COLOR_YELLOW);
    }
}

static void renderer_draw_actors(TGE_Playfield *pf, const PacmanWorld *world)
{
    bool frightened = world->mode == GHOST_FRIGHTENED;
    bool flash = frightened && world->frightened_timer < FLASH_SECONDS &&
                 ((int)(world->frightened_timer * 4) % 2 == 0);

    tge_actor_draw(&pf->grid_view, &pf->view, &world->pac.actor);

    for (int i = 0; i < GHOST_COUNT; i++) {
        const PacmanGhost *g = &world->ghosts[i];
        TGE_Actor copy = g->actor;
        if (g->state == GHOST_EATEN) {
            copy.fg = TGE_COLOR_WHITE; /* eyes, walking home */
        } else if (frightened) {
            copy.fg = flash ? TGE_COLOR_WHITE : TGE_COLOR_BLUE;
        }
        tge_actor_draw(&pf->grid_view, &pf->view, &copy);
    }
}

static void world_init(PacmanWorld *world)
{
    memset(world, 0, sizeof(*world));
    world->tiles = world_palette();
    tge_input_buffer_init(&world->input, DIRECTION_QUEUE_SIZE);
    tge_fixedstep_init(&world->step, STEP_SECONDS);
}

static void world_resize(PacmanWorld *world, TGE_View *view, int grid_width,
                         int grid_height)
{
    TGE_ViewUpdate view_update = tge_view_update(view, grid_width,
                                                 grid_height);
    world->playable = view->valid;

    switch (view_update) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(world);
        break;
    case TGE_VIEW_RESIZED:
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static void world_update(PacmanWorld *world, float delta_time)
{
    if (world->state != PACMAN_RUNNING || world->paused || !world->playable)
        return;
    tge_fixedstep_update(&world->step, delta_time);
    while (tge_fixedstep_next(&world->step))
        world_step(world);
}

static bool world_handle_input(PacmanWorld *world, TGE_Event *event)
{
    TGE_Direction direction = tge_input_direction(event);
    if (direction == TGE_DIR_NONE)
        return false;
    tge_input_buffer_push(&world->input, direction);
    return true;
}

static void world_reset(PacmanWorld *world)
{
    world_load_level(world);
    world_spawn_positions(world);
    world->score = 0;
    world->lives = 3;
    world->mode = GHOST_SCATTER;
    world->mode_phase = 0;
    world->mode_timer = MODE_PHASE_SECONDS[0];
    world->frightened_timer = 0.0f;
    world->dots_eaten = 0;
    world->state = PACMAN_RUNNING;
    world->paused = false;
    tge_fixedstep_reset(&world->step);
    tge_input_buffer_clear(&world->input);
}

/* Marker occurrence counts, so world_load_level can enforce that each marker
 * appears exactly once. */
typedef struct {
    PacmanWorld *world;
    int pac, ghosts, home, scatter;
} LevelLoadCtx;

static void world_load_level(PacmanWorld *world)
{
    /* The loader fills terrain from PACMAN_LEGEND and reports every other
     * glyph (the markers) through level_marker, which sets the spawn/target
     * cells and their roles. tge_tilemap_load_ascii already rejects rows that
     * are not exactly MAZE_W wide. */
    LevelLoadCtx ctx = { .world = world };

    tge_tilemap_init(&world->map, MAZE_W, MAZE_H);
    if (!tge_tilemap_load_ascii(&world->map, MAZE, MAZE_W, MAZE_H,
                                PACMAN_LEGEND,
                                (int)(sizeof(PACMAN_LEGEND) /
                                      sizeof(PACMAN_LEGEND[0])),
                                level_marker, &ctx)) {
        fprintf(stderr, "pacman: failed to load MAZE (row width or marker "
                        "without a legend entry)\n");
        return;
    }
    /* Every marker must appear exactly once: the maze is the level, so a
     * missing or duplicated marker is an authoring error. */
    if (ctx.pac != 1 || ctx.ghosts != GHOST_COUNT || ctx.home != 1 ||
        ctx.scatter != GHOST_COUNT) {
        fprintf(stderr,
                "pacman: expected 1 'P', %d '1'-'4', 1 'H' and %d 'A'-'D' "
                "markers, found %d/%d/%d/%d\n",
                GHOST_COUNT, GHOST_COUNT, ctx.pac, ctx.ghosts, ctx.home,
                ctx.scatter);
        return;
    }
    world->level.pellets_left =
        tge_tilemap_count(&world->map, ROLE_PELLET) +
        tge_tilemap_count(&world->map, ROLE_POWER);
}

static void level_marker(void *userdata, char marker, int x, int y)
{
    /* Spawn/target markers replace the terrain: 'P', '1'-'4' and 'H' become
     * floor (walkable), while 'A'-'D' keep the wall they sit on — a scatter
     * target is a logical corner the ghosts home in on, not a reachable
     * cell. */
    LevelLoadCtx *ctx = userdata;
    PacmanWorld *world = ctx->world;
    PacmanLevel *level = &world->level;
    switch (marker) {
    case 'P':
        level->pac_spawn = tge_vec2i(x, y);
        ctx->pac++;
        tge_tilemap_set(&world->map, x, y, ROLE_FLOOR);
        break;
    case '1':
    case '2':
    case '3':
    case '4':
        level->ghost_spawns[marker - '1'] = tge_vec2i(x, y);
        ctx->ghosts++;
        tge_tilemap_set(&world->map, x, y, ROLE_FLOOR);
        break;
    case 'H':
        level->ghost_home = tge_vec2i(x, y);
        ctx->home++;
        tge_tilemap_set(&world->map, x, y, ROLE_FLOOR);
        break;
    case 'A':
    case 'B':
    case 'C':
    case 'D':
        level->scatter_targets[marker - 'A'] = tge_vec2i(x, y);
        ctx->scatter++;
        tge_tilemap_set(&world->map, x, y, ROLE_WALL);
        break;
    default:
        break;
    }
}

static void world_spawn_positions(PacmanWorld *world)
{
    world->pac.actor.position = world->level.pac_spawn;
    world->pac.actor.sprite = &SPRITE_PACMAN;
    world->pac.actor.fg = TGE_COLOR_YELLOW;
    world->pac.actor.bg = TGE_COLOR_DEFAULT;
    /* Pac-Man idles until the first direction input: classic Pac-Man does not
     * run off by itself at spawn, and it keeps the deterministic ghost paths
     * from immediately running him down while he auto-moves. */
    world->pac.direction = TGE_DIR_NONE;

    for (int i = 0; i < GHOST_COUNT; i++) {
        PacmanGhost *g = &world->ghosts[i];
        g->actor.position = world->level.ghost_spawns[i];
        g->actor.sprite = &SPRITE_GHOST;
        g->color = tge_color_indexed(GHOST_COLOR_INDEX[i]);
        g->actor.fg = g->color;
        g->actor.bg = TGE_COLOR_DEFAULT;
        /* Blinky (0) starts outside, above the door, facing left; the others
         * start inside the house. Pinky (1) starts released, so she heads for
         * the exit from the first step; Inky (2) and Clyde (3) wait for
         * dots_eaten (30 / 60) before being released. */
        g->direction = (i == 0) ? TGE_DIR_LEFT : TGE_DIR_DOWN;
        g->state = GHOST_ACTIVE;
        g->rng = 0x13579BDFu + (uint32_t)i * 0x9E3779B9u;
        g->scatter_index = i;
        g->released = (i <= 1);
        g->just_reversed = false;
    }
}

static TGE_TileSet world_palette(void)
{
    TGE_TileSet pal;
    memset(&pal, 0, sizeof(pal));
    pal.tiles[ROLE_WALL].sprite = NULL; /* drawn as connected walls by renderer_draw_walls */
    pal.tiles[ROLE_WALL].fg = TGE_COLOR_BLUE;
    pal.tiles[ROLE_WALL].bg = TGE_COLOR_DEFAULT;
    pal.tiles[ROLE_DOOR].sprite = NULL; /* drawn as connected walls by renderer_draw_walls */
    pal.tiles[ROLE_DOOR].fg = TGE_COLOR_MAGENTA;
    pal.tiles[ROLE_DOOR].bg = TGE_COLOR_DEFAULT;
    pal.tiles[ROLE_PELLET].sprite = &SPRITE_PELLET;
    pal.tiles[ROLE_PELLET].fg = TGE_COLOR_WHITE;
    pal.tiles[ROLE_PELLET].bg = TGE_COLOR_DEFAULT;
    pal.tiles[ROLE_POWER].sprite = &SPRITE_POWER;
    pal.tiles[ROLE_POWER].fg = TGE_COLOR_WHITE;
    pal.tiles[ROLE_POWER].bg = TGE_COLOR_DEFAULT;
    return pal;
}

static void world_step(PacmanWorld *world)
{
    TGE_Direction direction;
    if (tge_input_buffer_pop(&world->input, &direction)) {
        /* The queued direction is a turn intent: it only takes effect when
         * the adjacent cell is open. Otherwise it is consumed and Pac-Man
         * keeps moving in the current direction. */
        TGE_Vec2i want = step_position(&world->map,
                                       world->pac.actor.position,
                                       direction, true);
        if (!tge_vec2i_eq(want, world->pac.actor.position))
            world->pac.direction = direction;
    }

    TGE_Vec2i pac_from = world->pac.actor.position;
    TGE_Vec2i next = step_position(&world->map, world->pac.actor.position,
                                   world->pac.direction, true);
    if (!tge_vec2i_eq(next, world->pac.actor.position)) {
        world->pac.actor.position = next;
        eat_cell(world);
    }

    if (world->state == PACMAN_WON)
        return;

    TGE_Vec2i ghost_from[GHOST_COUNT];
    for (int i = 0; i < GHOST_COUNT; i++) {
        PacmanGhost *g = &world->ghosts[i];
        ghost_from[i] = g->actor.position;
        TGE_Direction ghost_direction;
        if (g->just_reversed) {
            /* Honor the forced reversal for exactly one step: the ghost backs
             * into the cell it came from (always valid) before normal
             * cornering resumes. Without this bypass, ghost_choose_direction
             * would immediately exclude the reversed direction and the
             * reversal would be lost. */
            ghost_direction = g->direction;
            g->just_reversed = false;
        } else {
            ghost_direction = ghost_choose_direction(world, g);
        }
        g->direction = ghost_direction;
        g->actor.position = step_position(&world->map, g->actor.position,
                                          ghost_direction, false);
        if (g->state == GHOST_EATEN &&
            tge_vec2i_eq(g->actor.position, world->level.ghost_home)) {
            /* The eyes made it home: respawn exactly on arrival. */
            g->state = GHOST_ACTIVE;
            g->direction = TGE_DIR_DOWN;
            g->released = true;
        }
    }

    for (int i = 0; i < GHOST_COUNT; i++) {
        PacmanGhost *g = &world->ghosts[i];
        if (g->state == GHOST_EATEN)
            continue;
        /* Ghosts inside the ghost house are dormant: like the arcade, the
         * house shields Pac-Man, so they cannot hurt him until they leave. */
        if (cell_in_ghost_house(g->actor.position.x, g->actor.position.y))
            continue;
        bool same_cell = tge_vec2i_eq(g->actor.position,
                                      world->pac.actor.position);
        /* Head-on swap: both moved into each other's previous cell. */
        bool crossing = tge_vec2i_eq(g->actor.position, pac_from) &&
                        tge_vec2i_eq(ghost_from[i],
                                     world->pac.actor.position);
        if (same_cell || crossing) {
            if (world->mode == GHOST_FRIGHTENED)
                eat_ghost(world, g);
            else
                world_die(world);
            break;
        }
    }

    if (world->mode == GHOST_FRIGHTENED) {
        world->frightened_timer -= STEP_SECONDS;
        if (world->frightened_timer <= 0.0f) {
            /* Resume the paused Scatter/Chase phase: mode_timer is left
             * unchanged, so the phase continues where it was interrupted.
             * No reversal on frightened end. */
            world->mode = MODE_PHASE_KIND[world->mode_phase];
        }
    } else {
        world->mode_timer -= STEP_SECONDS;
        if (world->mode_timer <= 0.0f) {
            if (world->mode_phase < MODE_PHASE_COUNT - 1) {
                world->mode_phase++;
                world->mode = MODE_PHASE_KIND[world->mode_phase];
                world->mode_timer = MODE_PHASE_SECONDS[world->mode_phase];
                /* Every scatter<->chase transition forces all active ghosts
                 * to reverse direction. */
                force_ghost_reversal(world);
            } else {
                world->mode_timer = MODE_PHASE_SECONDS[world->mode_phase];
            }
        }
    }
}

static void eat_cell(PacmanWorld *world)
{
    TGE_Vec2i p = world->pac.actor.position;
    uint8_t role = tge_tilemap_get(&world->map, p.x, p.y);
    if (role == ROLE_PELLET) {
        tge_tilemap_set(&world->map, p.x, p.y, ROLE_FLOOR);
        world->score += 10;
        world->level.pellets_left--;
        world->dots_eaten++;
        /* House release is keyed off pellets eaten this level (Pinky at 0,
         * Inky at 30, Clyde at 60). Blinky is already out. */
        if (world->dots_eaten >= 30)
            world->ghosts[2].released = true;
        if (world->dots_eaten >= 60)
            world->ghosts[3].released = true;
        if (world->level.pellets_left == 0)
            world->state = PACMAN_WON;
    } else if (role == ROLE_POWER) {
        tge_tilemap_set(&world->map, p.x, p.y, ROLE_FLOOR);
        world->score += 50;
        world->level.pellets_left--;
        if (world->level.pellets_left == 0) {
            world->state = PACMAN_WON;
        } else {
            world->mode = GHOST_FRIGHTENED;
            world->frightened_timer = FRIGHTENED_DURATION;
            /* mode_timer (the Scatter/Chase phase) stays paused; it resumes
             * unchanged when frightened ends. */
            /* Entering frightened reverses every active ghost immediately. */
            force_ghost_reversal(world);
        }
    }
}

static void eat_ghost(PacmanWorld *world, PacmanGhost *g)
{
    world->score += 200;
    /* The ghost turns into "eyes" and walks home; it respawns when it
     * reaches the house (see world_step). */
    g->state = GHOST_EATEN;
}

static void world_die(PacmanWorld *world)
{
    world->lives--;
    if (world->lives <= 0) {
        world->state = PACMAN_OVER;
        return;
    }
    world_spawn_positions(world);
    world->mode = GHOST_SCATTER;
    world->mode_phase = 0;
    world->mode_timer = MODE_PHASE_SECONDS[0];
    world->frightened_timer = 0.0f;
    /* dots_eaten is preserved: the pellets already eaten stay eaten across a
     * death within the same level. */
}

static uint32_t ghost_rng_next(PacmanGhost *g)
{
    /* xorshift32: deterministic pseudo-random wander for frightened ghosts.
     * The seed is set per-ghost at spawn so runs are reproducible. */
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static TGE_Direction ghost_choose_direction(PacmanWorld *world,
                                            PacmanGhost *g)
{
    /* The arcade tie-break order: up, left, down, right. */
    static const TGE_Direction PREFERENCE[4] = {
        TGE_DIR_UP, TGE_DIR_LEFT, TGE_DIR_DOWN, TGE_DIR_RIGHT,
    };

    if (world->mode == GHOST_FRIGHTENED && g->state == GHOST_ACTIVE) {
        /* Frightened ghosts wander: any open direction except the reverse,
         * picked pseudo-randomly instead of chasing a target. */
        TGE_Direction open[4];
        int count = 0;
        for (int i = 0; i < 4; i++) {
            TGE_Direction candidate = PREFERENCE[i];
            if (candidate == tge_direction_opposite(g->direction))
                continue;
            TGE_Vec2i next =
                tge_vec2i_add(g->actor.position, tge_direction_vec(candidate));
            if (ghost_can_enter(world, g, next))
                open[count++] = candidate;
        }
        if (count == 0)
            return g->direction;
        return open[ghost_rng_next(g) % count];
    }

    TGE_Vec2i target = ghost_target(world, g);
    TGE_Direction best = g->direction;
    int best_distance = INT_MAX;
    for (int i = 0; i < 4; i++) {
        TGE_Direction candidate = PREFERENCE[i];
        /* No 180-degree turn during normal cornering, EXCEPT inside the ghost
         * house, where a released ghost must be allowed to reverse so it can
         * head up and out through the door. */
        if (candidate == tge_direction_opposite(g->direction) &&
            !cell_in_ghost_house(g->actor.position.x, g->actor.position.y))
            continue;
        TGE_Vec2i next =
            tge_vec2i_add(g->actor.position, tge_direction_vec(candidate));
        if (ghost_can_enter(world, g, next)) {
            int distance = tge_vec2i_dist2(next, target);
            if (distance < best_distance) {
                best_distance = distance;
                best = candidate;
            }
        }
    }
    return best;
}

static TGE_Vec2i ghost_target(const PacmanWorld *world, const PacmanGhost *g)
{
    if (g->state == GHOST_EATEN)
        return world->level.ghost_home; /* the eyes walk back to the house */

    /* Being inside the ghost house is a spatial condition; once there, the
     * ghost's release state decides its target: an unreleased ghost paces
     * toward the house center, a released one heads for the exit tile above
     * the door (two rows up) so it paths up and out. */
    if (cell_in_ghost_house(g->actor.position.x, g->actor.position.y)) {
        if (!g->released)
            return world->level.ghost_home;
        return tge_vec2i(world->level.ghost_home.x,
                         world->level.ghost_home.y - 2);
    }

    if (!g->released)
        return world->level.ghost_home; /* safety: unreleased never chases */

    if (world->mode == GHOST_SCATTER)
        return world->level.scatter_targets[g->scatter_index];

    /* CHASE: per-ghost personality from the Pac-Man Dossier. */
    TGE_Vec2i pac = world->pac.actor.position;
    TGE_Direction dir = world->pac.direction;
    switch (g->scatter_index) {
    case 0: /* Blinky: directly on Pac-Man */
        return pac;
    case 1: { /* Pinky: four tiles ahead, with the classic UP overflow bug */
        if (dir == TGE_DIR_NONE)
            return pac; /* a stationary Pac-Man: target him directly */
        TGE_Vec2i t = pac;
        for (int i = 0; i < 4; i++)
            t = tge_vec2i_add(t, tge_direction_vec(dir));
        if (dir == TGE_DIR_UP)
            t = tge_vec2i_add(pac, (TGE_Vec2i){ -4, -4 });
        return t;
    }
    case 2: { /* Inky: reflection of Blinky through the tile two ahead */
        if (dir == TGE_DIR_NONE)
            return pac; /* a stationary Pac-Man: target him directly */
        TGE_Vec2i tile2 = pac;
        for (int i = 0; i < 2; i++)
            tile2 = tge_vec2i_add(tile2, tge_direction_vec(dir));
        TGE_Vec2i blinky = world->ghosts[0].actor.position;
        TGE_Vec2i lead = tge_vec2i_sub(tile2, blinky);
        return tge_vec2i_add(tile2, lead); /* 2*tile2 - blinky */
    }
    case 3: { /* Clyde: chase when far, flee to his corner when close */
        TGE_Vec2i rel = tge_vec2i_sub(pac, g->actor.position);
        if (rel.x * rel.x + rel.y * rel.y > 64)
            return pac;
        return world->level.scatter_targets[3];
    }
    }
    return pac;
}

static bool ghost_can_enter(const PacmanWorld *world, const PacmanGhost *g,
                            TGE_Vec2i cell)
{
    if (cell.x < 0 || cell.y < 0 || cell.x >= world->map.width ||
        cell.y >= world->map.height)
        return false;
    uint8_t role = tge_tilemap_get(&world->map, cell.x, cell.y);
    if (role == ROLE_WALL)
        return false; /* walls block everyone */
    if (role == ROLE_DOOR) {
        /* The door blocks Pac-Man (handled in step_position) and keeps
         * unreleased ghosts in the house; eyes and released ghosts cross. */
        if (g->state == GHOST_EATEN)
            return true;
        if (g->state == GHOST_ACTIVE && g->released)
            return true;
        return false;
    }
    return true; /* floor, tunnel, pellet, power */
}

static void force_ghost_reversal(PacmanWorld *world)
{
    for (int i = 0; i < GHOST_COUNT; i++) {
        PacmanGhost *g = &world->ghosts[i];
        if (g->state == GHOST_ACTIVE) {
            g->direction = tge_direction_opposite(g->direction);
            g->just_reversed = true;
        }
    }
}

/* The ghost house block of the classic maze (rows 12-14, cols 10-17: the door
 * cells and the interior). Ghosts inside it are dormant: they still move and
 * leave, but cannot harm Pac-Man until they exit. */
static const TGE_Rect GHOST_HOUSE = { .x = 10, .y = 12, .w = 8, .h = 3 };
static bool cell_in_ghost_house(int x, int y)
{
    return tge_rect_contains(GHOST_HOUSE, x, y);
}

static TGE_Vec2i step_position(const TGE_TileMap *map, TGE_Vec2i p,
                               TGE_Direction direction, bool pacman)
{
    TGE_Vec2i next = tge_vec2i_add(p, tge_direction_vec(direction));

    /* The tunnel wraps horizontally: moving out of a '-' cell at either edge
     * comes back on the opposite edge. Any other out-of-bounds move is a
     * wall. */
    if (next.x < 0 || next.x >= map->width) {
        if (tge_tilemap_get(map, p.x, p.y) == ROLE_TUNNEL) {
            next.x = next.x < 0 ? map->width - 1 : 0;
        } else {
            return p;
        }
    }
    if (next.y < 0 || next.y >= map->height)
        return p;

    uint8_t role = tge_tilemap_get(map, next.x, next.y);
    if (role == ROLE_WALL)
        return p;
    /* The door blocks Pac-Man from re-entering the house, but lets him leave
     * through it (moving up), like the arcade. Ghosts ignore it entirely. */
    if (pacman && role == ROLE_DOOR && direction != TGE_DIR_UP)
        return p;
    return next;
}
