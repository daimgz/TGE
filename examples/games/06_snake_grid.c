/* 06_snake_grid - Snake with square pixels, a grid theme and the standard
 * TGE layering (world / renderer).
 *
 * Terminal cells are not square (~1:2), so the plain 01_snake looks
 * stretched. This version renders the whole playfield through a TGE_GridView
 * at cell size 2x1 (square pixels): every logical cell becomes a 2-character
 * block, so cells look square and horizontal motion matches vertical.
 *
 * Reading order: the file has two levels.
 *
 *   Level 1 (top)    the whole game at a glance: the types, the scene
 *                    callbacks (event / update / draw) and renderer_draw,
 *                    which is a list of steps. ~30 lines of flow, no
 *                    implementation.
 *   Level 2 (bottom) how each step works: the world rules (TGE_FixedStep +
 *                    TGE_InputBuffer + TGE_View) and the drawing helpers
 *                    that translate logical coordinates through a
 *                    TGE_GridView.
 *
 * It is the reference example of the game architecture the extra modules
 * serve, so the code is split into three concerns:
 *
 *   SnakeWorld     pure game state and rules: the snake, the food, scoring,
 *                  the step timer (TGE_FixedStep), the playfield layout
 *                  (TGE_View: min size, validity, first layout) and the queued
 *                  turns (TGE_InputBuffer). No canvas, no drawing.
 *   SnakeRenderer  everything that touches the screen: canvas clear, HUD
 *                  texts (tge_printf), and the playfield through a
 *                  TGE_GridView (theme, cell size, origin). The world is only
 *                  read, never changed.
 *   SnakeGame      the whole game: the scene glue (TGE_App/Scene wiring) that
 *                  owns one SnakeWorld + one SnakeRenderer, moves turns into
 *                  the input buffer, resizes into world_resize and draws
 *                  through the renderer.
 *
 * The playfield (logical LX x LY in grid cells) comes from the terminal size
 * through TGE_View: on a resize the interior (view.area) is recomputed and
 * the snake is clamped into the new bounds; the first VALID layout spawns a
 * fresh snake, so a too-small start simply waits until the terminal grows.
 */
#include "tge/tge.h"

#include "tge-extra/direction.h"
#include "tge-extra/fixedstep.h"
#include "tge-extra/grid.h"
#include "tge-extra/grid_view.h"
#include "tge-extra/input.h"
#include "tge-extra/input_buffer.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdlib.h>

/* Minimum playfield interior (cells inside the border): the game asks for at
 * least this before a layout counts as valid. At cell size 2x1 with a HUD row
 * the minimum canvas is 2*(MIN_PLAYFIELD_WIDTH+2) by
 * MIN_PLAYFIELD_HEIGHT+3. */
#define MIN_PLAYFIELD_WIDTH 10
#define MIN_PLAYFIELD_HEIGHT 6
#define MOVE_INTERVAL 0.10f
#define DIRECTION_QUEUE_SIZE 4

/* ═══════════════════════════════════════════════════════════════════════
 * LEVEL 1 - the whole game, read top to bottom
 * ═══════════════════════════════════════════════════════════════════════ */

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

/* The snake: its cells, its queued turns, its step timer and its layout.
 * The whole game state lives here. */
typedef struct {
    TGE_Vec2i *body; /* growable; capacity = playfield area */
    int body_capacity;
    int body_length;
    TGE_Direction direction;
    TGE_InputBuffer input; /* queued turns, one applied per step */
    TGE_Vec2i food;
    int score;
    SnakeState state;
    bool paused;
    TGE_FixedStep step;
    TGE_View view; /* logical playfield layout; view.area is the interior */
    int last_grid_width;  /* grid width the view was computed for */
    int last_grid_height; /* grid height the view was computed for */
} SnakeWorld;

/* How the playfield is drawn (2x1 grid cells + a theme). It only reads the
 * world, never changes it. */
typedef struct {
    const TGE_GridTheme *theme;
    TGE_GridView view;
} SnakeRenderer;

/* The game as a whole: world (rules) + renderer (screen). */
typedef struct {
    SnakeWorld world;
    SnakeRenderer renderer;
} SnakeGame;

static TGE_App *g_app = NULL;

/* The steps the flow calls are implemented in Level 2, below the divider. */
static void game_layout(SnakeGame *game, int canvas_width, int canvas_height);
static bool world_handle_input(SnakeWorld *world, TGE_Event *event);
static void world_update(SnakeWorld *world, float delta_time);
static void world_resize(SnakeWorld *world, int grid_width, int grid_height);
static void world_reset(SnakeWorld *world);
static void renderer_draw(SnakeRenderer *renderer, TGE_Canvas *canvas,
                          const SnakeWorld *world);
static void renderer_begin(SnakeRenderer *renderer, TGE_Canvas *canvas);
static void draw_hud(SnakeRenderer *renderer, TGE_Canvas *canvas,
                     const SnakeWorld *world);
static void draw_playfield(SnakeRenderer *renderer, TGE_Canvas *canvas,
                           const SnakeWorld *world);
static void draw_snake(SnakeRenderer *renderer, const SnakeWorld *world);
static void draw_food(SnakeRenderer *renderer, const SnakeWorld *world);
static void draw_overlay(TGE_Canvas *canvas, const SnakeWorld *world);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *event);

/* ── the renderer, in one glance: a list of steps ──────────────────────── */

static void renderer_draw(SnakeRenderer *renderer, TGE_Canvas *canvas,
                          const SnakeWorld *world)
{
    renderer_begin(renderer, canvas);        /* clear + attach the grid */
    draw_hud(renderer, canvas, world);       /* score */
    draw_playfield(renderer, canvas, world); /* border, "too small" */
    draw_snake(renderer, world);             /* body + head */
    draw_food(renderer, world);              /* bold food */
    draw_overlay(canvas, world);             /* GAME OVER / PAUSED */
}

/* ── the scene callbacks: the whole loop, ~20 lines ────────────────────── */

static void game_event(TGE_Scene *scene, TGE_Event *event)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;

    if (event->type == TGE_EVENT_RESIZE) {
        /* The terminal changed: recompute the layout and freeze the game so
         * the new bounds are not applied mid-movement. */
        game_layout(game, event->data.resize.w, event->data.resize.h);
        if (game->world.state != SNAKE_OVER)
            game->world.paused = true;
        return;
    }
    if (tge_input_pause(event)) {
        if (game->world.state != SNAKE_OVER)
            game->world.paused = !game->world.paused;
        return;
    }
    /* While paused only ESC (cancel) reaches the rest of the handler. */
    if (game->world.paused && !tge_input_cancel(event))
        return;
    if (world_handle_input(&game->world, event))
        return;
    if (tge_input_quit(event)) {
        if (game->world.state == SNAKE_OVER)
            TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(event)) {
        if (game->world.state == SNAKE_OVER && game->world.view.valid)
            world_reset(&game->world);
        return;
    }
    if (tge_input_cancel(event))
        TGE_PopScene(g_app);
}

static void game_update(TGE_Scene *scene, float delta_time)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;
    world_update(&game->world, delta_time);
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;
    game_layout(game, tge_canvas_width(canvas), tge_canvas_height(canvas));
    renderer_draw(&game->renderer, canvas, &game->world);
}

/* Free the body array (growable) allocated by world_resize(). */
static void game_destroy(TGE_Scene *scene)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;
    free(game->world.body);
}

static void init_app(TGE_App *app)
{
    g_app = app;
    /* The title is a draw/event-only scene (no update). opaque = false so it
     * does not erase whatever is underneath it. */
    TGE_Scene *title = NULL;
    tge_scene_create(&title, 0, NULL, title_draw, title_event, NULL);
    title->opaque = false;
    TGE_PushScene(app, title);
}

int main(void)
{
    /* Requested size is the minimum/fallback: the core starts with the real
     * terminal size when it can query it (TIOCGWINSZ). At 2x1 cells the grid
     * needs 2*(MIN_PLAYFIELD_WIDTH+2) columns and
     * MIN_PLAYFIELD_HEIGHT+3 rows. */
    TGE_App *app = TGE_Create(2 * (MIN_PLAYFIELD_WIDTH + 2),
                              MIN_PLAYFIELD_HEIGHT + 3, "TGE Snake 2x1");
    if (!app)
        return 1;
    TGE_Run(app, init_app, NULL, NULL, NULL);
    TGE_Destroy(app);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * LEVEL 2 - how each step works
 * ═══════════════════════════════════════════════════════════════════════ */

/* ── game glue: canvas size -> logical grid size -> layout ─────────────── */

/* The renderer knows how a canvas maps to grid cells; the world just applies
 * the layout. Only recomputed when the logical grid size changed. */
static void game_layout(SnakeGame *game, int canvas_width, int canvas_height)
{
    int grid_width, grid_height;
    tge_grid_view_size_for(&game->renderer.view, canvas_width, canvas_height,
                           &grid_width, &grid_height);
    if (game->world.last_grid_width != grid_width ||
        game->world.last_grid_height != grid_height)
        world_resize(&game->world, grid_width, grid_height);
}

/* ── world: the rules ──────────────────────────────────────────────────── */

static void world_init(SnakeWorld *world)
{
    /* The view starts at the minimum interior; once the terminal is big
     * enough, world_resize() updates it to the real grid size. */
    tge_view_init(&world->view, MIN_PLAYFIELD_WIDTH, MIN_PLAYFIELD_HEIGHT);
    /* Turns are queued so fast key presses are not lost between fixed steps:
     * up to DIRECTION_QUEUE_SIZE pending turns. */
    tge_input_buffer_init(&world->input, DIRECTION_QUEUE_SIZE);
}

/* The world works in local coordinates (0..w-1, 0..h-1); the renderer is the
 * only one that maps them to the screen with tge_rect_translate_point().
 * view.area is that mapping's screen offset, so this returns the world's own
 * 0-origin bounds. */
static TGE_Rect world_bounds(const SnakeWorld *world)
{
    return tge_rect(0, 0, world->view.area.w, world->view.area.h);
}

static bool world_spawn_food(SnakeWorld *world)
{
    /* If the playfield is full of snake there is nowhere to put the food. */
    if (world->view.area.w * world->view.area.h - world->body_length <= 0)
        return false;
    for (;;) {
        /* Pick a random cell inside the playfield... */
        TGE_Vec2i candidate = tge_rect_random_point(world_bounds(world));
        bool free_spot = true;
        /* ...and retry until it does not overlap the snake. */
        for (int i = 0; i < world->body_length; i++) {
            if (tge_vec2i_eq(world->body[i], candidate)) {
                free_spot = false;
                break;
            }
        }
        if (free_spot) {
            world->food = candidate;
            return true;
        }
    }
}

static void world_reset(SnakeWorld *world)
{
    /* Start a 3-cell snake centered on the playfield, heading right. */
    int center_x = world->view.area.w / 2;
    int center_y = world->view.area.h / 2;
    world->body_length = 3;
    world->body[0] = tge_vec2i(center_x, center_y);     /* head */
    world->body[1] = tge_vec2i(center_x - 1, center_y); /* body */
    world->body[2] = tge_vec2i(center_x - 2, center_y); /* tail */
    world->direction = TGE_DIR_RIGHT;
    world->score = 0;
    world->state = SNAKE_RUNNING;
    world->paused = false;
    /* MOVE_INTERVAL seconds between steps: the timer accumulates dt and fires
     * one step per interval (see world_update). */
    tge_fixedstep_init(&world->step, MOVE_INTERVAL);
    /* Drop any turns queued by a previous game. */
    tge_input_buffer_clear(&world->input);
    world_spawn_food(world);
}

static bool world_step(SnakeWorld *world)
{
    /* Apply the oldest queued turn that is not the current direction (or its
     * reverse, which would make the snake run into itself). */
    TGE_Direction direction;
    while (tge_input_buffer_pop(&world->input, &direction)) {
        if (direction == tge_direction_opposite(world->direction) ||
            direction == world->direction)
            continue;
        world->direction = direction;
        break;
    }

    /* The head moves one cell along the current direction... */
    TGE_Vec2i next_head =
        tge_vec2i_add(world->body[0], tge_direction_vec(world->direction));

    /* ...hitting the playfield wall ends the game. */
    if (next_head.x < 0 || next_head.x >= world->view.area.w ||
        next_head.y < 0 || next_head.y >= world->view.area.h)
        return false;

    /* Hitting the body ends the game too. When eating, the tail moves forward
     * out of the way, so the old tail cell is not part of the check. */
    bool ate = tge_vec2i_eq(next_head, world->food);
    int cells_to_check = world->body_length - (ate ? 0 : 1);
    for (int i = 0; i < cells_to_check; i++) {
        if (tge_vec2i_eq(next_head, world->body[i]))
            return false;
    }

    /* Shift every segment forward: the tail frees its cell and the head takes
     * the new one. */
    for (int i = world->body_length; i > 0; i--)
        world->body[i] = world->body[i - 1];
    world->body[0] = next_head;

    /* Eating grows the snake by one and places new food; if there is no room
     * left for it the game is over. */
    if (ate) {
        world->body_length++;
        world->score += 10;
        return world_spawn_food(world);
    }
    return true;
}

/* Recompute the playfield layout for a new logical grid size (grid_width x
 * grid_height grid cells, the renderer decides how a canvas maps to them).
 * The view decides what the layout means: the first valid one spawns a fresh
 * snake, later resizes keep the current one (clamped into the new bounds,
 * food respawned if it no longer fits), and a too-small size stays inactive
 * until the terminal grows. */
static void world_resize(SnakeWorld *world, int grid_width, int grid_height)
{
    /* Remember which grid size the current layout was computed for, so
     * game_draw() only recomputes when the terminal actually changed. */
    world->last_grid_width = grid_width;
    world->last_grid_height = grid_height;
    /* The view reconciles the requested size with the real one and tells us
     * what the new layout means for the game. */
    TGE_ViewUpdate view_update = tge_view_update(&world->view, grid_width,
                                                 grid_height);

    /* The snake body lives in a heap array sized to the playfield area (at
     * most one snake cell per grid cell). Grow/shrink it on resize. */
    int capacity = world->view.area.w * world->view.area.h;
    if (capacity < 1)
        capacity = 1;
    if (capacity != world->body_capacity) {
        TGE_Vec2i *new_body =
            (TGE_Vec2i *)realloc(world->body,
                                 (size_t)capacity * sizeof(TGE_Vec2i));
        if (new_body) {
            world->body = new_body;
            world->body_capacity = capacity;
        }
    }
    /* A shrinking terminal could leave the snake longer than the new area. */
    if (world->body_length > world->body_capacity)
        world->body_length = world->body_capacity;

    switch (view_update) {
    case TGE_VIEW_FIRST_VALID:
        /* First time the terminal is big enough: spawn a fresh game. */
        world_reset(world);
        break;
    case TGE_VIEW_RESIZED:
        /* Resize while playing: clamp the snake into the new bounds... */
        for (int i = 0; i < world->body_length; i++)
            world->body[i] =
                tge_vec2i_clamp_rect(world->body[i], world_bounds(world));
        /* ...and respawn the food if it ended up outside the playfield. */
        if (world->food.x >= world->view.area.w ||
            world->food.y >= world->view.area.h) {
            if (!world_spawn_food(world))
                world->state = SNAKE_OVER;
        }
        break;
    case TGE_VIEW_INVALID:
    default:
        /* Too small: keep the state frozen until the terminal grows. */
        break;
    }
}

static void world_update(SnakeWorld *world, float delta_time)
{
    /* Frozen while over, paused, or waiting for a valid terminal size. */
    if (world->state != SNAKE_RUNNING || world->paused || !world->view.valid)
        return;
    /* Feed the fixed-step timer with the real elapsed time... */
    tge_fixedstep_update(&world->step, delta_time);
    /* ...and run one world_step per accumulated interval, so the snake moves
     * at a constant speed regardless of the frame rate. */
    while (tge_fixedstep_next(&world->step)) {
        if (!world_step(world)) {
            world->state = SNAKE_OVER;
            break;
        }
    }
}

/* A turn input is not applied immediately: it is queued so fast key presses
 * survive between fixed steps (the input buffer holds up to
 * DIRECTION_QUEUE_SIZE of them). Returns true when the event was a turn. */
static bool world_handle_input(SnakeWorld *world, TGE_Event *event)
{
    TGE_Direction direction = tge_input_direction(event);
    if (direction == TGE_DIR_NONE)
        return false;
    tge_input_buffer_push(&world->input, direction);
    return true;
}

/* ── renderer: the screen ──────────────────────────────────────────────── */

static const TGE_Sprite SPRITE_EMPTY = TGE_SPRITE(2, 1, "  ", NULL);
static const TGE_Sprite SPRITE_BODY =
    TGE_SPRITE(2, 1, "\xE2\x96\x93\xE2\x96\x93", "..");
static const TGE_Sprite SPRITE_WALL =
    TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
static const TGE_Sprite SPRITE_HEAD =
    TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
static const TGE_Sprite SPRITE_FOOD =
    TGE_SPRITE(2, 1, "\xE2\x96\x93\xE2\x96\x93", "@@");
static const TGE_Sprite SPRITE_SELECT = TGE_SPRITE(2, 1, "::", NULL);

static const TGE_GridTheme SNAKE_THEME = {
    .empty = &SPRITE_EMPTY,
    .default_sprite = &SPRITE_BODY,
    .border = &SPRITE_WALL,
    .selection = &SPRITE_SELECT,
};

/* Re-create the grid view on the canvas being drawn: cell size 2x1 makes
 * each logical cell a square 2-char block, starting one row down (the top row
 * is the score HUD). The theme, cell size and origin are configured once
 * here and re-applied each frame against the current canvas. */
static void renderer_begin(SnakeRenderer *renderer, TGE_Canvas *canvas)
{
    tge_grid_view_init(&renderer->view, canvas, renderer->theme,
                       TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&renderer->view.grid, 0, 1);
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
}

static void draw_hud(SnakeRenderer *renderer, TGE_Canvas *canvas,
                     const SnakeWorld *world)
{
    (void)renderer;
    /* Score on the row above the playfield. */
    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK, " SCORE: %d ",
               world->score);
}

static void draw_playfield(SnakeRenderer *renderer, TGE_Canvas *canvas,
                           const SnakeWorld *world)
{
    /* Border around the playfield. */
    tge_grid_view_draw_border(&renderer->view, TGE_COLOR_CYAN,
                              TGE_COLOR_BLACK);
    if (!world->view.valid) {
        /* Terminal below the minimum: tell the player to grow it. */
        tge_draw_centered_text(canvas, tge_canvas_height(canvas) / 2,
                               " too small ", TGE_COLOR_RED, TGE_COLOR_BLACK);
    }
}

static void draw_snake(SnakeRenderer *renderer, const SnakeWorld *world)
{
    /* The world stores logical coordinates; translate each body cell to its
     * grid position (offset by world_bounds) before drawing. */
    for (int i = 1; i < world->body_length; i++) {
        TGE_Vec2i grid_point =
            tge_rect_translate_point(world->view.area, world->body[i]);
        tge_grid_view_set_cell(&renderer->view, grid_point.x, grid_point.y,
                               TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    }
    /* The head is a solid block so the player can tell it apart. */
    TGE_Vec2i head_point =
        tge_rect_translate_point(world->view.area, world->body[0]);
    tge_grid_view_put(&renderer->view, head_point.x, head_point.y,
                      &SPRITE_HEAD, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
}

static void draw_food(SnakeRenderer *renderer, const SnakeWorld *world)
{
    /* The food uses bold to stand out from the body. */
    TGE_Vec2i food_point =
        tge_rect_translate_point(world->view.area, world->food);
    tge_grid_view_put_attr(&renderer->view, food_point.x, food_point.y,
                           &SPRITE_FOOD, TGE_COLOR_RED, TGE_COLOR_BLACK,
                           TGE_CELL_ATTR_BOLD);
}

static void draw_overlay(TGE_Canvas *canvas, const SnakeWorld *world)
{
    /* GAME OVER has priority over the pause banner. */
    int canvas_width = tge_canvas_width(canvas);
    int canvas_height = tge_canvas_height(canvas);
    if (world->state == SNAKE_OVER) {
        const char *message = " GAME OVER ";
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_fill_rect(canvas, 1, canvas_height / 2 - 1, canvas_width - 2, 3,
                      ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, canvas_height / 2 - 1, message,
                               TGE_COLOR_RED, TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, canvas_height / 2 + 1, again,
                               TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    } else if (world->paused) {
        const char *again = " [P] resume ";
        tge_fill_rect(canvas, 1, canvas_height / 2 - 1, canvas_width - 2, 3,
                      ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, canvas_height / 2 - 1, " PAUSED ",
                               TGE_COLOR_YELLOW, TGE_COLOR_BLACK);
        tge_draw_centered_text(canvas, canvas_height / 2 + 1, again,
                               TGE_COLOR_WHITE, TGE_COLOR_BLACK);
    }
}

/* ── title scene ───────────────────────────────────────────────────────── */

static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    (void)scene;
    int canvas_width = tge_canvas_width(canvas);
    int canvas_height = tge_canvas_height(canvas);
    const char *title = " SNAKE 2X1 ";
    const char *subtitle = " square pixels, grid adapts to terminal ";
    const char *controls = " Arrows/WASD move  [P] pause ";
    const char *start = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, canvas_width, canvas_height      , TGE_COLOR_CYAN  , TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, canvas_height / 2 - 4, title   , TGE_COLOR_GREEN , TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, canvas_height / 2 - 2, subtitle, TGE_COLOR_CYAN  , TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, canvas_height / 2 + 1, controls, TGE_COLOR_WHITE , TGE_COLOR_BLACK);
    tge_draw_centered_text(canvas, canvas_height / 2 + 3, start   , TGE_COLOR_YELLOW, TGE_COLOR_BLACK);

    /* Decorative mini-snake under the text: a head, two body cells and a
     * food, drawn through the same grid theme. */
    TGE_GridView grid_view;
    tge_grid_view_init(&grid_view, canvas, &SNAKE_THEME, TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&grid_view.grid, 0, 15);
    tge_grid_view_put(&grid_view, 6, 0, &SPRITE_HEAD, TGE_COLOR_GREEN,
                      TGE_COLOR_BLACK);
    for (int i = 0; i < 3; i++)
        tge_grid_view_set_cell(&grid_view, 7 + i, 0, TGE_COLOR_GREEN,
                               TGE_COLOR_BLACK);
    tge_grid_view_put(&grid_view, 12, 0, &SPRITE_FOOD, TGE_COLOR_RED,
                      TGE_COLOR_BLACK);
}

static void title_event(TGE_Scene *scene, TGE_Event *event)
{
    (void)scene;
    if (tge_input_cancel(event) || tge_input_quit(event)) {
        /* ESC or Q quits from the title. */
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(event)) {
        /* ENTER creates the game scene (heap-allocated, owned by the app once
         * pushed) on top of the title. */
        TGE_Scene *game_scene = NULL;
        SnakeGame *game = (SnakeGame *)tge_scene_create(
            &game_scene, sizeof(SnakeGame), game_update, game_draw,
            game_event, game_destroy);
        game->renderer.theme = &SNAKE_THEME;
        /* Configure the renderer geometry once; renderer_begin() re-applies
         * it every frame against the current canvas. */
        tge_grid_view_init(&game->renderer.view, NULL, &SNAKE_THEME,
                           TGE_GRID_SCALE_2X1);
        tge_grid_set_origin(&game->renderer.view.grid, 0, 1);
        world_init(&game->world);
        TGE_PushScene(g_app, game_scene);
    }
}
