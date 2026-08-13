/* 08_dino - A Chrome T-Rex Runner clone in box-art: a terminal endless runner
 * rebuilt on the visual language of termrex (~/tmp/termrex), the C++ cousin
 * of this file. Where 08_geometry_dash scaffolded a runner with flat rects
 * and a solid block, this rewrite keeps the same Chrome rules and dresses
 * them in termrex's look: a framed arena, box-drawing sprites that each carry
 * a collision mask, a scrolling textured ground and a block-digit scoreboard.
 *
 *   DinoWorld    pure game state and rules: player (float position, gravity,
 *                jump, timed duck), the scrolling ground pattern, a fixed
 *                array of obstacles, deterministic spawns, clouds, score /
 *                hi-score and a MENU/RUNNING/OVER state machine. No canvas,
 *                no drawing. Collision reads the sprites' masks ('1' cells),
 *                exactly like termrex's SpriteInstance::collide.
 *   DinoRenderer everything that touches the screen: the framed arena, the
 *                block-digit HUD, the textured ground, sprites, clouds and
 *                the overlays. The world is only read, never changed.
 *   DinoGame     the scene glue (a TGE_GameContext from tge-extra/game, first
 *                member) that owns one DinoWorld + one DinoRenderer, moves
 *                input into the world, resizes into world_layout and draws
 *                through the renderer.
 *
 * Design decisions, borrowed from the Chrome game, from termrex and from the
 * existing terminal clones (researched before writing this file):
 *   - The visual language is termrex's. The arena is a box-drawing frame; the
 *     ground is a pattern of `__,-.` / `___.__` chunks that scrolls in whole
 *     cells with a sparse subterrain line below; the scoreboard is rendered
 *     in 2x3 block digits; and every sprite is box-art with a parallel
 *     collision mask. The ASCII fallback collapses the sprites to solid
 *     blocks and the scoreboard to plain text (termrex ships a whole second
 *     ASCII art set; this example keeps the fallback minimal on purpose).
 *   - The sprites hold BOTH the art (what the renderer draws) and a mask
 *     (what the world collides against), a pair that now lives in
 *     tge-extra/sprite as TGE_MaskSprite; this file consumes it. Two sprites
 *     collide only where solid cells overlap, so the ducked dino — a 2-row
 *     sprite — slips under a pterodactyl that would hit the standing one. A
 *     one-AABB-per-obstacle test would not do: the masks are the lesson of
 *     this file, as the flat rects were the lesson of 08_geometry_dash.
 *   - Duck is TIMED, not held. Terminals do not send key-release events, so a
 *     hold-to-duck mechanic cannot work; pressing DOWN ducks for DUCK_TIME and
 *     then stands. Airborne DOWN is a fast-fall instead (a gravity burst).
 *   - Holding SPACE/UP bunny-hops, exactly like the browser: the terminal
 *     repeats the key, the parser turns each repeat into a fresh event, and
 *     each event that finds the dino grounded jumps. No debounce, on purpose.
 *   - Pterodactyls unlock after PTERO_UNLOCK_SCROLL cells of distance, like
 *     Chrome's late-game birds. They come at two altitudes: PTERO_MID overlaps
 *     the standing dino and clears the ducked one (duck or time a jump),
 *     PTERO_LOW overlaps both ground poses and must be jumped.
 *   - Obstacles spawn ONE at a time, only after the previous one has cleared
 *     the field (the reference pygame clone's rule): the gap is the time it
 *     takes to cross the screen, so it shrinks naturally as the speed ramps.
 *     Never two obstacles on screen, which keeps the field fair and readable.
 *   - Anti-tunneling is a non-issue by design: only obstacles move
 *     horizontally, far below any frame rate, so a frame can never skip a
 *     collision.
 *   - Night mode flips every NIGHT_CYCLE points (sky goes blue, a moon
 *     appears) and a milestone every MILESTONE points flashes "100!" and
 *     briefly boosts speed, both straight from Chrome. The sky does not snap:
 *     after each flip it fades smoothly between day and night over
 *     NIGHT_FADE_TIME seconds, like the browser's day/night cycle. The
 *     hi-score lives for the app session only (no file IO in the example
 *     family).
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/game.h"
#include "tge-extra/input.h"
#include "tge-extra/sprite.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Playfield interior (cells), same convention as 08_geometry_dash: the
 * canvas is MIN_FW+2 by MIN_FH+2 (the view adds a 1-cell margin ring, which
 * the renderer turns into the arena frame). The bottom two rows are the
 * ground surface line and the subterrain below it. 1 cell = 1 character. */
#define MIN_FW 38
#define MIN_FH 18
#define DINO_X 6             /* dino left edge, fixed in local cells */
#define DINO_W 18            /* run sprite width, in cells */
#define DINO_RUN_H 6
#define DINO_DUCK_H 2
#define DINO_HIT_W 10        /* collision width; art is wider (Chrome-style) */
/* The jump keeps its ~11-cell peak (it must fit under the top frame) but
 * rides longer than the old arc: at the run's early scroll speed a full-width
 * jump would only cover ~15 cells, less than the 18-cell sprite plus a cactus
 * needs, so the lower gravity buys the horizontal clearance the wide art
 * requires. The 10-cell hitbox is the other half of that (Chrome-style). */
#define GRAVITY 30.0f        /* cells/s^2; peak ~11.3 cells, airtime ~1.73s */
#define JUMP_VY -26.0f       /* jump impulse, cells/s (negative = up) */
#define FAST_FALL_TIME 0.35f /* airborne DOWN: gravity burst this long */
#define BASE_SPEED 10.0f     /* scroll speed at the start of a run */
#define MAX_SPEED 16.0f      /* scroll speed cap */
#define ACCEL 0.12f          /* scroll speed gain, cells/s^2 */
#define BOOST_EXTRA 4.0f     /* extra speed during a milestone boost */
#define BOOST_TIME 1.5f
#define FLASH_TIME 1.2f      /* "100!" flash duration */
#define DUCK_TIME 0.6f       /* how long a DOWN press keeps the dino low */
#define SCORE_CELLS 2        /* scroll cells per score point */
#define MILESTONE 100        /* points between speed-boost milestones */
#define NIGHT_CYCLE 100      /* points per day/night period */
#define NIGHT_FADE_TIME 2.0f /* seconds the sky takes to fade after a flip */
#define MAX_OBSTACLES 64     /* fixed array, no malloc */
#define MAX_CLOUDS 4
#define CLOUD_INTERVAL 16.0f /* scroll cells between cloud spawns */
#define RUN_ANIM_HZ 8.0f     /* dino leg animation rate while running */
#define PTERO_UNLOCK_SCROLL 90.0f /* pterodactyls appear after this */
#define GROUP_UNLOCK_SCROLL 45.0f /* the wide cactus cluster, after this */
#define DINO_DEFAULT_SEED 12345u
#define TERRAIN_BUF 512      /* ground pattern ring buffer (no malloc) */

/* HUD layout in playfield cells: HI + hi-score + current score, block digits
 * 2 cells wide and 3 tall (termrex's UNICODE_DIGIT), right-aligned. */
#define HUD_DIGITS 5
#define HUD_DIGIT_W 2
#define HUD_DIGIT_H 3
#define HUD_HI_W 6
#define HUD_MSG_GAP 2
#define HUD_GAP 3
#define HUD_PAD 2
#define HUD_W (HUD_HI_W + HUD_MSG_GAP + HUD_DIGITS * HUD_DIGIT_W + HUD_GAP + \
               HUD_DIGITS * HUD_DIGIT_W + HUD_PAD)

#define COL_FRAME   TGE_COLOR_GREEN
#define COL_DINO    TGE_COLOR_WHITE
#define COL_EYE     TGE_COLOR_YELLOW
#define COL_CACTUS  TGE_COLOR_GREEN
#define COL_PTERO   TGE_COLOR_YELLOW
#define COL_GROUND  TGE_COLOR_GREEN
#define COL_SUB     TGE_COLOR_GREEN
#define COL_CLOUD   TGE_COLOR_WHITE
#define COL_MOON    TGE_COLOR_YELLOW
#define COL_FLASH   TGE_COLOR_YELLOW

/* Sky endpoints for the day/night fade, in 24-bit RGB. Day is black (the
 * terminal's natural dark look), night is the same blue the scene used to
 * snap to (the ANSI blue slot is roughly #0000AA). */
#define SKY_DAY_R 0
#define SKY_DAY_G 0
#define SKY_DAY_B 0
#define SKY_NIGHT_R 0
#define SKY_NIGHT_G 0
#define SKY_NIGHT_B 160
#define COL_HUD_HI  TGE_COLOR_GREEN
#define COL_HUD_CUR TGE_COLOR_WHITE

/* ------------------------------------------------------------ sprites */

/* A sprite is box-art plus its collision mask, both fixed-size grids of cells
 * written out by hand (termrex's SpriteAsset has the same pair: ANSI_ART and
 * COLLISION_MASK). The shared pair is tge-extra/sprite's TGE_MaskSprite: the
 * world reads `mask` ('1' = solid) and the renderer draws `art` (UTF-8
 * box-drawing glyphs) with the mask as the ASCII fallback. eye/mouth stay
 * Dino-specific: they tell the renderer where to paint the eye and the
 * game-over "surprised" mouth; -1 means the sprite has none. */
typedef struct {
    TGE_MaskSprite base;  /* art, mask, w, h */
    int eye_r, eye_c;
    int mouth_r, mouth_c;
} DinoSprite;

static const char *const dino_run_a_art[] = {
    "               __ ",  /* head top */
    "              / _)",  /* head with eye */
    "     _.----._/ /  ",  /* jaw + body */
    "    /         /   ",  /* back */
    " __/ (  | (  |    ",  /* tail + legs */
    "/__.-'|_|--|_|    ",  /* feet */
};
static const char *const dino_run_a_mask[] = {
    "000000000000000110",
    "000000000000001111",
    "000001111111111100",
    "000011111111111110",
    "011111111111110000",
    "111111111111110000",
};
static const DinoSprite dino_run_a = {
    { dino_run_a_art, dino_run_a_mask, 18, 6 }, 1, 16, 2, 12,
};

static const char *const dino_run_b_art[] = {
    "               __ ",
    "              / _)",
    "     _.----._/ /  ",
    "    /         /   ",
    " __/ (  | (  |    ",
    "/__.-'|_|-|_|     ",
};
static const char *const dino_run_b_mask[] = {
    "000000000000000110",
    "000000000000001111",
    "000001111111111100",
    "000011111111111110",
    "011111111111110000",
    "111111111111100000",
};
static const DinoSprite dino_run_b = {
    { dino_run_b_art, dino_run_b_mask, 18, 6 }, 1, 16, 2, 12,
};

static const char *const dino_jump_art[] = {
    "               __ ",
    "              / _)",
    "     _.----._/ /  ",
    "    /         /   ",
    " __/ (  | (  |    ",
    "/__.-'|_|--|_|    ",
};
static const char *const dino_jump_mask[] = {
    "000000000000000110",
    "000000000000001111",
    "000001111111111100",
    "000011111111111110",
    "011111111111110000",
    "111111111111110000",
};
static const DinoSprite dino_jump = {
    { dino_jump_art, dino_jump_mask, 18, 6 }, 1, 16, 2, 12,
};

static const char *const dino_duck_a_art[] = {
    "     _.----._/ /  ",  /* crouched: low body, head forward */
    "    /         /   ",
};
static const char *const dino_duck_a_mask[] = {
    "000001111111111100",
    "000011111111111110",
};
static const DinoSprite dino_duck_a = {
    { dino_duck_a_art, dino_duck_a_mask, 18, 2 }, -1, -1, -1, -1,
};

static const char *const dino_duck_b_art[] = {
    "     _.----._/ /  ",
    "    /         /   ",
};
static const char *const dino_duck_b_mask[] = {
    "000001111111111100",
    "000011111111111110",
};
static const DinoSprite dino_duck_b = {
    { dino_duck_b_art, dino_duck_b_mask, 18, 2 }, -1, -1, -1, -1,
};

static const char *const cactus_small_art[] = {
    "█▛",
    "██",
    "▝▘",
};
static const char *const cactus_small_mask[] = { "11", "11", "11" };
static const DinoSprite cactus_small = {
    { cactus_small_art, cactus_small_mask, 2, 3 }, -1, -1, -1, -1,
};

static const char *const cactus_tall_art[] = {
    "▛▀",
    "██",
    "██",
    "▝▘",
};
static const char *const cactus_tall_mask[] = { "11", "11", "11", "11" };
static const DinoSprite cactus_tall = {
    { cactus_tall_art, cactus_tall_mask, 2, 4 }, -1, -1, -1, -1,
};

static const char *const cactus_group_art[] = {
    "█▛ █▛",
    "██ ██",
    "▝▘ ▝▘",
};
static const char *const cactus_group_mask[] = { "11011", "11011", "11011" };
static const DinoSprite cactus_group = {
    { cactus_group_art, cactus_group_mask, 5, 3 }, -1, -1, -1, -1,
};

/* The pterodactyl flaps between two frames; collision always uses frame A's
 * mask (the wings do not change the hitbox meaningfully). */
static const char *const ptero_a_art[] = {
    "▗▄▄▄▄▖",  /* wings up */
    "▝████▘",
    "  ▝▘  ",
};
static const char *const ptero_a_mask[] = { "111111", "111111", "001100" };
static const DinoSprite ptero_a = {
    { ptero_a_art, ptero_a_mask, 6, 3 }, -1, -1, -1, -1,
};

static const char *const ptero_b_art[] = {
    "▗▄▄▄▄▖",  /* wings down */
    "▐████▌",
    "  ▝▀▘ ",
};
static const char *const ptero_b_mask[] = { "111111", "111111", "001110" };
static const DinoSprite ptero_b = {
    { ptero_b_art, ptero_b_mask, 6, 3 }, -1, -1, -1, -1,
};

/* ------------------------------------------------------------ digits */

/* Block digits, 2 cells wide x 3 tall (termrex UNICODE_DIGIT). */
static const char *const digit_art[10][3] = {
    { "▄▖", "▌▌", "▙▌" },  /* 0 */
    { "▗ ", "▜ ", "▟▖" },  /* 1 */
    { "▄▖", "▄▌", "▙▖" },  /* 2 */
    { "▄▖", "▄▌", "▄▌" },  /* 3 */
    { "▖▖", "▙▌", " ▌" },  /* 4 */
    { "▄▖", "▙▖", "▄▌" },  /* 5 */
    { "▄▖", "▙▖", "▙▌" },  /* 6 */
    { "▄▖", " ▌", " ▌" },  /* 7 */
    { "▄▖", "▙▌", "▙▌" },  /* 8 */
    { "▄▖", "▙▌", "▄▌" },  /* 9 */
};
static const char *const hi_art[3] = {
    "▄ ▄▗▄▖",
    "█▄█ █ ",
    "█ █▗█▖",
};

/* ---------------------------------------------------------------- world */

/* The scrolling ground. The surface is a stream of chunks (termrex's
 * CHUNKS); the subterrain line below is sparse `-`/`_`/`.` (its SUB_SYMBOLS).
 * Cells are appended to a ring buffer as the run scrolls (no realloc); the
 * buffer may hold more than the visible window after a shrink, and
 * draw_terrain presents the most recent `width` cells. */
static const char *const ground_chunks[] = {
    "__,-.",       "___.__",       "__.-.__",   "______________",
    "__.,.__",     "___,~.____",   "_,---._",   "____________",
    "__,-.__",     "__.-._____",   "______",
};
#define GROUND_CHUNK_COUNT \
    ((int)(sizeof(ground_chunks) / sizeof(ground_chunks[0])))

typedef struct {
    char cells[TERRAIN_BUF];
    char subs[TERRAIN_BUF];
    int width;        /* visible window, in cells */
    int head;         /* next append index in the ring */
    int count;        /* valid cells in the ring */
    const char *chunk; /* chunk being drained right now */
    int chunk_i;       /* index into chunk */
    float carry;       /* fractional scroll cells */
} DinoTerrain;

typedef enum { DINO_MENU = 0, DINO_RUNNING, DINO_OVER } DinoState;

typedef enum {
    DINO_OB_SMALL = 0,   /* 2x3 cactus, jump */
    DINO_OB_TALL,        /* 2x4 cactus, jump */
    DINO_OB_GROUP,       /* 5x3 cactus cluster */
    DINO_OB_PTERO_MID,   /* overlaps standing dino: duck or jump */
    DINO_OB_PTERO_LOW,   /* overlaps both ground poses: jump */
} DinoObstacleKind;

typedef struct {
    float x;             /* left edge, playfield local cells */
    DinoObstacleKind kind;
    bool active;
} DinoObstacle;

typedef struct {
    float x, y;          /* anchor, playfield local cells */
    bool active;
} DinoCloud;

typedef struct {
    TGE_View view;       /* logical playfield layout; view.area is the field */
    DinoState state;
    bool paused;
    float dino_x, dino_y; /* dino top-left, local cell units (floats) */
    float vy;             /* vertical velocity, cells/s */
    bool ducking;         /* crouching on the ground */
    float duck_timer;     /* remaining duck time, seconds */
    float fast_fall;      /* remaining gravity-burst time, seconds */
    float run_time;       /* seconds the dino has been running (animation) */
    float speed;          /* current scroll speed, cells/s */
    float scroll;         /* total scroll distance since the run started */
    float cloud_spawn;    /* accumulated scroll between cloud spawns */
    float flash_timer;    /* remaining "100!" flash time */
    float boost_timer;    /* remaining milestone boost time */
    int score;
    int hi_score;         /* best score of the session (never reset) */
    int next_milestone;   /* the milestone the run will announce next */
    int last_milestone;   /* the one announced by the current flash */
    bool night;           /* current day/night phase (score parity) */
    float phase_fade;     /* seconds since the phase last flipped; >= NIGHT_FADE_TIME = settled */
    uint32_t rng;         /* deterministic LCG: spawns, terrain, clouds */
    DinoTerrain terrain;
    DinoObstacle obs[MAX_OBSTACLES];
    DinoCloud clouds[MAX_CLOUDS];
} DinoWorld;

static uint32_t lcg_next(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

/* Rows, measured from the top: the field is view.area rows tall, the last row
 * is the subterrain and the one above it is the ground surface line. */
static int ground_row(const DinoWorld *w)
{
    return w->view.area.h - 2;
}

/* Rest height of the dino: the top-left row that sits the pose (standing or
 * ducking) on the ground line. Single source of the grounding geometry,
 * shared by duck, landing, reset, start and resize. */
static float dino_rest_y(const DinoWorld *w, bool ducking)
{
    int h = ducking ? DINO_DUCK_H : DINO_RUN_H;
    return (float)(ground_row(w) - h + 1);
}

/* The sprite the world collides against for a given player pose. The same
 * picker drives the renderer, so the visible pose always matches the hitbox. */
static const DinoSprite *world_dino_sprite(const DinoWorld *w)
{
    bool b = ((int)(w->run_time * RUN_ANIM_HZ) & 1) != 0;
    if (w->ducking)
        return b ? &dino_duck_a : &dino_duck_b;
    if (w->vy != 0.0f)
        return &dino_jump;
    return b ? &dino_run_a : &dino_run_b;
}

static const DinoSprite *obstacle_sprite(DinoObstacleKind kind)
{
    switch (kind) {
    case DINO_OB_SMALL:
        return &cactus_small;
    case DINO_OB_TALL:
        return &cactus_tall;
    case DINO_OB_GROUP:
        return &cactus_group;
    case DINO_OB_PTERO_MID:
    case DINO_OB_PTERO_LOW:
    default:
        return &ptero_a;
    }
}

/* Top row of an obstacle, so its feet sit on the ground line (bottom row g):
 * the cacti stand on the ground, the pterodactyls fly. MID hovers so its 3
 * rows overlap the standing dino's head but clear the 2-row duck; LOW is
 * lower and overlaps both ground poses. */
static int obstacle_top(const DinoWorld *w, const DinoObstacle *o)
{
    int g = ground_row(w);
    switch (o->kind) {
    case DINO_OB_SMALL:
        return g - 2;
    case DINO_OB_TALL:
        return g - 3;
    case DINO_OB_GROUP:
        return g - 2;
    case DINO_OB_PTERO_MID:
        return g - 5;
    case DINO_OB_PTERO_LOW:
    default:
        return g - 3;
    }
}

/* Mask collision via tge-extra/sprite (the termrex SpriteInstance::collide
 * algorithm): a hit needs a solid cell in both masks where they overlap, so a
 * ducked dino slips under a pterodactyl that would hit the standing one. The
 * hitbox is narrower than the art, Chrome-style: a full-width box on the
 * 18-cell dino could not be crossed by a single jump at the run's speed. */
static bool world_player_hits(const DinoWorld *w, const DinoObstacle *o)
{
    const DinoSprite *d = world_dino_sprite(w);
    const DinoSprite *s = obstacle_sprite(o->kind);
    int hit_left = (DINO_W - DINO_HIT_W) / 2; /* center the Chrome-style box */
    return tge_sprite_collide(&d->base, w->dino_x + hit_left, (int)w->dino_y,
                              DINO_HIT_W, &s->base, (int)o->x,
                              obstacle_top(w, o));
}

/* One terrain cell scrolls into view: surface char from the current chunk,
 * subterrain char from the sparse symbol stream. */
static void terrain_step(DinoTerrain *t, uint32_t *rng)
{
    t->cells[t->head] = t->chunk[t->chunk_i];
    t->chunk_i++;
    if (t->chunk[t->chunk_i] == '\0') {
        t->chunk = ground_chunks[lcg_next(rng) % GROUND_CHUNK_COUNT];
        t->chunk_i = 0;
    }
    if (lcg_next(rng) % 100u < 10u) {
        static const char sub[] = "-_.";
        t->subs[t->head] = sub[lcg_next(rng) % 3u];
    } else {
        t->subs[t->head] = ' ';
    }
    t->head = (t->head + 1) % TERRAIN_BUF;
}

/* Fill the ground from scratch with `width` visible cells. Deterministic: it
 * consumes a fixed number of LCG draws, so every run from the same seed shows
 * the same ground as the obstacles. */
static void terrain_fill(DinoTerrain *t, int width, uint32_t *rng)
{
    t->width = width;
    t->head = 0;
    t->count = 0;
    t->carry = 0.0f;
    t->chunk = ground_chunks[lcg_next(rng) % GROUND_CHUNK_COUNT];
    t->chunk_i = 0;
    for (int i = 0; i < width; i++)
        terrain_step(t, rng);
    t->count = width;
}

/* Widen the visible window after a resize, topping up the ring as needed. */
static void terrain_set_width(DinoTerrain *t, int width, uint32_t *rng)
{
    t->width = width;
    while (t->count < width && t->count < TERRAIN_BUF) {
        terrain_step(t, rng);
        t->count++;
    }
}

static float effective_speed(const DinoWorld *w)
{
    return w->speed + (w->boost_timer > 0.0f ? BOOST_EXTRA : 0.0f);
}

static void world_jump(DinoWorld *w)
{
    if (w->vy == 0.0f) { /* grounded (standing or ducking) */
        w->ducking = false;
        w->fast_fall = 0.0f;
        w->vy = JUMP_VY;
    }
}

/* Start the run: a complete MENU->RUNNING transition, so every future code
 * path that opens a game gets the full run invariants in one call. The dino
 * stands resting at the new run's ground, physics and animation reset, and
 * every run begins in a settled day phase. */
static void world_start(DinoWorld *w)
{
    w->state = DINO_RUNNING;
    w->scroll = 0.0f;
    w->speed = BASE_SPEED;
    w->score = 0;
    w->next_milestone = MILESTONE;
    w->last_milestone = 0;
    w->flash_timer = 0.0f;
    w->boost_timer = 0.0f;
    w->ducking = false;
    w->fast_fall = 0.0f;
    w->duck_timer = 0.0f;
    w->dino_y = dino_rest_y(w, false);
    w->vy = 0.0f;
    w->run_time = 0.0f;
    w->night = false;
    w->phase_fade = NIGHT_FADE_TIME; /* settled day sky until the first flip */
}

/* DOWN: duck when grounded (timed, terminals send no key release), else a
 * short gravity burst for the classic "drop faster" fall. Ducking snaps the
 * dino down to the 2-row pose so the lowered hitbox sits on the ground. */
static void world_duck(DinoWorld *w)
{
    if (w->vy == 0.0f) {
        w->ducking = true;
        w->duck_timer = DUCK_TIME;
        w->dino_y = dino_rest_y(w, true);
    } else {
        w->fast_fall = FAST_FALL_TIME;
    }
}

static void world_spawn(DinoWorld *w)
{
    DinoObstacle *slot = NULL;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!w->obs[i].active) {
            slot = &w->obs[i];
            break;
        }
    }
    if (!slot)
        return; /* defensive: a free slot always exists under the one-obstacle rule */
    slot->active = true;
    slot->x = (float)w->view.area.w;
    uint32_t r = lcg_next(&w->rng) % 10u;
    DinoObstacleKind kind;
    if (r < 4u) {
        kind = DINO_OB_SMALL;
    } else if (r < 7u) {
        kind = DINO_OB_TALL;
    } else if (r < 8u) {
        kind = (w->scroll < GROUP_UNLOCK_SCROLL) ? DINO_OB_SMALL
                                                 : DINO_OB_GROUP;
    } else if (w->scroll < PTERO_UNLOCK_SCROLL) {
        kind = DINO_OB_SMALL; /* no birds early on */
    } else if (r < 9u) {
        kind = DINO_OB_PTERO_LOW;
    } else {
        kind = DINO_OB_PTERO_MID;
    }
    slot->kind = kind;
}

static void world_update(DinoWorld *w, float dt)
{
    if (!w->view.valid || w->paused)
        return;

    if (w->state == DINO_MENU) {
        /* Living backdrop only: the seeded clouds drift until the run. */
        for (int i = 0; i < MAX_CLOUDS; i++) {
            if (!w->clouds[i].active)
                continue;
            w->clouds[i].x -= 1.5f * dt;
            if (w->clouds[i].x < -4.0f)
                w->clouds[i].active = false;
        }
        return;
    }
    if (w->state != DINO_RUNNING)
        return;

    float eff = effective_speed(w);
    w->speed += ACCEL * dt;
    if (w->speed > MAX_SPEED)
        w->speed = MAX_SPEED;
    if (w->boost_timer > 0.0f)
        w->boost_timer -= dt;
    if (w->flash_timer > 0.0f)
        w->flash_timer -= dt;
    w->scroll += eff * dt;
    w->score = (int)(w->scroll / SCORE_CELLS);
    if (w->score >= w->next_milestone) {
        w->last_milestone = w->next_milestone;
        w->next_milestone += MILESTONE;
        w->flash_timer = FLASH_TIME;
        w->boost_timer = BOOST_TIME;
    }
    w->run_time += dt;

    /* Day/night: note the phase (score parity) and how long it has been in
     * force, so the renderer can fade the sky instead of snapping it. */
    bool night_now = ((w->score / NIGHT_CYCLE) & 1) != 0;
    if (night_now != w->night) {
        w->night = night_now;
        w->phase_fade = 0.0f;
    }
    if (w->phase_fade < NIGHT_FADE_TIME)
        w->phase_fade += dt;

    /* The ground scrolls in whole cells: the terrain step timer shares the
     * run's scroll speed, so the pattern moves left at the same rate as the
     * obstacles (termrex does the same). */
    w->terrain.carry += eff * dt;
    while (w->terrain.carry >= 1.0f) {
        terrain_step(&w->terrain, &w->rng);
        w->terrain.carry -= 1.0f;
    }

    /* Player physics: gravity, jump arc, duck timer, landing. The standing
     * pose moves down three cells while ducking; standing while grounded
     * after the timer expires lands the dino back where it belongs. */
    if (w->ducking) {
        w->duck_timer -= dt;
        if (w->duck_timer <= 0.0f)
            w->ducking = false;
    }
    float gravity = (w->fast_fall > 0.0f) ? GRAVITY * 1.6f : GRAVITY;
    w->vy += gravity * dt;
    if (w->fast_fall > 0.0f)
        w->fast_fall -= dt;
    w->dino_y += w->vy * dt;
    {
        float land = dino_rest_y(w, w->ducking);
        if (w->dino_y >= land) {
            w->dino_y = land;
            w->vy = 0.0f;
            w->fast_fall = 0.0f;
        }
    }

    /* Obstacles scroll left; a fresh one spawns only once the previous has
     * fully cleared the field (the reference pygame clone's rule: one obstacle
     * on screen at a time). The gap is the screen-crossing time, so it shrinks
     * naturally as the speed ramps. */
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!w->obs[i].active)
            continue;
        w->obs[i].x -= eff * dt;
        if (w->obs[i].x + 6.0f < 0.0f) /* widest obstacle is 6 cells */
            w->obs[i].active = false;
    }
    {
        bool busy = false;
        for (int i = 0; i < MAX_OBSTACLES; i++)
            if (w->obs[i].active) {
                busy = true;
                break;
            }
        if (!busy)
            world_spawn(w);
    }

    /* Clouds drift at a third of the ground speed and respawn to the right. */
    w->cloud_spawn += eff * dt;
    for (int i = 0; i < MAX_CLOUDS; i++) {
        if (!w->clouds[i].active)
            continue;
        w->clouds[i].x -= eff * dt / 3.0f;
        if (w->clouds[i].x < -4.0f)
            w->clouds[i].active = false;
    }
    while (w->cloud_spawn >= CLOUD_INTERVAL) {
        int slot = -1;
        for (int i = 0; i < MAX_CLOUDS; i++) {
            if (!w->clouds[i].active) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            w->cloud_spawn = 0.0f;
            break;
        }
        w->clouds[slot].active = true;
        w->clouds[slot].x = (float)(w->view.area.w + 2);
        w->clouds[slot].y = 2.0f + (float)(lcg_next(&w->rng) % 4u);
        w->cloud_spawn -= CLOUD_INTERVAL;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (w->obs[i].active && world_player_hits(w, &w->obs[i])) {
            w->state = DINO_OVER;
            if (w->score > w->hi_score)
                w->hi_score = w->score;
            return;
        }
    }
}

static void world_reset(DinoWorld *w)
{
    w->state = DINO_MENU;
    w->paused = false;
    w->rng = DINO_DEFAULT_SEED;
    w->dino_x = DINO_X;
    w->dino_y = dino_rest_y(w, false);
    w->vy = 0.0f;
    w->ducking = false;
    w->duck_timer = 0.0f;
    w->fast_fall = 0.0f;
    w->run_time = 0.0f;
    w->speed = BASE_SPEED;
    w->scroll = 0.0f;
    w->cloud_spawn = 0.0f;
    w->flash_timer = 0.0f;
    w->boost_timer = 0.0f;
    w->score = 0;
    w->next_milestone = MILESTONE;
    w->last_milestone = 0;
    w->night = false;
    w->phase_fade = NIGHT_FADE_TIME; /* settled day sky until the first flip */
    terrain_fill(&w->terrain, w->view.area.w, &w->rng);
    for (int i = 0; i < MAX_OBSTACLES; i++)
        w->obs[i].active = false;
    for (int i = 0; i < MAX_CLOUDS; i++)
        w->clouds[i].active = false;
    /* Seed the menu backdrop so the idle screen is not empty. */
    w->clouds[0].active = true;
    w->clouds[0].x = 10.0f;
    w->clouds[0].y = 3.0f;
    w->clouds[1].active = true;
    w->clouds[1].x = (float)(w->view.area.w - 8);
    w->clouds[1].y = 5.0f;
}

/* Keep the player on the ground and drop anything that ended up off the
 * playfield after a resize. The clamp respects the current pose, so a ducked
 * dino is never left floating above the new ground line. */
static void world_resize_fix(DinoWorld *w)
{
    int top = (int)dino_rest_y(w, w->ducking);
    if (w->dino_y > top)
        w->dino_y = (float)top;
    if (w->state == DINO_MENU) {
        w->dino_y = dino_rest_y(w, false);
        w->vy = 0.0f;
    }
    terrain_set_width(&w->terrain, w->view.area.w, &w->rng);
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (w->obs[i].active &&
            (w->obs[i].x > (float)w->view.area.w || w->obs[i].x < -6.0f))
            w->obs[i].active = false;
    }
    for (int i = 0; i < MAX_CLOUDS; i++) {
        if (w->clouds[i].active &&
            (w->clouds[i].x > (float)(w->view.area.w + 4) ||
             w->clouds[i].y >= (float)(w->view.area.h - 6)))
            w->clouds[i].active = false;
    }
}

/* Recompute the playfield for a new surface size (the renderer feeds the
 * canvas size; 1 cell = 1 character). First valid layout spawns a fresh
 * game, later resizes keep the current game and too-small stays inactive. */
static void world_layout(DinoWorld *w, int gw, int gh)
{
    TGE_ViewUpdate upd = tge_view_update(&w->view, gw, gh);
    switch (upd) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(w);
        break;
    case TGE_VIEW_RESIZED:
        world_resize_fix(w);
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static void world_init(DinoWorld *w)
{
    tge_view_init(&w->view, MIN_FW, MIN_FH);
}

/* ------------------------------------------------------------- renderer */

/* Rendering is stateless apart from the portable glyphs and the layout
 * cache: the playfield geometry lives in DinoWorld.view, and layout_w/h
 * remember the surface size the world layout was computed for (the world
 * itself never remembers how it was presented). No TGE_GridLayout on
 * purpose, same reasoning as 08_geometry_dash: this game has no grid. */
typedef struct {
    uint32_t cloud;    /* cloud glyph (U+2601) or '~' */
    uint32_t moon;     /* crescent glyph (U+263E) or ')' */
    int layout_w;      /* surface width the world layout was computed for */
    int layout_h;      /* surface height the world layout was computed for */
} DinoRenderer;

/* Draw one row of UTF-8 box glyphs, one cell per glyph, skipping spaces (the
 * background already shows through). */
static void draw_utf8_row(TGE_Canvas *canvas, int x, int y, const char *s,
                          TGE_Color fg, TGE_Color bg)
{
    int cx = x;
    const char *p = s;
    while (*p) {
        uint32_t cp;
        int n = tge_utf8_decode(p, 8, &cp);
        if (n <= 0)
            break;
        if (cp != ' ' && cp != 0)
            tge_set_cell(canvas, cx, y, cp, fg, bg);
        cx++;
        p += n;
    }
}

/* The dino, with its eye (and, when it crashes, its surprised eye + open
 * mouth) painted on top of tge-extra/sprite's body draw, like termrex's
 * changeEye/changeMouth. */
static void draw_dino(TGE_Canvas *canvas, int x, int y, const DinoSprite *s,
                      TGE_Color sky, bool unicode, bool over)
{
    tge_sprite_draw(canvas, x, y, &s->base, COL_DINO, sky);
    if (s->eye_r < 0)
        return;
    uint32_t eye = unicode ? (over ? 0x25CFu : 0x25A0u) : (over ? 'X' : '#');
    tge_set_cell(canvas, x + s->eye_c, y + s->eye_r, eye,
                 over ? TGE_COLOR_RED : COL_EYE, sky);
    if (over && unicode && s->mouth_r >= 0)
        draw_utf8_row(canvas, x + s->mouth_c, y + s->mouth_r, "▀▀",
                      TGE_COLOR_RED, sky);
}

static int digit_at(int value, int place)
{
    for (int i = 0; i < place; i++)
        value /= 10;
    return value % 10;
}

/* Right-aligned block-digit scoreboard: a dim "HI" + hi-score, a gap, then
 * the current score in bright — Chrome's layout in termrex's 2x3 digits. The
 * ASCII fallback is the plain one-line HUD. */
static void hud_draw(TGE_Canvas *canvas, const DinoWorld *w, bool unicode,
                     int gw, TGE_Color sky)
{
    TGE_Vec2i o = tge_view_translate(&w->view, tge_vec2i(0, 0));
    /* The 33-cell block HUD only fits when the playfield is wide enough to
     * leave the 18-cell dino to itself; otherwise fall back to the compact
     * one-line HUD (which also serves non-unicode terminals). Its 14 chars
     * hug the right edge so the dino's head stays clear even at MIN_FW. */
    if (!unicode || gw - HUD_W <= w->dino_x + DINO_W) {
        char buf[32];
        snprintf(buf, sizeof buf, "HI %05d %05d", w->hi_score, w->score);
        tge_draw_text(canvas, o.x + gw - (int)strlen(buf), o.y, buf,
                      COL_HUD_CUR, sky);
        return;
    }
    int hi = w->hi_score > 99999 ? 99999 : w->hi_score;
    int cur = w->score > 99999 ? 99999 : w->score;
    int base = gw - HUD_W;
    for (int r = 0; r < HUD_DIGIT_H; r++)
        draw_utf8_row(canvas, o.x + base, o.y + r, hi_art[r], COL_HUD_HI, sky);
    for (int i = 0; i < HUD_DIGITS; i++) {
        int x = base + HUD_HI_W + HUD_MSG_GAP + i * HUD_DIGIT_W;
        for (int r = 0; r < HUD_DIGIT_H; r++)
            draw_utf8_row(canvas, o.x + x, o.y + r,
                          digit_art[digit_at(hi, HUD_DIGITS - 1 - i)][r],
                          COL_HUD_HI, sky);
    }
    int x = base + HUD_HI_W + HUD_MSG_GAP + HUD_DIGITS * HUD_DIGIT_W + HUD_GAP;
    for (int i = 0; i < HUD_DIGITS; i++) {
        for (int r = 0; r < HUD_DIGIT_H; r++)
            draw_utf8_row(canvas, o.x + x + i * HUD_DIGIT_W, o.y + r,
                          digit_art[digit_at(cur, HUD_DIGITS - 1 - i)][r],
                          COL_HUD_CUR, sky);
    }
}

/* The ground surface line and the sparse subterrain below it, drawn from the
 * terrain ring buffer. */
static void draw_terrain(TGE_Canvas *canvas, const DinoWorld *w, TGE_Color sky)
{
    const DinoTerrain *t = &w->terrain;
    int n = t->count < t->width ? t->count : t->width;
    int start = (t->head - n + TERRAIN_BUF) % TERRAIN_BUF;
    int g = ground_row(w);
    TGE_Vec2i o = tge_view_translate(&w->view, tge_vec2i(0, g));
    for (int i = 0; i < n; i++) {
        int idx = (start + i) % TERRAIN_BUF;
        if (t->cells[idx] != ' ')
            tge_set_cell(canvas, o.x + i, o.y, (uint32_t)t->cells[idx],
                         COL_GROUND, sky);
        if (t->subs[idx] != ' ')
            tge_set_cell(canvas, o.x + i, o.y + 1, (uint32_t)t->subs[idx],
                         COL_SUB, sky);
    }
}

/* Sky color with a smooth day/night fade, like the browser game. phase_fade
 * holds the seconds since the phase last flipped; blend runs 0->1 day->night
 * after a flip into night and reverses after a flip back to day. The phase is
 * the world's `night` (set from score parity in world_update), so the
 * renderer never re-derives it. */
static TGE_Color day_night_sky(const DinoWorld *w)
{
    float t = w->phase_fade / NIGHT_FADE_TIME;
    if (t > 1.0f)
        t = 1.0f;
    if (!w->night)
        t = 1.0f - t;
    uint8_t r = (uint8_t)(SKY_DAY_R + (int)((SKY_NIGHT_R - SKY_DAY_R) * t));
    uint8_t g = (uint8_t)(SKY_DAY_G + (int)((SKY_NIGHT_G - SKY_DAY_G) * t));
    uint8_t b = (uint8_t)(SKY_DAY_B + (int)((SKY_NIGHT_B - SKY_DAY_B) * t));
    return tge_color_rgb(r, g, b);
}

static void renderer_draw(DinoRenderer *r, TGE_Canvas *canvas,
                          const DinoWorld *w)
{
    int cw = tge_canvas_width(canvas);
    int ch = tge_canvas_height(canvas);
    TGE_Color sky = day_night_sky(w);
    bool unicode = tge_unicode_supported();

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, sky);
    tge_draw_frame(canvas, 0, 0, cw, ch, COL_FRAME, sky);

    if (!w->view.valid) {
        tge_draw_centered_text(canvas, ch / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    if (w->night) {
        TGE_Vec2i m = tge_view_translate(&w->view, tge_vec2i(2, 2));
        tge_set_cell(canvas, m.x, m.y, r->moon, COL_MOON, sky);
    }
    for (int i = 0; i < MAX_CLOUDS; i++) {
        if (!w->clouds[i].active)
            continue;
        TGE_Vec2i p = tge_view_translate(
            &w->view, tge_vec2i((int)w->clouds[i].x, (int)w->clouds[i].y));
        tge_set_cell(canvas, p.x, p.y, r->cloud, COL_CLOUD, sky);
    }

    draw_terrain(canvas, w, sky);

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        const DinoObstacle *o = &w->obs[i];
        if (!o->active)
            continue;
        const DinoSprite *s = obstacle_sprite(o->kind);
        bool is_ptero = o->kind == DINO_OB_PTERO_MID ||
                        o->kind == DINO_OB_PTERO_LOW;
        if (is_ptero)
            s = ((int)(w->run_time * 12.0f) & 1) ? &ptero_b : &ptero_a;
        TGE_Vec2i p = tge_view_translate(
            &w->view, tge_vec2i((int)o->x, obstacle_top(w, o)));
        tge_sprite_draw(canvas, p.x, p.y, &s->base,
                        is_ptero ? COL_PTERO : COL_CACTUS, sky);
    }

    {
        const DinoSprite *s = world_dino_sprite(w);
        TGE_Vec2i dp = tge_view_translate(
            &w->view, tge_vec2i((int)w->dino_x, (int)w->dino_y));
        draw_dino(canvas, dp.x, dp.y, s, sky, unicode, w->state == DINO_OVER);
    }

    if (w->flash_timer > 0.0f) {
        char buf[16];
        snprintf(buf, sizeof buf, "%d!", w->last_milestone);
        TGE_Vec2i dp = tge_view_translate(
            &w->view, tge_vec2i((int)w->dino_x, (int)w->dino_y));
        tge_draw_text(canvas, dp.x + DINO_W + 1, dp.y - 1, buf, COL_FLASH,
                      sky);
    }

    hud_draw(canvas, w, unicode, w->view.area.w, sky);

    if (w->state == DINO_OVER) {
        const char *again = " [SPACE]/[ENTER] restart  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    } else if (w->paused) {
        const char *again = " [P] resume ";
        tge_draw_modal(canvas, " PAUSED ", again, TGE_COLOR_YELLOW);
    } else if (w->state == DINO_MENU) {
        tge_draw_centered_text(canvas, ch / 2, " [SPACE] run to start ",
                               TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    }
}

/* ---------------------------------------------------------------- scenes */

/* The game as a whole: the scene glue (TGE_GameContext) + world + renderer,
 * wired together by the game callbacks. */
typedef struct {
    TGE_GameContext ctx; /* first member: scene->userdata == &game->ctx */
    DinoWorld world;
    DinoRenderer renderer;
} DinoGame;

/* Only the title scene still talks directly to the app. Game scenes receive
 * ctx->app through TGE_GameContext. */
static TGE_App *g_app = NULL;

/* Jump input: UP/W (mapped by tge-extra) plus SPACE (KEYDOWN or TEXT). The
 * terminal repeats held keys, so holding SPACE bunny-hops like Chrome. */
static bool dino_is_jump(const TGE_Event *ev)
{
    if (tge_input_direction(ev) == TGE_DIR_UP)
        return true;
    if (ev->type == TGE_EVENT_TEXT && ev->data.text.codepoint == ' ')
        return true;
    if (ev->type == TGE_EVENT_KEYDOWN && ev->data.key.keycode == TGE_KEY_SPACE)
        return true;
    return false;
}

/* Duck input: DOWN arrow or S (both mapped by tge-extra). */
static bool dino_is_duck(const TGE_Event *ev)
{
    return tge_input_direction(ev) == TGE_DIR_DOWN;
}

static void game_update(TGE_GameContext *ctx, float dt)
{
    DinoGame *g = (DinoGame *)tge_game_instance(ctx);
    world_update(&g->world, dt);
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    DinoGame *g = (DinoGame *)tge_game_instance(ctx);
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    if (g->renderer.layout_w != w || g->renderer.layout_h != h) {
        g->renderer.layout_w = w;
        g->renderer.layout_h = h;
        world_layout(&g->world, w, h);
    }
    renderer_draw(&g->renderer, canvas, &g->world);
}

static void game_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    DinoGame *g = (DinoGame *)tge_game_instance(ctx);

    if (ev->type == TGE_EVENT_RESIZE) {
        g->renderer.layout_w = ev->data.resize.w;
        g->renderer.layout_h = ev->data.resize.h;
        world_layout(&g->world, ev->data.resize.w, ev->data.resize.h);
        if (g->world.state != DINO_OVER)
            g->world.paused = true;
        return;
    }
    if (tge_input_pause(ev)) {
        if (g->world.state != DINO_OVER)
            g->world.paused = !g->world.paused;
        return;
    }
    if (g->world.paused && !tge_input_cancel(ev))
        return;
    if (dino_is_jump(ev)) {
        if (g->world.state == DINO_MENU)
            world_start(&g->world);
        else if (g->world.state == DINO_RUNNING)
            world_jump(&g->world);
        else if (g->world.state == DINO_OVER && g->world.view.valid)
            world_reset(&g->world);
        return;
    }
    if (dino_is_duck(ev)) {
        if (g->world.state == DINO_RUNNING)
            world_duck(&g->world);
        return;
    }
    if (tge_input_quit(ev)) {
        if (g->world.state == DINO_OVER)
            TGE_Quit(ctx->app);
        return;
    }
    bool confirm = tge_input_confirm(ev);
    if (!confirm && ev->type == TGE_EVENT_TEXT &&
        ev->data.text.codepoint == ' ')
        confirm = true;
    if (confirm && g->world.state == DINO_OVER && g->world.view.valid) {
        world_reset(&g->world);
        return;
    }
    if (tge_input_cancel(ev)) {
        TGE_PopScene(ctx->app);
    }
}

/* The game's interface, handed to tge_game_create(). No destroy: the
 * game owns no memory beyond the scene itself. */
static const TGE_GameCallbacks dino_callbacks = {
    game_update, game_draw, game_event, NULL,
};

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    TGE_Color bg = TGE_COLOR_BLACK;
    const char *title = " DINO RUNNER ";
    const char *subtitle = " a T-Rex clone: jump, duck, run! ";
    const char *controls = " SPACE/UP jump  DOWN duck  P: pause ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, bg);
    tge_draw_frame(canvas, 0, 0, w, h, COL_FRAME, bg);
    tge_draw_centered_text(canvas, h / 2 - 6, title, TGE_COLOR_GREEN, bg);
    tge_draw_centered_text(canvas, h / 2 - 4, subtitle, TGE_COLOR_CYAN, bg);
    tge_draw_centered_text(canvas, h / 2 - 1, controls, TGE_COLOR_WHITE, bg);
    tge_draw_centered_text(canvas, h / 2 + 1, start, TGE_COLOR_YELLOW, bg);

    /* The dino and a strip of box-art cacti stand on the frame floor, like
     * the game it is cloning. */
    int ground = h - 2;
    tge_sprite_draw(canvas, w / 2 - 19, ground - DINO_RUN_H + 1,
                    &dino_run_a.base, COL_DINO, bg);
    for (int i = 0; i < 3; i++)
        tge_sprite_draw(canvas, w / 2 + 1 + i * 5, ground - 2,
                        &cactus_group.base, COL_CACTUS, bg);
}

static void title_event(TGE_Scene *scene, TGE_Event *ev)
{
    (void)scene;
    if (tge_input_cancel(ev) || tge_input_quit(ev)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(ev)) {
        DinoGame *g = (DinoGame *)tge_game_create(g_app, sizeof(DinoGame),
                                                  &dino_callbacks);
        if (!g)
            return;
        g->renderer.cloud = tge_unicode_supported() ? 0x2601u : '~';
        g->renderer.moon = tge_unicode_supported() ? 0x263Eu : ')';
        world_init(&g->world);
    }
}

static void init_app(TGE_App *app)
{
    g_app = app;
    TGE_Scene *title = NULL;
    tge_scene_create(&title, 0, NULL, title_draw, title_event, NULL);
    title->opaque = false;
    TGE_PushScene(app, title);
}

int main(void)
{
    /* Requested size is the minimum/fallback: the core starts with the real
     * terminal size when it can query it (TIOCGWINSZ). 1 cell = 1 character,
     * no square pixels, so the canvas is MIN_FW+2 by MIN_FH+2. */
    TGE_App *app = TGE_Create(MIN_FW + 2, MIN_FH + 2, "TGE Dino Runner");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}
