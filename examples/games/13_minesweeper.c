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

/* Minesweeper — Fase 3b #10. El juego que exige el mouse de verdad: clic
 * izquierdo revela, clic derecho marca (flag). Valida tres cosas nuevas:
 *   - entrada de mouse (TGE_EVENT_MOUSE*) mapeada a celda de grid,
 *   - flood fill BFS sobre el grid al revelar una celda con 0 minas vecinas,
 *   - generacion determinista con seed (xorshift32) para el layout de minas.
 * Reusa TGE_TileMap (rol por celda = lo que se dibuja) + TGE_Playfield, como
 * Sokoban/Pac-Man; el estado logico (mina / adyacencia) vive en arreglos del
 * mundo, no en tge-extra. */

#define MS_W 16
#define MS_H 16
#define MS_MINES 40
#define MS_BTN_LEFT  0
#define MS_BTN_RIGHT 2

typedef enum {
    CELL_HIDDEN = 0,
    CELL_FLAG,
    CELL_MINE,
    CELL_0, CELL_1, CELL_2, CELL_3, CELL_4,
    CELL_5, CELL_6, CELL_7, CELL_8
} MineRole;

typedef enum { MS_PLAYING, MS_WON, MS_LOST } MineState;

typedef struct {
    TGE_TileMap map;
    TGE_TileSet tiles;
    int         w, h, mines;
    bool        mine[MS_H][MS_W];
    int         adj[MS_H][MS_W];
    uint32_t    rng;
    int         first_click; /* 0 until the first reveal (safe-first-click) */
    int         flags;
    int         revealed_count;
    MineState   state;
    TGE_Vec2i   cur; /* keyboard cursor */
} MineWorld;

typedef struct {
    TGE_GameContext ctx;
    TGE_Playfield   pf;
    MineWorld       world;
} MineGame;

static TGE_App *g_app = NULL;

static const int MS_DX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
static const int MS_DY[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

static const TGE_Sprite SPRITE_EMPTY = TGE_SPRITE(1, 1, " ", NULL);
static const TGE_Sprite SPRITE_HIDDEN = TGE_SPRITE(1, 1, "\xe2\x96\x92", ".");
static const TGE_Sprite SPRITE_FLAG   = TGE_SPRITE(1, 1, "F", "F");
static const TGE_Sprite SPRITE_MINE   = TGE_SPRITE(1, 1, "*", "*");
static const TGE_Sprite SPRITE_N0 = TGE_SPRITE(1, 1, " ", " ");
static const TGE_Sprite SPRITE_N1 = TGE_SPRITE(1, 1, "1", "1");
static const TGE_Sprite SPRITE_N2 = TGE_SPRITE(1, 1, "2", "2");
static const TGE_Sprite SPRITE_N3 = TGE_SPRITE(1, 1, "3", "3");
static const TGE_Sprite SPRITE_N4 = TGE_SPRITE(1, 1, "4", "4");
static const TGE_Sprite SPRITE_N5 = TGE_SPRITE(1, 1, "5", "5");
static const TGE_Sprite SPRITE_N6 = TGE_SPRITE(1, 1, "6", "6");
static const TGE_Sprite SPRITE_N7 = TGE_SPRITE(1, 1, "7", "7");
static const TGE_Sprite SPRITE_N8 = TGE_SPRITE(1, 1, "8", "8");

static const TGE_GridTheme MINESWEEPER_THEME = {
    .empty          = &SPRITE_EMPTY,
    .default_sprite = &SPRITE_EMPTY,
    .border         = &SPRITE_EMPTY,
};

static const char *const MS_LEVELS_NOTE = "mouse: left reveal, right flag";

static void game_event  (TGE_GameContext *ctx, TGE_Event *event);
static void game_draw   (TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_update (TGE_GameContext *ctx, float delta_time);
static void game_destroy(TGE_GameContext *ctx);
static void game_world_resize(void *userdata, int grid_w, int grid_h);

static const TGE_GameCallbacks minesweeper_callbacks = {
    .update  = game_update,
    .draw    = game_draw,
    .event   = game_event,
    .destroy = game_destroy,
};

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *event);

static void world_init (MineWorld *world);
static void mine_new   (MineWorld *world, uint32_t seed);
static void world_reset(MineWorld *world);
static void world_resize(MineWorld *world, TGE_View *view, int grid_w, int grid_h);
static void place_mines(MineWorld *world);
static void compute_adj(MineWorld *world);
static uint32_t mine_rng_next(uint32_t *s);
static void reveal      (MineWorld *world, int x, int y);
static void toggle_flag (MineWorld *world, int x, int y);
static bool world_handle_input(MineWorld *world, const TGE_Event *event,
                               const TGE_Playfield *pf);
static bool mouse_to_cell(const TGE_Playfield *pf, const TGE_Event *ev,
                          int *cx, int *cy);

static void renderer_draw(TGE_Playfield *pf, const MineWorld *world,
                          TGE_Canvas *canvas);

static TGE_TileSet minesweeper_palette(void)
{
    TGE_TileSet pal;
    memset(&pal, 0, sizeof(pal));
    pal.tiles[CELL_HIDDEN].sprite = &SPRITE_HIDDEN;
    pal.tiles[CELL_HIDDEN].fg     = TGE_COLOR_CYAN;
    pal.tiles[CELL_HIDDEN].bg     = TGE_COLOR_DEFAULT;
    pal.tiles[CELL_FLAG].sprite    = &SPRITE_FLAG;
    pal.tiles[CELL_FLAG].fg        = TGE_COLOR_RED;
    pal.tiles[CELL_FLAG].bg        = TGE_COLOR_DEFAULT;
    pal.tiles[CELL_MINE].sprite    = &SPRITE_MINE;
    pal.tiles[CELL_MINE].fg        = TGE_COLOR_RED;
    pal.tiles[CELL_MINE].bg        = TGE_COLOR_DEFAULT;
    pal.tiles[CELL_0].sprite = &SPRITE_N0; pal.tiles[CELL_0].fg = TGE_COLOR_DEFAULT;
    pal.tiles[CELL_1].sprite = &SPRITE_N1; pal.tiles[CELL_1].fg = TGE_COLOR_BLUE;
    pal.tiles[CELL_2].sprite = &SPRITE_N2; pal.tiles[CELL_2].fg = TGE_COLOR_GREEN;
    pal.tiles[CELL_3].sprite = &SPRITE_N3; pal.tiles[CELL_3].fg = TGE_COLOR_RED;
    pal.tiles[CELL_4].sprite = &SPRITE_N4; pal.tiles[CELL_4].fg = TGE_COLOR_MAGENTA;
    pal.tiles[CELL_5].sprite = &SPRITE_N5; pal.tiles[CELL_5].fg = TGE_COLOR_YELLOW;
    pal.tiles[CELL_6].sprite = &SPRITE_N6; pal.tiles[CELL_6].fg = TGE_COLOR_CYAN;
    pal.tiles[CELL_7].sprite = &SPRITE_N7; pal.tiles[CELL_7].fg = TGE_COLOR_WHITE;
    pal.tiles[CELL_8].sprite = &SPRITE_N8; pal.tiles[CELL_8].fg = TGE_COLOR_DEFAULT;
    return pal;
}

#if defined(TGE_MINESWEEPER_TEST)
static int minesweeper_main(void)
#else
int main(void)
#endif
{
    TGE_App *app = TGE_Create(MS_W + 2, MS_H + 3, "TGE Minesweeper");
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
    tge_draw_centered_text(canvas, ch / 2 - 3, " MINESWEEPER ",
                           TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2 - 1, MS_LEVELS_NOTE,
                           TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2 + 1,
                           " reveal safe cells, flag the mines ",
                           TGE_COLOR_WHITE, TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, ch / 2 + 3,
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
        MineGame *game = (MineGame *)tge_game_create(
            g_app, sizeof(MineGame), &minesweeper_callbacks);
        if (!game)
            return;
        tge_playfield_init(&game->pf, &MINESWEEPER_THEME, TGE_GRID_SCALE_1X1,
                           MS_W, MS_H);
        tge_grid_set_origin(&game->pf.grid_view.grid, 0, 1);
        world_init(&game->world);
    }
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    MineGame *game = (MineGame *)tge_game_instance(ctx);
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
    MineGame *game = (MineGame *)tge_game_instance(ctx);
    MineWorld *world = &game->world;

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
    if (world->state == MS_WON || world->state == MS_LOST) {
        if (tge_input_confirm(event))
            world_reset(world); /* new layout (reseeded) */
        return;
    }
    world_handle_input(world, event, &game->pf);
}

static void game_destroy(TGE_GameContext *ctx)
{
    (void)ctx;
}

static void game_world_resize(void *userdata, int grid_w, int grid_h)
{
    MineGame *game = (MineGame *)userdata;
    world_resize(&game->world, &game->pf.view, grid_w, grid_h);
}

static void renderer_draw(TGE_Playfield *pf, const MineWorld *world,
                          TGE_Canvas *canvas)
{
    tge_playfield_attach(pf, canvas);
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);

    int remaining = world->mines - world->flags;
    const char *status = (world->state == MS_WON)  ? " YOU WIN! " :
                         (world->state == MS_LOST) ? " BOOM! "   : " MINESWEEPER ";
    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT,
               " %s  mines left: %d ", status, remaining);
    tge_playfield_draw_border(pf, TGE_COLOR_CYAN, TGE_COLOR_DEFAULT);

    if (!pf->view.valid) {
        tge_draw_centered_text(canvas, tge_canvas_height(canvas) / 2,
                               " too small ", TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }

    tge_tilemap_draw(&world->map, &pf->grid_view.grid, pf->view.area.x,
                     pf->view.area.y, &world->tiles);

    /* keyboard cursor highlight (only over an untouched cell) */
    uint8_t cr = tge_tilemap_get(&world->map, world->cur.x, world->cur.y);
    if (cr == CELL_HIDDEN || cr == CELL_FLAG)
        tge_grid_view_put_local(&pf->grid_view, &pf->view, world->cur,
                                &SPRITE_HIDDEN, TGE_COLOR_WHITE, TGE_COLOR_BLUE);

    if (world->state == MS_WON)
        tge_draw_modal(canvas, " YOU WIN! ",
                       " [ENTER] new game   [ESC] menu ", TGE_COLOR_GREEN);
    else if (world->state == MS_LOST)
        tge_draw_modal(canvas, " BOOM! ",
                       " [ENTER] new game   [ESC] menu ", TGE_COLOR_RED);
}

static uint32_t mine_rng_next(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void place_mines(MineWorld *world)
{
    int total = world->w * world->h;
    int order[MS_W * MS_H];
    for (int i = 0; i < total; i++)
        order[i] = i;
    /* Fisher-Yates shuffle, then the first `mines` positions become mines. */
    for (int i = total - 1; i > 0; i--) {
        int j = (int)(mine_rng_next(&world->rng) % (uint32_t)(i + 1));
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int k = 0; k < world->mines; k++) {
        int idx = order[k];
        world->mine[idx / world->w][idx % world->w] = true;
    }
}

static void compute_adj(MineWorld *world)
{
    for (int y = 0; y < world->h; y++) {
        for (int x = 0; x < world->w; x++) {
            if (world->mine[y][x]) {
                world->adj[y][x] = 0;
                continue;
            }
            int c = 0;
            for (int d = 0; d < 8; d++) {
                int nx = x + MS_DX[d], ny = y + MS_DY[d];
                if (nx < 0 || ny < 0 || nx >= world->w || ny >= world->h)
                    continue;
                if (world->mine[ny][nx])
                    c++;
            }
            world->adj[y][x] = c;
        }
    }
}

static void mine_new(MineWorld *world, uint32_t seed)
{
    world->rng = seed ? seed : 0x9E3779B9u;
    world->first_click = 0;
    world->flags = 0;
    world->revealed_count = 0;
    world->state = MS_PLAYING;
    world->cur = tge_vec2i(world->w / 2, world->h / 2);

    tge_tilemap_init(&world->map, world->w, world->h);
    for (int y = 0; y < world->h; y++) {
        for (int x = 0; x < world->w; x++) {
            world->mine[y][x] = false;
            world->adj[y][x] = 0;
            tge_tilemap_set(&world->map, x, y, CELL_HIDDEN);
        }
    }
    place_mines(world);
    compute_adj(world);
}

static void world_init(MineWorld *world)
{
    memset(world, 0, sizeof(*world));
    world->tiles = minesweeper_palette();
    world->w = MS_W;
    world->h = MS_H;
    world->mines = MS_MINES;
    world_reset(world);
}

static void world_reset(MineWorld *world)
{
    /* reseed each new game so layouts vary; still deterministic given a seed
     * when called from tests. */
    mine_new(world, world->rng + 1);
}

static void world_resize(MineWorld *world, TGE_View *view, int grid_w, int grid_h)
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

static void reveal(MineWorld *world, int sx, int sy)
{
    if (world->state != MS_PLAYING)
        return;
    if (sx < 0 || sy < 0 || sx >= world->w || sy >= world->h)
        return;
    uint8_t r0 = tge_tilemap_get(&world->map, sx, sy);
    if (r0 == CELL_FLAG || r0 != CELL_HIDDEN)
        return; /* flagged or already revealed */

    /* Safe first click: move the mine off the clicked cell if needed. */
    if (!world->first_click) {
        world->first_click = 1;
        if (world->mine[sy][sx]) {
            int total = world->w * world->h;
            int r;
            do {
                r = (int)(mine_rng_next(&world->rng) % (uint32_t)total);
            } while (world->mine[r / world->w][r % world->w]);
            int rx = r % world->w, ry = r / world->w;
            world->mine[sy][sx] = false;
            world->mine[ry][rx] = true;
            compute_adj(world);
        }
    }

    if (world->mine[sy][sx]) {
        tge_tilemap_set(&world->map, sx, sy, CELL_MINE);
        world->state = MS_LOST;
        for (int y = 0; y < world->h; y++)
            for (int x = 0; x < world->w; x++)
                if (world->mine[y][x])
                    tge_tilemap_set(&world->map, x, y, CELL_MINE);
        return;
    }

    /* Flood fill (BFS) over connected zero-adjacency regions. Each cell is
     * enqueued at most once, so a queue of w*h plus a `queued` guard is
     * enough even when many neighbours re-discover the same cell. */
    int queue[MS_W * MS_H];
    bool queued[MS_H][MS_W] = {0};
    int qh = 0, qt = 0;
    queue[qt++] = sy * world->w + sx;
    queued[sy][sx] = true;
    while (qh < qt) {
        int idx = queue[qh++];
        int x = idx % world->w, y = idx / world->w;
        queued[y][x] = false;
        if (world->mine[y][x])
            continue; /* never auto-reveal a mine */
        if (tge_tilemap_get(&world->map, x, y) != CELL_HIDDEN)
            continue;
        tge_tilemap_set(&world->map, x, y, (MineRole)(CELL_0 + world->adj[y][x]));
        world->revealed_count++;
        if (world->adj[y][x] == 0) {
            for (int d = 0; d < 8; d++) {
                int nx = x + MS_DX[d], ny = y + MS_DY[d];
                if (nx < 0 || ny < 0 || nx >= world->w || ny >= world->h)
                    continue;
                if (world->mine[ny][nx])
                    continue;
                if (!queued[ny][nx] &&
                    tge_tilemap_get(&world->map, nx, ny) == CELL_HIDDEN) {
                    queued[ny][nx] = true;
                    queue[qt++] = ny * world->w + nx;
                }
            }
        }
    }

    if (world->revealed_count == world->w * world->h - world->mines)
        world->state = MS_WON;
}

static void toggle_flag(MineWorld *world, int x, int y)
{
    if (world->state != MS_PLAYING)
        return;
    if (x < 0 || y < 0 || x >= world->w || y >= world->h)
        return;
    uint8_t r = tge_tilemap_get(&world->map, x, y);
    if (r == CELL_HIDDEN) {
        tge_tilemap_set(&world->map, x, y, CELL_FLAG);
        world->flags++;
    } else if (r == CELL_FLAG) {
        tge_tilemap_set(&world->map, x, y, CELL_HIDDEN);
        world->flags--;
    }
}

static bool mouse_to_cell(const TGE_Playfield *pf, const TGE_Event *ev,
                           int *cx, int *cy)
{
    /* mouse coords are 1-based terminal cells; the view area is in canvas
     * coordinates, so convert to 0-based local, reject outside the area. */
    int lx = ev->data.mouse.x - 1 - pf->view.area.x;
    int ly = ev->data.mouse.y - 1 - pf->view.area.y;
    if (!tge_view_contains(&pf->view, tge_vec2i(lx, ly)))
        return false;
    /* the grid itself is offset by its origin (set via tge_grid_set_origin)
     * inside the playfield area, so subtract that to reach the cell. */
    *cx = lx - pf->grid_view.grid.ox;
    *cy = ly - pf->grid_view.grid.oy;
    if (*cx < 0 || *cy < 0 || *cx >= pf->view.area.w || *cy >= pf->view.area.h)
        return false; /* header/border rows the grid doesn't cover */
    return true;
}

static bool world_handle_input(MineWorld *world, const TGE_Event *event,
                               const TGE_Playfield *pf)
{
    if (event->type == TGE_EVENT_MOUSEDOWN) {
        int cx, cy;
        if (!mouse_to_cell(pf, event, &cx, &cy))
            return true;
        if (event->data.mouse.button == MS_BTN_RIGHT)
            toggle_flag(world, cx, cy);
        else
            reveal(world, cx, cy);
        return true;
    }

    /* keyboard fallback: arrows move cursor, space reveals, f flags */
    TGE_Direction dir = tge_input_direction(event);
    if (dir != TGE_DIR_NONE) {
        TGE_Vec2i step = tge_direction_vec(dir);
        world->cur = tge_vec2i(
            (world->cur.x + step.x + world->w) % world->w,
            (world->cur.y + step.y + world->h) % world->h);
        return true;
    }
    if (event->type == TGE_EVENT_KEYDOWN &&
        event->data.key.keycode == TGE_KEY_SPACE) {
        reveal(world, world->cur.x, world->cur.y);
        return true;
    }
    if (event->type == TGE_EVENT_TEXT) {
        uint32_t c = event->data.text.codepoint;
        if (c == 'f' || c == 'F') {
            toggle_flag(world, world->cur.x, world->cur.y);
            return true;
        }
    }
    return false;
}
