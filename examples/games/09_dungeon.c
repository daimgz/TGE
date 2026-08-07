/* 09_dungeon - A turn-based roguelike-lite: a composition experiment, not a
 * dungeon crawler.
 *
 * The screen is split into four regions that share no rendering logic, so
 * this second composition consumer (after Tetris) shows which patterns a
 * game is actually forced to repeat:
 *
 *   MAPA      one TGE_Grid of 1x1 cells: terrain + entities in a single
 *             grid, no layers.
 *   STATS     plain text read straight from the world; dumb on purpose, no
 *             bars, icons or colors.
 *   LOG       text with its own scrollback: DungeonLog is state owned by the
 *             game, fed by the world, drawn by the renderer.
 *   CONTROLES a fixed text line; deliberately no border or title.
 *
 * The experiment: each region is described as an outer rect (x, y, w, h) and
 * its content area is recomputed as x+1 / y+1 / w-2 / h-2 wherever it is
 * used; every bordered region repeats the frame + title draw. That repetition
 * (captured once in the local `region_draw` helper) is the evidence under
 * study. No tge-extra API is added in this example.
 *
 * Gameplay is minimal on purpose: exterior walls plus a few random interior
 * walls, a few enemies, gold and an exit. Arrows/WASD move one cell per
 * turn, [ENTER] waits a turn, enemies step toward the player one cell per
 * turn. HP <= 0 is game over.
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/game.h"
#include "tge-extra/grid.h"
#include "tge-extra/input.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN_W 52
#define WIN_H 24

/* Region geometry: the outer rects. Content areas are the inset x+1 / y+1 /
 * w-2 / h-2 (see MAP_INNER_* below and the STATS/LOG draws). */
#define MAP_X 1
#define MAP_Y 1
#define MAP_W 36
#define MAP_H 16

#define STATS_X 38
#define STATS_Y 1
#define STATS_W 13
#define STATS_H 16

#define LOG_X 1
#define LOG_Y 18
#define LOG_W 50
#define LOG_H 4

#define CTRL_Y 23

#define MAP_INNER_W (MAP_W - 2)
#define MAP_INNER_H (MAP_H - 2)

#define WALL_CHANCE 16
#define MAX_ENEMIES 8
#define ENEMIES_PER_FLOOR 3
#define GOLD_PILES 3
#define GOLD_VALUE 10
#define PLAYER_HP 20
#define PLAYER_ATK 5
#define ENEMY_HP 6
#define ENEMY_ATK 3

#define LOG_LINES 32
#define LOG_LINE_CAP 64
#define LOG_LINES_VISIBLE 2

typedef enum {
    TILE_WALL = 0,
    TILE_FLOOR,
    TILE_PLAYER,
    TILE_ENEMY,
    TILE_GOLD,
    TILE_EXIT,
} DungeonTile;

typedef struct {
    TGE_Vec2i pos;
    int hp;
    int atk;
} DungeonEnemy;

typedef struct {
    char tiles[MAP_INNER_H][MAP_INNER_W];
    TGE_Vec2i player;
    DungeonEnemy enemies[MAX_ENEMIES];
    int enemy_count;
    int player_hp;
    int player_max_hp;
    int player_atk;
    int gold;
    int floor;
    bool over;
} DungeonWorld;

typedef struct {
    char lines[LOG_LINES][LOG_LINE_CAP];
    int head;
    int count;
} DungeonLog;

typedef struct {
    TGE_Grid grid; /* the MAPA region interior, 1x1 cells */
} DungeonRenderer;

typedef struct {
    TGE_GameContext ctx;
    DungeonWorld world;
    DungeonRenderer renderer;
    DungeonLog log;
} DungeonGame;

static TGE_App *g_app = NULL;

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *ev);
static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_event(TGE_GameContext *ctx, TGE_Event *ev);
static void renderer_init(DungeonRenderer *r);
static void renderer_draw(DungeonRenderer *r, TGE_Canvas *canvas,
                          const DungeonWorld *w, const DungeonLog *log);
static void region_draw(TGE_Canvas *canvas, int x, int y, int w, int h,
                        const char *title, TGE_Color fg);
static void renderer_draw_map(DungeonRenderer *r, const DungeonWorld *w);
static void renderer_draw_stats(TGE_Canvas *canvas, const DungeonWorld *w);
static void renderer_draw_log(TGE_Canvas *canvas, const DungeonLog *log);
static void renderer_draw_controls(TGE_Canvas *canvas);
static void log_init(DungeonLog *log);
static void log_push(DungeonLog *log, const char *msg);
static const TGE_Sprite *tile_sprite(DungeonTile t);
static TGE_Color tile_color(DungeonTile t);
static TGE_Vec2i world_random_floor(const DungeonWorld *w);
static void world_gen_map(DungeonWorld *w);
static void world_move_player(DungeonWorld *w, TGE_Vec2i n);
static void world_attack_enemy(DungeonWorld *w, DungeonLog *log, TGE_Vec2i p);
static void world_player_act(DungeonWorld *w, DungeonLog *log,
                             TGE_Direction d);
static bool world_enemy_can_step(const DungeonWorld *w, TGE_Vec2i p);
static void world_enemies_turn(DungeonWorld *w, DungeonLog *log);
static void world_turn(DungeonWorld *w, DungeonLog *log, TGE_Direction d);
static void world_reset(DungeonWorld *w);

static const TGE_Sprite SPR_FLOOR  = TGE_SPRITE(1, 1, ".", ".");
static const TGE_Sprite SPR_WALL   = TGE_SPRITE(1, 1, "\xE2\x96\x88", "#");
static const TGE_Sprite SPR_PLAYER = TGE_SPRITE(1, 1, "@", "@");
static const TGE_Sprite SPR_ENEMY  = TGE_SPRITE(1, 1, "\xE2\x99\xA0", "E");
static const TGE_Sprite SPR_GOLD   = TGE_SPRITE(1, 1, "\xE2\x97\x86", "$");
static const TGE_Sprite SPR_EXIT   = TGE_SPRITE(1, 1, "\xE2\x96\xBC", ">");

static const TGE_GameCallbacks dungeon_callbacks = {
    .draw  = game_draw,
    .event = game_event,
};

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Dungeon");
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
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    const char *title    = " DUNGEON ";
    const char *subtitle = " four regions, one turn-based grid ";
    const char *controls = " Arrows/WASD move  [ENTER] wait  [ESC] menu ";
    const char *start    = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, w, h, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 - 4, title, TGE_COLOR_GREEN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 - 2, subtitle, TGE_COLOR_CYAN,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 1, controls, TGE_COLOR_WHITE,
                           TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, h / 2 + 3, start, TGE_COLOR_YELLOW,
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
        DungeonGame *g = (DungeonGame *)tge_game_create(
            g_app, sizeof(DungeonGame), &dungeon_callbacks);
        if (!g)
            return;
        renderer_init(&g->renderer);
        log_init(&g->log);
        world_reset(&g->world);
    }
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    DungeonGame *g = (DungeonGame *)tge_game_instance(ctx);
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    if (w < WIN_W || h < WIN_H) {
        tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
        tge_draw_centered_text(canvas, h / 2, " terminal too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }
    renderer_draw(&g->renderer, canvas, &g->world, &g->log);
    if (g->world.over) {
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    }
}

static void game_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    DungeonGame *g = (DungeonGame *)tge_game_instance(ctx);

    if (ev->type == TGE_EVENT_RESIZE)
        return;
    if (g->world.over) {
        if (tge_input_confirm(ev))
            world_reset(&g->world);
        else if (tge_input_quit(ev))
            TGE_Quit(ctx->app);
        else if (tge_input_cancel(ev))
            TGE_PopScene(ctx->app);
        return;
    }
    TGE_Direction d = tge_input_direction(ev);
    if (d != TGE_DIR_NONE) {
        world_turn(&g->world, &g->log, d);
        return;
    }
    if (tge_input_confirm(ev)) {
        world_turn(&g->world, &g->log, TGE_DIR_NONE); /* wait a turn */
        return;
    }
    if (tge_input_cancel(ev))
        TGE_PopScene(ctx->app);
    else if (tge_input_quit(ev))
        TGE_Quit(ctx->app);
}

static void renderer_init(DungeonRenderer *r)
{
    tge_grid_init(&r->grid, NULL);
    tge_grid_set_cell_size(&r->grid, 1, 1);
    tge_grid_set_origin(&r->grid, MAP_X + 1, MAP_Y + 1);
}

static void renderer_draw(DungeonRenderer *r, TGE_Canvas *canvas,
                          const DungeonWorld *w, const DungeonLog *log)
{
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
    tge_grid_attach(&r->grid, canvas);

    region_draw(canvas, MAP_X, MAP_Y, MAP_W, MAP_H, NULL, TGE_COLOR_CYAN);
    region_draw(canvas, STATS_X, STATS_Y, STATS_W, STATS_H, " STATS ",
                TGE_COLOR_YELLOW);
    region_draw(canvas, LOG_X, LOG_Y, LOG_W, LOG_H, " LOG ", TGE_COLOR_CYAN);

    renderer_draw_map(r, w);
    renderer_draw_stats(canvas, w);
    renderer_draw_log(canvas, log);
    renderer_draw_controls(canvas);
}

/* The repeated frame + title pattern, captured once: every bordered region
 * asks for exactly this, with a NULL title meaning "no title row". */
static void region_draw(TGE_Canvas *canvas, int x, int y, int w, int h,
                        const char *title, TGE_Color fg)
{
    tge_draw_frame(canvas, x, y, w, h, fg, TGE_COLOR_DEFAULT);
    if (title)
        tge_draw_text(canvas, x + 1, y, title, fg, TGE_COLOR_DEFAULT);
}

static void renderer_draw_map(DungeonRenderer *r, const DungeonWorld *w)
{
    for (int y = 0; y < MAP_INNER_H; y++)
        for (int x = 0; x < MAP_INNER_W; x++) {
            DungeonTile t = (DungeonTile)w->tiles[y][x];
            tge_grid_put(&r->grid, x, y, tile_sprite(t), tile_color(t),
                         TGE_COLOR_DEFAULT);
        }
}

static void renderer_draw_stats(TGE_Canvas *canvas, const DungeonWorld *w)
{
    tge_printf(canvas, STATS_X + 1, STATS_Y + 1, TGE_COLOR_WHITE,
               TGE_COLOR_DEFAULT, "HP : %d/%d", w->player_hp,
               w->player_max_hp);
    tge_printf(canvas, STATS_X + 1, STATS_Y + 2, TGE_COLOR_WHITE,
               TGE_COLOR_DEFAULT, "ATK: %d", w->player_atk);
    tge_printf(canvas, STATS_X + 1, STATS_Y + 3, TGE_COLOR_WHITE,
               TGE_COLOR_DEFAULT, "LV : %d", w->floor);
    tge_printf(canvas, STATS_X + 1, STATS_Y + 5, TGE_COLOR_WHITE,
               TGE_COLOR_DEFAULT, "Gold: %d", w->gold);
    tge_printf(canvas, STATS_X + 1, STATS_Y + 7, TGE_COLOR_WHITE,
               TGE_COLOR_DEFAULT, "Enemies: %d", w->enemy_count);
    tge_printf(canvas, STATS_X + 1, STATS_Y + 8, TGE_COLOR_WHITE,
               TGE_COLOR_DEFAULT, "Floor: %d", w->floor);
}

static void renderer_draw_log(TGE_Canvas *canvas, const DungeonLog *log)
{
    for (int i = 0; i < LOG_LINES_VISIBLE; i++) {
        if (i >= log->count)
            break;
        int idx = log->head - 1 - i;
        if (idx < 0)
            idx += LOG_LINES;
        tge_draw_text(canvas, LOG_X + 1, LOG_Y + LOG_H - 2 - i,
                      log->lines[idx], TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    }
}

static void renderer_draw_controls(TGE_Canvas *canvas)
{
    tge_draw_centered_text(canvas, CTRL_Y,
                           " Arrows/WASD move  ENTER wait  ESC menu  Q quit ",
                           TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
}

static void log_init(DungeonLog *log)
{
    log->head = 0;
    log->count = 0;
}

static void log_push(DungeonLog *log, const char *msg)
{
    snprintf(log->lines[log->head], LOG_LINE_CAP, "%s", msg);
    log->head = (log->head + 1) % LOG_LINES;
    if (log->count < LOG_LINES)
        log->count++;
}

static const TGE_Sprite *tile_sprite(DungeonTile t)
{
    switch (t) {
    case TILE_WALL:
        return &SPR_WALL;
    case TILE_PLAYER:
        return &SPR_PLAYER;
    case TILE_ENEMY:
        return &SPR_ENEMY;
    case TILE_GOLD:
        return &SPR_GOLD;
    case TILE_EXIT:
        return &SPR_EXIT;
    case TILE_FLOOR:
    default:
        return &SPR_FLOOR;
    }
}

static TGE_Color tile_color(DungeonTile t)
{
    switch (t) {
    case TILE_WALL:
        return TGE_COLOR_CYAN;
    case TILE_PLAYER:
        return TGE_COLOR_GREEN;
    case TILE_ENEMY:
        return TGE_COLOR_RED;
    case TILE_GOLD:
        return TGE_COLOR_YELLOW;
    case TILE_EXIT:
        return TGE_COLOR_MAGENTA;
    case TILE_FLOOR:
    default:
        return TGE_COLOR_DEFAULT;
    }
}

static TGE_Vec2i world_random_floor(const DungeonWorld *w)
{
    for (;;) {
        TGE_Vec2i p = tge_vec2i(1 + rand() % (MAP_INNER_W - 2),
                                1 + rand() % (MAP_INNER_H - 2));
        if (w->tiles[p.y][p.x] == TILE_FLOOR)
            return p;
    }
}

static void world_gen_map(DungeonWorld *w)
{
    for (int y = 0; y < MAP_INNER_H; y++)
        for (int x = 0; x < MAP_INNER_W; x++) {
            bool border = (x == 0 || y == 0 || x == MAP_INNER_W - 1 ||
                           y == MAP_INNER_H - 1);
            w->tiles[y][x] =
                (border || (rand() % 100) < WALL_CHANCE) ? TILE_WALL
                                                         : TILE_FLOOR;
        }

    w->enemy_count = 0;
    int enemies = ENEMIES_PER_FLOOR + w->floor;
    if (enemies > MAX_ENEMIES)
        enemies = MAX_ENEMIES;
    for (int i = 0; i < enemies; i++) {
        TGE_Vec2i p = world_random_floor(w);
        w->tiles[p.y][p.x] = TILE_ENEMY;
        w->enemies[w->enemy_count].pos = p;
        w->enemies[w->enemy_count].hp = ENEMY_HP;
        w->enemies[w->enemy_count].atk = ENEMY_ATK;
        w->enemy_count++;
    }
    for (int i = 0; i < GOLD_PILES; i++) {
        TGE_Vec2i p = world_random_floor(w);
        w->tiles[p.y][p.x] = TILE_GOLD;
    }
    TGE_Vec2i exit = world_random_floor(w);
    w->tiles[exit.y][exit.x] = TILE_EXIT;

    w->tiles[w->player.y][w->player.x] = TILE_PLAYER;
}

static void world_move_player(DungeonWorld *w, TGE_Vec2i n)
{
    w->tiles[w->player.y][w->player.x] = TILE_FLOOR;
    w->player = n;
    w->tiles[n.y][n.x] = TILE_PLAYER;
}

static void world_attack_enemy(DungeonWorld *w, DungeonLog *log, TGE_Vec2i p)
{
    for (int i = 0; i < w->enemy_count; i++) {
        DungeonEnemy *e = &w->enemies[i];
        if (e->pos.x != p.x || e->pos.y != p.y)
            continue;
        e->hp -= w->player_atk;
        log_push(log, "You strike the monster (5).");
        if (e->hp <= 0) {
            w->tiles[p.y][p.x] = TILE_FLOOR;
            w->enemy_count--;
            w->enemies[i] = w->enemies[w->enemy_count];
            log_push(log, "The monster dies.");
        }
        return;
    }
}

static void world_player_act(DungeonWorld *w, DungeonLog *log,
                             TGE_Direction d)
{
    TGE_Vec2i n = tge_vec2i_add(w->player, tge_direction_vec(d));
    switch ((DungeonTile)w->tiles[n.y][n.x]) {
    case TILE_WALL:
        log_push(log, "The wall blocks you.");
        return;
    case TILE_GOLD:
        w->gold += GOLD_VALUE;
        log_push(log, "You pick up gold (+10).");
        world_move_player(w, n);
        return;
    case TILE_EXIT:
        w->floor++;
        log_push(log, "You descend to the next floor.");
        world_gen_map(w);
        return;
    case TILE_ENEMY:
        world_attack_enemy(w, log, n);
        return;
    case TILE_FLOOR:
        world_move_player(w, n);
        return;
    default:
        return;
    }
}

static bool world_enemy_can_step(const DungeonWorld *w, TGE_Vec2i p)
{
    if (w->tiles[p.y][p.x] != TILE_FLOOR)
        return false;
    for (int i = 0; i < w->enemy_count; i++)
        if (w->enemies[i].pos.x == p.x && w->enemies[i].pos.y == p.y)
            return false;
    return true;
}

static void world_enemies_turn(DungeonWorld *w, DungeonLog *log)
{
    for (int i = 0; i < w->enemy_count; i++) {
        DungeonEnemy *e = &w->enemies[i];
        TGE_Vec2i d = tge_vec2i_sub(w->player, e->pos);
        if (abs(d.x) + abs(d.y) == 1) {
            w->player_hp -= e->atk;
            log_push(log, "The monster hits you (3).");
            if (w->player_hp <= 0) {
                w->player_hp = 0;
                w->over = true;
                log_push(log, "You die.");
                return;
            }
            continue;
        }
        int dx = (d.x > 0) - (d.x < 0);
        int dy = (d.y > 0) - (d.y < 0);
        TGE_Vec2i step;
        if (abs(d.x) >= abs(d.y)) {
            step = tge_vec2i(dx, 0);
            if (!world_enemy_can_step(w, tge_vec2i_add(e->pos, step)))
                step = tge_vec2i(0, dy);
        } else {
            step = tge_vec2i(0, dy);
            if (!world_enemy_can_step(w, tge_vec2i_add(e->pos, step)))
                step = tge_vec2i(dx, 0);
        }
        TGE_Vec2i target = tge_vec2i_add(e->pos, step);
        if (!world_enemy_can_step(w, target))
            continue;
        w->tiles[e->pos.y][e->pos.x] = TILE_FLOOR;
        e->pos = target;
        w->tiles[target.y][target.x] = TILE_ENEMY;
    }
}

static void world_turn(DungeonWorld *w, DungeonLog *log, TGE_Direction d)
{
    if (w->over)
        return;
    if (d != TGE_DIR_NONE)
        world_player_act(w, log, d);
    world_enemies_turn(w, log);
}

static void world_reset(DungeonWorld *w)
{
    w->player_hp = PLAYER_HP;
    w->player_max_hp = PLAYER_HP;
    w->player_atk = PLAYER_ATK;
    w->gold = 0;
    w->floor = 1;
    w->over = false;
    w->player = tge_vec2i(1, 1);
    world_gen_map(w);
}
