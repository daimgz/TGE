#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/game.h"
#include "tge-extra/input.h"
#include "tge-extra/playfield.h"
#include "tge-extra/tilemap.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Sokoban — Fase 3b #09. Valida TGE_TileMap como fuente de verdad de un nivel
 * cuyas celdas CAMBIAN de rol (empujar cajas), y TGE_Playfield como composición
 * sin asumir un game loop con fixedstep: el movimiento es discreto por tecla
 * (event-driven), a diferencia de Pac-Man. El undo es específico del juego y
 * no vive en tge-extra.
 *
 * El TileMap conserva el estado persistente de la celda (FLOOR/GOAL + caja); el
 * jugador es un overlay aparte (no un rol del mapa) y solo modifica
 * temporalmente esa representación, lo que mantiene el undo trivial. */

#define SOKO_W 12
#define SOKO_H 10
#define LEVEL_COUNT 3
#define UNDO_MAX 256

typedef enum {
    SOKO_WALL = 0,
    SOKO_FLOOR,
    SOKO_GOAL,
    SOKO_BOX,
    SOKO_BOX_ON_GOAL
} SokoRole;

typedef enum { SOKO_PLAYING, SOKO_WON, SOKO_ALL_CLEAR } SokoState;

/* One undoable step: the player's previous cell, plus (when a box was pushed)
 * the two cells whose roles changed, stored as their PRE-move roles so undo is
 * a straight restoration. */
typedef struct {
    TGE_Vec2i player_from;
    bool      pushed;
    TGE_Vec2i box_from;      /* the cell the box left */
    uint8_t   box_from_role; /* SOKO_BOX or SOKO_BOX_ON_GOAL */
    TGE_Vec2i box_to;        /* the cell the box entered */
    uint8_t   box_to_role;   /* SOKO_FLOOR or SOKO_GOAL (terrain underneath) */
} Move;

typedef struct {
    TGE_TileMap map;
    TGE_TileSet tiles;
    TGE_Vec2i   player;      /* overlay; the map never stores the player */
    int         level;
    int         moves;
    SokoState   state;
    int         undo_count;
    Move        undo[UNDO_MAX];
} SokoWorld;

typedef struct {
    TGE_GameContext ctx;
    TGE_Playfield   pf;
    SokoWorld       world;
} SokoGame;

static TGE_App *g_app = NULL;

typedef struct {
    int       w;
    int       h;
    const char *const *rows;
} SokoLevelDef;

static const TGE_TileLegend SOKO_LEGEND[] = {
    { '#', SOKO_WALL },
    { ' ', SOKO_FLOOR },
    { '.', SOKO_GOAL },
    { '$', SOKO_BOX },
    { '*', SOKO_BOX_ON_GOAL },
    /* '@' and '+' are markers, not tiles: handled in level_marker() */
};

static const char *const LEVEL0[SOKO_H] = {
    "############",
    "#          #",
    "#@$ .      #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "############",
};
static const char *const LEVEL1[SOKO_H] = {
    "############",
    "#          #",
    "#@$ .  $ . #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "############",
};
static const char *const LEVEL2[SOKO_H] = {
    "############",
    "#          #",
    "#@$ .      #",
    "#          #",
    "#      $ . #",
    "#          #",
    "#          #",
    "#          #",
    "#          #",
    "############",
};
static const SokoLevelDef LEVELS[LEVEL_COUNT] = {
    { SOKO_W, SOKO_H, LEVEL0 },
    { SOKO_W, SOKO_H, LEVEL1 },
    { SOKO_W, SOKO_H, LEVEL2 },
};

static const TGE_Sprite SPRITE_EMPTY  = TGE_SPRITE(1, 1, " ", NULL);
static const TGE_Sprite SPRITE_WALL   = TGE_SPRITE(1, 1, "#", NULL);
static const TGE_Sprite SPRITE_GOAL   = TGE_SPRITE(1, 1, ".", NULL);
static const TGE_Sprite SPRITE_BOX    = TGE_SPRITE(1, 1, "$", NULL);
static const TGE_Sprite SPRITE_BOX_G  = TGE_SPRITE(1, 1, "*", NULL);
static const TGE_Sprite SPRITE_PLAYER = TGE_SPRITE(1, 1, "@", NULL);

static const TGE_GridTheme SOKOBAN_THEME = {
    .empty          = &SPRITE_EMPTY,
    .default_sprite = &SPRITE_EMPTY,
    .border         = &SPRITE_EMPTY,
};

typedef struct {
    SokoWorld *world;
    int        player;
    TGE_Vec2i  player_pos;
} LevelLoadCtx;

static void game_event  (TGE_GameContext *ctx, TGE_Event *event);
static void game_draw   (TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_update (TGE_GameContext *ctx, float delta_time);
static void game_destroy(TGE_GameContext *ctx);
static void game_world_resize(void *userdata, int grid_w, int grid_h);

static const TGE_GameCallbacks sokoban_callbacks = {
    .update  = game_update,
    .draw    = game_draw,
    .event   = game_event,
    .destroy = game_destroy,
};

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *event);

static void world_init  (SokoWorld *world);
static void world_reset (SokoWorld *world);
static void world_resize(SokoWorld *world, TGE_View *view, int grid_w, int grid_h);
static bool sokoban_load_def(SokoWorld *world, const SokoLevelDef *def);
static void level_marker(void *userdata, char marker, int x, int y);
static void world_move   (SokoWorld *world, TGE_Direction dir);
static void world_undo   (SokoWorld *world);
static bool world_handle_input(SokoWorld *world, TGE_Event *event);
static void advance_level(SokoWorld *world);

static void renderer_draw(TGE_Playfield *pf, const SokoWorld *world,
                          TGE_Canvas *canvas);

static TGE_TileSet sokoban_palette(void)
{
    TGE_TileSet pal;
    memset(&pal, 0, sizeof(pal));
    pal.tiles[SOKO_WALL].sprite          = &SPRITE_WALL;
    pal.tiles[SOKO_WALL].fg              = TGE_COLOR_BLUE;
    pal.tiles[SOKO_WALL].bg              = TGE_COLOR_DEFAULT;
    pal.tiles[SOKO_FLOOR].sprite         = NULL; /* empty floor: skipped by draw */
    pal.tiles[SOKO_GOAL].sprite          = &SPRITE_GOAL;
    pal.tiles[SOKO_GOAL].fg              = TGE_COLOR_YELLOW;
    pal.tiles[SOKO_GOAL].bg              = TGE_COLOR_DEFAULT;
    pal.tiles[SOKO_BOX].sprite           = &SPRITE_BOX;
    pal.tiles[SOKO_BOX].fg               = TGE_COLOR_WHITE;
    pal.tiles[SOKO_BOX].bg               = TGE_COLOR_DEFAULT;
    pal.tiles[SOKO_BOX_ON_GOAL].sprite   = &SPRITE_BOX_G;
    pal.tiles[SOKO_BOX_ON_GOAL].fg       = TGE_COLOR_GREEN;
    pal.tiles[SOKO_BOX_ON_GOAL].bg       = TGE_COLOR_DEFAULT;
    return pal;
}

#if defined(TGE_SOKOBAN_TEST)
static int sokoban_main(void)
#else
int main(void)
#endif
{
    TGE_App *app = TGE_Create(SOKO_W + 2, SOKO_H + 2, "TGE Sokoban");
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
    int cw = tge_canvas_width(canvas);
    int ch = tge_canvas_height(canvas);

    tge_draw_frame(canvas, 0, 0, cw, ch, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2 - 2, " SOKOBAN ",
                           TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2, " push boxes onto goals ",
                           TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2 + 2,
                           " Arrows/WASD move   U undo ",
                           TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2 + 4,
                           " [ENTER] start   [ESC]/[Q] quit ",
                           TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
}

static void title_event(TGE_Scene *scene, TGE_Event *event)
{
    (void)scene;
    if (tge_input_cancel(event) || tge_input_quit(event)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(event)) {
        SokoGame *game = (SokoGame *)tge_game_create(
            g_app, sizeof(SokoGame), &sokoban_callbacks);
        if (!game)
            return;
        tge_playfield_init(&game->pf, &SOKOBAN_THEME, TGE_GRID_SCALE_1X1,
                           SOKO_W, SOKO_H);
        tge_grid_set_origin(&game->pf.grid_view.grid, 0, 1);
        world_init(&game->world);
    }
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    SokoGame *game = (SokoGame *)tge_game_instance(ctx);
    tge_playfield_sync(&game->pf, tge_canvas_width(canvas),
                       tge_canvas_height(canvas), game_world_resize, game);
    renderer_draw(&game->pf, &game->world, canvas);
}

static void game_update(TGE_GameContext *ctx, float delta_time)
{
    (void)ctx;
    (void)delta_time;
}

static void game_event(TGE_GameContext *ctx, TGE_Event *event)
{
    SokoGame *game = (SokoGame *)tge_game_instance(ctx);
    SokoWorld *world = &game->world;

    if (event->type == TGE_EVENT_RESIZE) {
        tge_playfield_sync(&game->pf, event->data.resize.w,
                           event->data.resize.h, game_world_resize, game);
        return;
    }
    if (tge_input_quit(event)) {
        TGE_Quit(ctx->app);
        return;
    }
    if (tge_input_cancel(event)) {
        TGE_PopScene(ctx->app);
        return;
    }
    if (world->state == SOKO_WON) {
        if (tge_input_confirm(event))
            advance_level(world);
        return;
    }
    if (world->state == SOKO_ALL_CLEAR) {
        if (tge_input_confirm(event)) {
            world->level = 0;
            world_reset(world);
        }
        return;
    }
    world_handle_input(world, event);
}

static void game_destroy(TGE_GameContext *ctx)
{
    (void)ctx;
}

static void game_world_resize(void *userdata, int grid_w, int grid_h)
{
    SokoGame *game = (SokoGame *)userdata;
    world_resize(&game->world, &game->pf.view, grid_w, grid_h);
}

static void renderer_draw(TGE_Playfield *pf, const SokoWorld *world,
                          TGE_Canvas *canvas)
{
    tge_playfield_attach(pf, canvas);
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT,
               " MOVES: %d  LEVEL: %d/%d ", world->moves, world->level + 1,
               LEVEL_COUNT);
    tge_playfield_draw_border(pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);

    if (!pf->view.valid) {
        tge_draw_centered_text(canvas, tge_canvas_height(canvas) / 2,
                               " too small ", TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    tge_tilemap_draw(&world->map, &pf->grid_view.grid, pf->view.area.x,
                     pf->view.area.y, &world->tiles);
    tge_grid_view_put_local(&pf->grid_view, &pf->view, world->player,
                            &SPRITE_PLAYER, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);

    if (world->state == SOKO_WON) {
        tge_draw_modal(canvas, " LEVEL CLEAR ",
                       " [ENTER] next   [ESC] menu ", TGE_COLOR_GREEN);
    } else if (world->state == SOKO_ALL_CLEAR) {
        tge_draw_modal(canvas, " YOU WIN! ",
                       " [ENTER] restart   [ESC] menu ", TGE_COLOR_GREEN);
    }
}

static void world_init(SokoWorld *world)
{
    memset(world, 0, sizeof(*world));
    world->tiles = sokoban_palette();
    world->level = 0;
}

static void world_reset(SokoWorld *world)
{
    if (!sokoban_load_def(world, &LEVELS[world->level]))
        world->state = SOKO_ALL_CLEAR; /* only with a malformed shipped level */
}

static void world_resize(SokoWorld *world, TGE_View *view, int grid_w, int grid_h)
{
    TGE_ViewUpdate u = tge_view_update(view, grid_w, grid_h);
    switch (u) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(world);
        break;
    default:
        break;
    }
}

static void level_marker(void *userdata, char marker, int x, int y)
{
    /* The player is a marker, not a tile: the cell it stands on keeps its
     * underlying terrain (FLOOR, or GOAL when it starts on a goal). The map
     * only ever holds walls/floor/goal/boxes. */
    LevelLoadCtx *ctx = userdata;
    switch (marker) {
    case '@':
        ctx->player++;
        ctx->player_pos = tge_vec2i(x, y);
        tge_tilemap_set(&ctx->world->map, x, y, SOKO_FLOOR);
        break;
    case '+':
        ctx->player++;
        ctx->player_pos = tge_vec2i(x, y);
        tge_tilemap_set(&ctx->world->map, x, y, SOKO_GOAL);
        break;
    default:
        break;
    }
}

static bool sokoban_load_def(SokoWorld *world, const SokoLevelDef *def)
{
    tge_tilemap_init(&world->map, def->w, def->h);
    LevelLoadCtx ctx = { .world = world };
    if (!tge_tilemap_load_ascii(&world->map, def->rows, def->w, def->h,
                                SOKO_LEGEND,
                                (int)(sizeof(SOKO_LEGEND) /
                                      sizeof(SOKO_LEGEND[0])),
                                level_marker, &ctx)) {
        fprintf(stderr, "sokoban: failed to load level (row width or unknown "
                        "marker)\n");
        return false;
    }
    int boxes = tge_tilemap_count(&world->map, SOKO_BOX) +
                tge_tilemap_count(&world->map, SOKO_BOX_ON_GOAL);
    int goals = tge_tilemap_count(&world->map, SOKO_GOAL) +
                tge_tilemap_count(&world->map, SOKO_BOX_ON_GOAL);
    if (ctx.player != 1 || boxes != goals) {
        fprintf(stderr,
                "sokoban: expected 1 player and equal boxes/goals, found "
                "%d/%d\n", ctx.player, boxes);
        return false;
    }
    world->player = ctx.player_pos;
    world->moves = 0;
    world->undo_count = 0;
    world->state = SOKO_PLAYING;
    return true;
}

static void world_move(SokoWorld *world, TGE_Direction dir)
{
    if (world->state != SOKO_PLAYING || dir == TGE_DIR_NONE)
        return;

    TGE_Vec2i target = tge_vec2i_add(world->player, tge_direction_vec(dir));
    uint8_t t = tge_tilemap_get(&world->map, target.x, target.y);
    if (t == SOKO_WALL)
        return;

    Move m;
    m.pushed = false;
    m.player_from = world->player;

    if (t == SOKO_BOX || t == SOKO_BOX_ON_GOAL) {
        TGE_Vec2i beyond = tge_vec2i_add(target, tge_direction_vec(dir));
        uint8_t b = tge_tilemap_get(&world->map, beyond.x, beyond.y);
        if (b == SOKO_WALL || b == SOKO_BOX || b == SOKO_BOX_ON_GOAL)
            return; /* cannot push two boxes or into a wall/box */
        m.pushed = true;
        m.box_from = target;
        m.box_from_role = t;
        m.box_to = beyond;
        m.box_to_role = b; /* FLOOR or GOAL underneath the destination */
        tge_tilemap_set(&world->map, beyond.x, beyond.y,
                        (b == SOKO_GOAL) ? SOKO_BOX_ON_GOAL : SOKO_BOX);
        /* the cell the box left reverts to its underlying terrain */
        tge_tilemap_set(&world->map, target.x, target.y,
                        (t == SOKO_BOX_ON_GOAL) ? SOKO_GOAL : SOKO_FLOOR);
    }
    /* moving into FLOOR/GOAL changes nothing in the map: the player is an
     * overlay drawn on top. */

    world->player = target;
    if (world->undo_count < UNDO_MAX)
        world->undo[world->undo_count++] = m;
    world->moves++;

    if (tge_tilemap_count(&world->map, SOKO_BOX) == 0)
        world->state = SOKO_WON;
}

static void world_undo(SokoWorld *world)
{
    if (world->state != SOKO_PLAYING || world->undo_count <= 0)
        return;
    Move m = world->undo[--world->undo_count];
    if (m.pushed) {
        tge_tilemap_set(&world->map, m.box_to.x, m.box_to.y, m.box_to_role);
        tge_tilemap_set(&world->map, m.box_from.x, m.box_from.y,
                        m.box_from_role);
    }
    world->player = m.player_from;
    world->moves--;
}

static void advance_level(SokoWorld *world)
{
    if (world->level + 1 < LEVEL_COUNT) {
        world->level++;
        sokoban_load_def(world, &LEVELS[world->level]);
    } else {
        world->state = SOKO_ALL_CLEAR;
    }
}

static bool soko_undo_key(const TGE_Event *ev)
{
    if (ev->type == TGE_EVENT_KEYDOWN && ev->data.key.keycode == 'u')
        return true;
    if (ev->type == TGE_EVENT_TEXT) {
        uint32_t c = ev->data.text.codepoint;
        if (c == 'u' || c == 'U' || c == 'z' || c == 'Z')
            return true;
    }
    return false;
}

static bool world_handle_input(SokoWorld *world, TGE_Event *event)
{
    TGE_Direction dir = tge_input_direction(event);
    if (dir != TGE_DIR_NONE) {
        world_move(world, dir);
        return true;
    }
    if (soko_undo_key(event)) {
        world_undo(world);
        return true;
    }
    return false;
}
