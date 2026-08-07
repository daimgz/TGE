/* 08_dino - A Chrome T-Rex Runner clone: a terminal endless runner.
 *
 * The fourth game on the TGE_Game architecture, and the runner that
 * 08_geometry_dash wanted to be. Geometry Dash was a cube on a flat floor
 * with two block shapes; the Chrome dino adds what a runner is really about
 * once you stop scaffolding physics: a two-tier obstacle set (ground cacti
 * you jump, flying pterodactyls you jump OR duck), a living background
 * (clouds, day/night cycle) and Chrome's milestone speed boosts.
 *
 *   DinoWorld    pure game state and rules: player (float position, gravity,
 *                jump, duck), a fixed array of obstacles that scroll left at
 *                an ever-growing speed, deterministic spawns (same tiny LCG
 *                as 08_geometry_dash), clouds, score / hi-score and a
 *                MENU/RUNNING/OVER state machine. No canvas, no drawing.
 *   DinoRenderer everything that touches the screen: canvas clear, right-
 *                aligned HUD (HI + score, like Chrome), ground band, clouds,
 *                moon, cacti, pterodactyls, the animated dino and the
 *                overlays. The world is only read, never changed.
 *   DinoGame     the scene glue (a TGE_GameContext from tge-extra/game, first
 *                member) that owns one DinoWorld + one DinoRenderer, moves
 *                input into the world, resizes into world_layout and draws
 *                through the renderer.
 *
 * Design decisions, borrowed from the Chrome game and from existing terminal
 * clones (researched before writing this file):
 *   - Duck is TIMED, not held. Terminals do not send key-release events, so
 *     a hold-to-duck mechanic cannot work; pressing DOWN ducks for DUCK_TIME
 *     and then stands. termrex documents the same constraint. Airborne DOWN
 *     is a fast-fall instead (a gravity burst), Chrome's other use of the
 *     key.
 *   - Holding SPACE/UP bunny-hops, exactly like the browser: the terminal
 *     repeats the key, the parser turns each repeat into a fresh event, and
 *     each event that finds the dino grounded jumps. No debounce, on
 *     purpose.
 *   - Pterodactyls unlock after PTERO_UNLOCK_SCROLL cells of distance, like
 *     Chrome's late-game birds. Early runs are cacti only.
 *   - The pterodactyl has two altitudes, so the obstacle set forces a
 *     decision: PTERO_MID overlaps the standing dino and clears the ducked
 *     one (duck or time a jump), PTERO_LOW overlaps both ground poses and
 *     must be jumped.
 *   - The dino's hitbox is its full sprite (the opposite of Chrome, whose
 *     hitboxes are famously narrower than the art). One AABB test per
 *     obstacle keeps the collision the lesson of this file, like
 *     08_geometry_dash before it.
 *   - Anti-tunneling is a non-issue by design: only obstacles move
 *     horizontally, far below any frame rate, so a frame can never skip a
 *     collision.
 *   - Night mode flips every NIGHT_CYCLE points (sky goes blue, a moon
 *     appears) and a milestone every MILESTONE points flashes "100!" and
 *     briefly boosts speed, both straight from Chrome. The hi-score lives
 *     for the app session only (no file IO in the example family).
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/game.h"
#include "tge-extra/input.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdint.h>
#include <stdio.h>

/* Playfield interior (cells), same convention as 08_geometry_dash: the
 * canvas is MIN_FW+2 by MIN_FH+2 (the view adds a 1-cell margin; the
 * renderer keeps the top row for the HUD). 1 cell = 1 character. */
#define MIN_FW 26
#define MIN_FH 13
#define DINO_X 6             /* dino left edge, fixed in local cells */
#define DINO_W 4
#define DINO_RUN_H 3
#define DINO_DUCK_H 2
#define GRAVITY 40.0f        /* cells/s^2; peak jump ~4.1 cells (clears 3) */
#define JUMP_VY -18.0f       /* jump impulse, cells/s (negative = up) */
#define FAST_FALL_TIME 0.35f /* airborne DOWN: gravity burst this long */
#define BASE_SPEED 7.0f      /* scroll speed at the start of a run */
#define MAX_SPEED 16.0f      /* scroll speed cap */
#define ACCEL 0.12f          /* scroll speed gain, cells/s^2 */
#define BOOST_EXTRA 4.0f     /* extra speed during a milestone boost */
#define BOOST_TIME 1.5f
#define FLASH_TIME 1.2f      /* "100!" flash duration */
#define DUCK_TIME 0.6f       /* how long a DOWN press keeps the dino low */
#define GAP_MIN 9            /* minimum obstacle gap, in scroll cells */
#define GAP_JITTER 7         /* random extra gap, drawn from the LCG */
#define SCORE_CELLS 2        /* scroll cells per score point */
#define MILESTONE 100        /* points between speed-boost milestones */
#define NIGHT_CYCLE 100      /* points per day/night period */
#define MAX_OBSTACLES 64     /* fixed array, no malloc */
#define MAX_CLOUDS 4
#define CLOUD_INTERVAL 16.0f /* scroll cells between cloud spawns */
#define RUN_ANIM_HZ 8.0f     /* dino leg animation rate while running */
#define PTERO_UNLOCK_SCROLL 90.0f /* pterodactyls appear after this */
#define DINO_DEFAULT_SEED 12345u

/* ---------------------------------------------------------------- world */

typedef enum { DINO_MENU = 0, DINO_RUNNING, DINO_OVER } DinoState;

typedef enum {
    DINO_OB_SMALL = 0,   /* 1x2 cactus, jump */
    DINO_OB_TALL,        /* 1x3 cactus, jump higher */
    DINO_OB_GROUP,       /* 3x2 cactus cluster */
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
    float next_spawn;     /* scroll value at which the next obstacle appears */
    float cloud_spawn;    /* accumulated scroll between cloud spawns */
    float flash_timer;    /* remaining "100!" flash time */
    float boost_timer;    /* remaining milestone boost time */
    int score;
    int hi_score;         /* best score of the session (never reset) */
    int next_milestone;   /* the milestone the run will announce next */
    int last_milestone;   /* the one announced by the current flash */
    uint32_t rng;         /* deterministic spawn LCG */
    DinoObstacle obs[MAX_OBSTACLES];
    DinoCloud clouds[MAX_CLOUDS];
} DinoWorld;

static uint32_t lcg_next(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static bool rectf_intersects(float ax, float ay, float aw, float ah,
                             float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

/* Rows, measured from the top: the field is view.area rows tall. */
static int ground_row(const DinoWorld *w)
{
    return w->view.area.h - 1;
}

static float effective_speed(const DinoWorld *w)
{
    return w->speed + (w->boost_timer > 0.0f ? BOOST_EXTRA : 0.0f);
}

/* Hitbox of the player: the full sprite, duck lowers it to two rows. */
static void player_rect(const DinoWorld *w, float *x, float *y,
                        float *rw, float *rh)
{
    *x = w->dino_x;
    *y = w->dino_y;
    *rw = DINO_W;
    *rh = w->ducking ? DINO_DUCK_H : DINO_RUN_H;
}

/* Hitbox of an obstacle, all standing on the ground or flying level. */
static void obstacle_rect(const DinoWorld *w, const DinoObstacle *o,
                          float *x, float *y, float *rw, float *rh)
{
    int g = ground_row(w);
    *x = o->x;
    switch (o->kind) {
    case DINO_OB_SMALL:
        *y = (float)(g - 1); *rw = 1.0f; *rh = 2.0f; break;
    case DINO_OB_TALL:
        *y = (float)(g - 2); *rw = 1.0f; *rh = 3.0f; break;
    case DINO_OB_GROUP:
        *y = (float)(g - 1); *rw = 3.0f; *rh = 2.0f; break;
    case DINO_OB_PTERO_MID:
        *y = (float)(g - 3); *rw = 3.0f; *rh = 2.0f; break;
    case DINO_OB_PTERO_LOW:
        *y = (float)(g - 2); *rw = 3.0f; *rh = 2.0f; break;
    default:
        *y = (float)(g - 1); *rw = 1.0f; *rh = 2.0f; break;
    }
}

static bool world_player_hits(const DinoWorld *w, const DinoObstacle *o)
{
    float px, py, pw, ph, ox, oy, ow, oh;
    player_rect(w, &px, &py, &pw, &ph);
    obstacle_rect(w, o, &ox, &oy, &ow, &oh);
    return rectf_intersects(px, py, pw, ph, ox, oy, ow, oh);
}

static void world_jump(DinoWorld *w)
{
    if (w->vy == 0.0f) { /* grounded (standing or ducking) */
        w->ducking = false;
        w->fast_fall = 0.0f;
        w->vy = JUMP_VY;
    }
}

/* Start the run: the first tap both launches the scroll and jumps. */
static void world_start(DinoWorld *w)
{
    w->state = DINO_RUNNING;
    w->scroll = 0.0f;
    w->speed = BASE_SPEED;
    w->next_spawn = 16.0f;
    w->score = 0;
    w->next_milestone = MILESTONE;
    w->last_milestone = 0;
    w->flash_timer = 0.0f;
    w->boost_timer = 0.0f;
    w->ducking = false;
    w->fast_fall = 0.0f;
    world_jump(w);
}

/* DOWN: duck when grounded (timed, terminals send no key release), else a
 * short gravity burst for the classic "drop faster" fall. Ducking drops the
 * dino one cell so the lowered hitbox sits on the ground, not in the air. */
static void world_duck(DinoWorld *w)
{
    if (w->vy == 0.0f) {
        w->ducking = true;
        w->duck_timer = DUCK_TIME;
        w->dino_y = (float)(w->view.area.h - DINO_RUN_H + 1);
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
        return; /* all 64 busy (only possible at extreme speeds): skip one */
    slot->active = true;
    slot->x = (float)w->view.area.w;
    uint32_t r = lcg_next(&w->rng) % 10u;
    DinoObstacleKind kind;
    if (r < 4u) {
        kind = DINO_OB_SMALL;
    } else if (r < 7u) {
        kind = DINO_OB_TALL;
    } else if (r < 8u) {
        kind = DINO_OB_GROUP;
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

    /* Player physics: gravity, jump arc, duck timer, landing. The rest
     * position moves down one cell while ducking; standing while grounded
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
        float land = (float)(w->view.area.h - DINO_RUN_H) +
                     (w->ducking ? 1.0f : 0.0f);
        if (w->dino_y >= land) {
            w->dino_y = land;
            w->vy = 0.0f;
            w->fast_fall = 0.0f;
        }
    }

    /* Obstacles scroll left; the spawner works in scroll cells like the
     * others, so as the speed ramps the time between obstacles shrinks. */
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (!w->obs[i].active)
            continue;
        w->obs[i].x -= eff * dt;
        if (w->obs[i].x + 3.0f < 0.0f)
            w->obs[i].active = false;
    }
    while (w->scroll >= w->next_spawn) {
        world_spawn(w);
        w->next_spawn += GAP_MIN + (float)(lcg_next(&w->rng) % GAP_JITTER);
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
    w->dino_y = (float)(w->view.area.h - DINO_RUN_H);
    w->vy = 0.0f;
    w->ducking = false;
    w->duck_timer = 0.0f;
    w->fast_fall = 0.0f;
    w->run_time = 0.0f;
    w->speed = BASE_SPEED;
    w->scroll = 0.0f;
    w->next_spawn = 16.0f;
    w->cloud_spawn = 0.0f;
    w->flash_timer = 0.0f;
    w->boost_timer = 0.0f;
    w->score = 0;
    w->next_milestone = MILESTONE;
    w->last_milestone = 0;
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
 * playfield after a resize. */
static void world_resize_fix(DinoWorld *w)
{
    int top = w->view.area.h - DINO_RUN_H;
    if (w->dino_y > top)
        w->dino_y = (float)top;
    if (w->state == DINO_MENU) {
        w->dino_y = (float)top;
        w->vy = 0.0f;
    }
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (w->obs[i].active &&
            (w->obs[i].x > (float)w->view.area.w || w->obs[i].x < -3.0f))
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
    uint32_t solid;    /* full block when the terminal supports Unicode */
    uint32_t cloud;    /* cloud glyph (U+2601) or '~' */
    uint32_t moon;     /* crescent glyph (U+263E) or ')' */
    int layout_w;      /* surface width the world layout was computed for */
    int layout_h;      /* surface height the world layout was computed for */
} DinoRenderer;

/* Sprites are small char grids; 'X' stands for the solid glyph so the
 * shapes survive terminals without Unicode. ' ' cells are transparent. */
static const char *const dino_run_a[] = { "X XX", "XXXX", "X  X" };
static const char *const dino_run_b[] = { "X XX", "XXXX", "XX X" };
static const char *const dino_jump[] = { "X XX", "XXXX", " XX " };
static const char *const dino_duck[] = { "XXXX", "X XX" };
static const char *const ptero_a[] = { "X X", "XXX" };
static const char *const ptero_b[] = { "XXX", "X X" };

static void draw_sprite(TGE_Canvas *canvas, int x, int y,
                        const char *const rows[], int w, int h,
                        uint32_t solid, TGE_Color fg, TGE_Color bg)
{
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            if (rows[r][c] != ' ')
                tge_set_cell(canvas, x + c, y + r, solid, fg, bg);
        }
    }
}

static void renderer_draw(DinoRenderer *r, TGE_Canvas *canvas,
                          const DinoWorld *w)
{
    int cw = tge_canvas_width(canvas);
    int ch = tge_canvas_height(canvas);
    bool night = ((w->score / NIGHT_CYCLE) & 1) != 0;

    tge_clear(canvas, ' ', TGE_COLOR_BLACK,
              night ? TGE_COLOR_BLUE : TGE_COLOR_BLACK);

    /* Chrome HUD: hi-score and score, top right. */
    tge_printf(canvas, cw - 18, 0, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT,
               " HI %05d  %05d ", w->hi_score, w->score);

    if (!w->view.valid) {
        tge_draw_centered_text(canvas, ch / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    int ground = ground_row(w);

    if (night) {
        TGE_Vec2i m = tge_view_translate(&w->view, tge_vec2i(2, 2));
        tge_set_cell(canvas, m.x, m.y, r->moon, TGE_COLOR_YELLOW,
                     TGE_COLOR_DEFAULT);
    }

    for (int i = 0; i < MAX_CLOUDS; i++) {
        if (!w->clouds[i].active)
            continue;
        TGE_Vec2i p = tge_view_translate(
            &w->view, tge_vec2i((int)w->clouds[i].x, (int)w->clouds[i].y));
        tge_set_cell(canvas, p.x, p.y, r->cloud, TGE_COLOR_WHITE,
                     TGE_COLOR_DEFAULT);
    }

    /* Ground band: the last playfield row, green like 08_geometry_dash. */
    {
        TGE_Vec2i p = tge_view_translate(&w->view, tge_vec2i(0, ground));
        tge_fill_rect(canvas, p.x, p.y, w->view.area.w, 1, ' ',
                      TGE_COLOR_BLACK, TGE_COLOR_GREEN);
    }

    const char *const *ptero =
        ((int)(w->run_time * 12.0f) & 1) ? ptero_b : ptero_a;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        const DinoObstacle *o = &w->obs[i];
        if (!o->active)
            continue;
        switch (o->kind) {
        case DINO_OB_SMALL: {
            TGE_Vec2i p = tge_view_translate(
                &w->view, tge_vec2i((int)o->x, ground - 1));
            tge_fill_rect(canvas, p.x, p.y, 1, 2, r->solid, TGE_COLOR_GREEN,
                          TGE_COLOR_DEFAULT);
            break;
        }
        case DINO_OB_TALL: {
            TGE_Vec2i p = tge_view_translate(
                &w->view, tge_vec2i((int)o->x, ground - 2));
            tge_fill_rect(canvas, p.x, p.y, 1, 3, r->solid, TGE_COLOR_GREEN,
                          TGE_COLOR_DEFAULT);
            break;
        }
        case DINO_OB_GROUP: {
            TGE_Vec2i p = tge_view_translate(
                &w->view, tge_vec2i((int)o->x, ground - 1));
            tge_fill_rect(canvas, p.x, p.y, 3, 2, r->solid, TGE_COLOR_GREEN,
                          TGE_COLOR_DEFAULT);
            break;
        }
        case DINO_OB_PTERO_MID:
        case DINO_OB_PTERO_LOW: {
            int top = (o->kind == DINO_OB_PTERO_LOW) ? ground - 2 : ground - 3;
            TGE_Vec2i p = tge_view_translate(&w->view, tge_vec2i((int)o->x, top));
            draw_sprite(canvas, p.x, p.y, ptero, 3, 2, r->solid,
                        TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
            break;
        }
        default:
            break;
        }
    }

    {
        TGE_Vec2i dp = tge_view_translate(
            &w->view, tge_vec2i((int)w->dino_x, (int)w->dino_y));
        const char *const *dino = dino_run_a;
        int dh = DINO_RUN_H;
        if (w->ducking) {
            dino = dino_duck;
            dh = DINO_DUCK_H;
        } else if (w->vy != 0.0f) {
            dino = dino_jump;
        } else if ((int)(w->run_time * RUN_ANIM_HZ) & 1) {
            dino = dino_run_b;
        }
        draw_sprite(canvas, dp.x, dp.y, dino, DINO_W, dh, r->solid,
                    TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    }

    if (w->flash_timer > 0.0f) {
        char buf[16];
        snprintf(buf, sizeof buf, "%d!", w->last_milestone);
        TGE_Vec2i dp = tge_view_translate(
            &w->view, tge_vec2i((int)w->dino_x, (int)w->dino_y));
        tge_draw_text(canvas, dp.x + DINO_W + 2, dp.y - 1, buf,
                      TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    }

    if (w->state == DINO_OVER) {
        const char *again =
            " [SPACE]/[ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    } else if (w->paused) {
        const char *again = " [P] resume ";
        tge_draw_modal(canvas, " PAUSED ", again, TGE_COLOR_YELLOW);
    } else if (w->state == DINO_MENU) {
        tge_draw_centered_text(canvas, ch / 2 + 2, " [SPACE] jump to start ",
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
    const char *title = " DINO RUNNER ";
    const char *subtitle =
        " a Chrome T-Rex clone: jump, duck, endless runner ";
    const char *controls = " SPACE/UP jump  DOWN duck  P: pause ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 - 4, title, TGE_COLOR_GREEN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 - 2, subtitle, TGE_COLOR_CYAN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 1, controls, TGE_COLOR_WHITE,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 3, start, TGE_COLOR_YELLOW,
                           TGE_COLOR_DEFAULT);

    /* A strip of cacti on the ground, like the game it is cloning. */
    uint32_t solid = tge_unicode_supported() ? 0x2588u : '#';
    for (int i = 0; i < 6; i++) {
        tge_set_cell(canvas, w / 2 - 7 + i * 2, h / 2 + 5, solid,
                     TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
        tge_set_cell(canvas, w / 2 - 7 + i * 2, h / 2 + 6, solid,
                     TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
    }
    tge_set_cell(canvas, w / 2 + 5, h / 2 + 4, solid, TGE_COLOR_GREEN,
                 TGE_COLOR_DEFAULT);
    tge_set_cell(canvas, w / 2 + 5, h / 2 + 5, solid, TGE_COLOR_GREEN,
                 TGE_COLOR_DEFAULT);
    tge_set_cell(canvas, w / 2 + 5, h / 2 + 6, solid, TGE_COLOR_GREEN,
                 TGE_COLOR_DEFAULT);
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
        g->renderer.solid = tge_unicode_supported() ? 0x2588u : '#';
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
