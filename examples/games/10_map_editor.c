/* 10_map_editor - A minimal tile map editor: the third composition consumer
 * (after Tetris and the dungeon), written to validate whether the "screen
 * region" pattern seen twice before survives contact with an editor layout
 * whose regions are not static HUD boxes.
 *
 *   TOOLS   a toolbar panel, bordered + titled.
 *   MAP     one TGE_Grid of 1x1 cells, fully editable, with a REVERSE cursor.
 *   PALETTE a second interactive region (not the map): a list with a
 *           highlighted selection, bordered + titled.
 *   STATUS  a dynamic text line, deliberately no border.
 *
 * Two regions take input (MAP, PALETTE) and TAB moves focus between them;
 * the question under study is whether the region needs to know anything about
 * input. The answer this file argues: no - game_event resolves focus and both
 * regions stay plain geometry + a draw function.
 *
 * Every region is a TGE_Rect from the core, and its content area is
 * tge_rect_inset(r, 1) - the x+1 / y+1 / w-2 / h-2 relation this example
 * reached for a third time and that the core now provides. The draw helper
 * (frame + optional title) was extracted into tge-extra/ui as tge_draw_region;
 * this example now consumes it instead of keeping a local copy.
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/game.h"
#include "tge-extra/grid.h"
#include "tge-extra/input.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/ui.h"

#include <stdio.h>
#include <stdlib.h>

#define WIN_W 52
#define WIN_H 24

/* Region geometry. Sizes are compile-time macros because they size the MAP
 * cell array below; positions are plain literals inside the rects. */
#define TOOLS_W 50
#define TOOLS_H 3
#define MAP_W 36
#define MAP_H 15
#define PALETTE_W 13
#define PALETTE_H 15

static const TGE_Rect TOOLS   = { 1, 1, TOOLS_W, TOOLS_H };
static const TGE_Rect MAP     = { 1, 5, MAP_W, MAP_H };
static const TGE_Rect PALETTE = { 38, 5, PALETTE_W, PALETTE_H };
static const TGE_Rect STATUS  = { 1, 21, TOOLS_W, 1 };

#define CTRL_Y 22

/* The MAP grid occupies the inset content rect. Compile-time because it sizes the
 * cell array; the drawing code derives the interior from the rect instead. */
#define MAP_GRID_W (MAP_W - 2)
#define MAP_GRID_H (MAP_H - 2)

typedef enum {
    TILE_EMPTY = 0,
    TILE_WALL,
    TILE_FLOOR,
    TILE_DOOR,
} TileId;

typedef struct {
    TileId id;
    const char *name;
    const TGE_Sprite *sprite;
    TGE_Color color;
} EditorTile;

#define PALETTE_MAX 4

typedef enum {
    FOCUS_MAP = 0,
    FOCUS_PALETTE,
} Focus;

typedef struct {
    TGE_Grid grid; /* the MAP region interior, 1x1 cells */
} EditorRenderer;

typedef struct {
    TGE_GameContext ctx;
    EditorRenderer renderer;
    EditorTile palette[PALETTE_MAX];
    int palette_count;
    TileId cells[MAP_GRID_H][MAP_GRID_W];
    TGE_Vec2i cursor;
    int sel;
    Focus focus;
} EditorGame;

static TGE_App *g_app = NULL;

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *ev);
static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_event(TGE_GameContext *ctx, TGE_Event *ev);
static void renderer_init(EditorRenderer *r);
static void renderer_draw(EditorRenderer *r, TGE_Canvas *canvas,
                          const EditorGame *g);
static void palette_init(EditorTile *palette);
static void editor_reset(EditorGame *g);
static void editor_move_cursor(EditorGame *g, TGE_Direction d);
static void editor_move_selection(EditorGame *g, TGE_Direction d);
static void editor_paint(EditorGame *g);
static void editor_clear(EditorGame *g);
static const TGE_Sprite *tile_sprite(TileId t);
static TGE_Color tile_color(TileId t);
static void renderer_draw_tools(TGE_Canvas *canvas);
static void renderer_draw_map(EditorRenderer *r, const EditorGame *g);
static void renderer_draw_palette(TGE_Canvas *canvas, const EditorGame *g);
static void renderer_draw_status(TGE_Canvas *canvas, const EditorGame *g);
static void renderer_draw_controls(TGE_Canvas *canvas);

static const TGE_Sprite SPR_WALL  = TGE_SPRITE(1, 1, "\xE2\x96\x88", "#");
static const TGE_Sprite SPR_FLOOR = TGE_SPRITE(1, 1, ".", ".");
static const TGE_Sprite SPR_DOOR  = TGE_SPRITE(1, 1, "+", NULL);
static const TGE_Sprite SPR_EMPTY = TGE_SPRITE(1, 1, " ", NULL);

static const TGE_GameCallbacks editor_callbacks = {
    .draw  = game_draw,
    .event = game_event,
};

int main(void)
{
    TGE_App *app = TGE_Create(WIN_W, WIN_H, "TGE Map Editor");
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
    const char *title    = " MAP EDITOR ";
    const char *subtitle = " two interactive regions, one grid ";
    const char *controls = " TAB switch focus  arrows/WASD move  ENTER paint ";
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
        EditorGame *g = (EditorGame *)tge_game_create(
            g_app, sizeof(EditorGame), &editor_callbacks);
        if (!g)
            return;
        renderer_init(&g->renderer);
        palette_init(g->palette);
        g->palette_count = PALETTE_MAX;
        editor_reset(g);
    }
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    EditorGame *g = (EditorGame *)tge_game_instance(ctx);
    int w = tge_canvas_width(canvas);
    int h = tge_canvas_height(canvas);
    if (w < WIN_W || h < WIN_H) {
        tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
        tge_draw_centered_text(canvas, h / 2, " terminal too small ",
                               TGE_COLOR_RED, TGE_COLOR_DEFAULT);
        return;
    }
    renderer_draw(&g->renderer, canvas, g);
}

static void game_event(TGE_GameContext *ctx, TGE_Event *ev)
{
    EditorGame *g = (EditorGame *)tge_game_instance(ctx);

    if (ev->type == TGE_EVENT_RESIZE)
        return;
    if (tge_input_cancel(ev)) {
        TGE_PopScene(ctx->app);
        return;
    }
    if (tge_input_quit(ev)) {
        TGE_Quit(ctx->app);
        return;
    }
    if (ev->type == TGE_EVENT_KEYDOWN && ev->data.key.keycode == TGE_KEY_TAB) {
        g->focus = (g->focus == FOCUS_MAP) ? FOCUS_PALETTE : FOCUS_MAP;
        return;
    }
    TGE_Direction d = tge_input_direction(ev);
    if (d != TGE_DIR_NONE) {
        if (g->focus == FOCUS_MAP)
            editor_move_cursor(g, d);
        else
            editor_move_selection(g, d);
        return;
    }
    if (tge_input_confirm(ev)) {
        if (g->focus == FOCUS_MAP)
            editor_paint(g);
        else
            g->focus = FOCUS_MAP; /* a palette pick hands back to the map */
        return;
    }
    if (ev->type == TGE_EVENT_TEXT && ev->data.text.codepoint == 'c')
        editor_clear(g);
}

static void renderer_init(EditorRenderer *r)
{
    tge_grid_init(&r->grid, NULL);
    tge_grid_set_cell_size(&r->grid, 1, 1);
    TGE_Rect inner = tge_rect_inset(MAP, 1);
    tge_grid_set_origin(&r->grid, inner.x, inner.y);
}

static void renderer_draw(EditorRenderer *r, TGE_Canvas *canvas,
                          const EditorGame *g)
{
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
    tge_grid_attach(&r->grid, canvas);

    tge_draw_region(canvas, TOOLS, " TOOLS ", TGE_COLOR_CYAN);
    tge_draw_region(canvas, MAP, NULL, TGE_COLOR_CYAN);
    tge_draw_region(canvas, PALETTE, " PALETTE ", TGE_COLOR_YELLOW);

    renderer_draw_tools(canvas);
    renderer_draw_map(r, g);
    renderer_draw_palette(canvas, g);
    renderer_draw_status(canvas, g);
    renderer_draw_controls(canvas);
}

static void renderer_draw_tools(TGE_Canvas *canvas)
{
    TGE_Rect inner = tge_rect_inset(TOOLS, 1);
    tge_draw_text(canvas, inner.x, inner.y,
                  " TAB switch  ENTER paint  C clear  ESC menu ",
                  TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
}

static void renderer_draw_map(EditorRenderer *r, const EditorGame *g)
{
    for (int y = 0; y < MAP_GRID_H; y++)
        for (int x = 0; x < MAP_GRID_W; x++) {
            TileId t = g->cells[y][x];
            uint8_t attr = (x == g->cursor.x && y == g->cursor.y)
                               ? TGE_CELL_ATTR_REVERSE
                               : 0;
            tge_grid_put_attr(&r->grid, x, y, tile_sprite(t), tile_color(t),
                              TGE_COLOR_DEFAULT, attr);
        }
}

static void renderer_draw_palette(TGE_Canvas *canvas, const EditorGame *g)
{
    TGE_Rect inner = tge_rect_inset(PALETTE, 1);
    for (int i = 0; i < g->palette_count; i++) {
        if (i >= inner.h)
            break;
        const EditorTile *t = &g->palette[i];
        bool sel = (i == g->sel);
        char line[12];
        snprintf(line, sizeof line, "%s %s", sel ? ">" : " ", t->name);
        TGE_Color fg = sel ? TGE_COLOR_BLACK : t->color;
        TGE_Color bg = sel ? TGE_COLOR_WHITE : TGE_COLOR_DEFAULT;
        tge_draw_text(canvas, inner.x, inner.y + i, line, fg, bg);
    }
}

static void renderer_draw_status(TGE_Canvas *canvas, const EditorGame *g)
{
    const char *focus = (g->focus == FOCUS_MAP) ? "MAP" : "PALETTE";
    tge_printf(canvas, STATUS.x, STATUS.y, TGE_COLOR_WHITE, TGE_COLOR_DEFAULT,
               " Focus: %s  Tile: %s  Cursor: (%d,%d) ", focus,
               g->palette[g->sel].name, g->cursor.x, g->cursor.y);
}

static void renderer_draw_controls(TGE_Canvas *canvas)
{
    tge_draw_centered_text(
        canvas, CTRL_Y, " TAB switch  arrows/WASD move  ENTER paint  ESC menu",
        TGE_COLOR_GREEN, TGE_COLOR_DEFAULT);
}

static void palette_init(EditorTile *palette)
{
    palette[0] = (EditorTile){ TILE_WALL, "Wall", &SPR_WALL, TGE_COLOR_CYAN };
    palette[1] = (EditorTile){ TILE_FLOOR, "Floor", &SPR_FLOOR, TGE_COLOR_WHITE };
    palette[2] = (EditorTile){ TILE_DOOR, "Door", &SPR_DOOR, TGE_COLOR_YELLOW };
    palette[3] = (EditorTile){ TILE_EMPTY, "Erase", &SPR_EMPTY,
                               TGE_COLOR_DEFAULT };
}

static const TGE_Sprite *tile_sprite(TileId t)
{
    switch (t) {
    case TILE_WALL:
        return &SPR_WALL;
    case TILE_FLOOR:
        return &SPR_FLOOR;
    case TILE_DOOR:
        return &SPR_DOOR;
    case TILE_EMPTY:
    default:
        return &SPR_EMPTY;
    }
}

static TGE_Color tile_color(TileId t)
{
    switch (t) {
    case TILE_WALL:
        return TGE_COLOR_CYAN;
    case TILE_FLOOR:
        return TGE_COLOR_WHITE;
    case TILE_DOOR:
        return TGE_COLOR_YELLOW;
    case TILE_EMPTY:
    default:
        return TGE_COLOR_DEFAULT;
    }
}

static void editor_move_cursor(EditorGame *g, TGE_Direction d)
{
    TGE_Vec2i n = tge_vec2i_add(g->cursor, tge_direction_vec(d));
    if (n.x < 0 || n.y < 0 || n.x >= MAP_GRID_W || n.y >= MAP_GRID_H)
        return;
    g->cursor = n;
}

static void editor_move_selection(EditorGame *g, TGE_Direction d)
{
    if (d == TGE_DIR_UP && g->sel > 0)
        g->sel--;
    if (d == TGE_DIR_DOWN && g->sel < g->palette_count - 1)
        g->sel++;
}

static void editor_paint(EditorGame *g)
{
    g->cells[g->cursor.y][g->cursor.x] = g->palette[g->sel].id;
}

static void editor_clear(EditorGame *g)
{
    for (int y = 0; y < MAP_GRID_H; y++)
        for (int x = 0; x < MAP_GRID_W; x++)
            g->cells[y][x] = TILE_EMPTY;
}

static void editor_reset(EditorGame *g)
{
    editor_clear(g);
    g->cursor = tge_vec2i(MAP_GRID_W / 2, MAP_GRID_H / 2);
    g->sel = 0;
    g->focus = FOCUS_MAP;
}
