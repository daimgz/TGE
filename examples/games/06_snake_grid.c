/* 06_snake_grid - Snake with square pixels, a grid theme and the standard
 * TGE layering (world / renderer).
 *
 * Terminal cells are not square (~1:2), so the plain 01_snake looks
 * stretched. This version renders the whole playfield through a TGE_GridView
 * at cell size 2x1 (square pixels): every logical cell becomes a 2-character
 * block, so cells look square and horizontal motion matches vertical.
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
 *                  owns one SnakeWorld + one SnakeRenderer and moves turns
 *                  into the input buffer, resizes into world_layout and draws
 *                  through the renderer.
 *
 * The playfield (logical LX x LY in grid cells) comes from the terminal size
 * through TGE_View: on a resize the interior (view.area) is recomputed and
 * the snake is clamped into the new bounds; the first VALID layout spawns a
 * fresh snake, so a too-small start simply waits until the terminal grows.
 * tge_view_update() returns what the new layout means
 * (TGE_VIEW_INVALID/RESIZED/FIRST_VALID), so the world reacts without
 * tracking its own "laid out" flag. TGE_View and the grid draw everything in
 * logical coordinates, so the same game logic runs at any terminal
 * resolution.
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

/* ------------------------------------------------------------------ world */

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

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

static void world_init(SnakeWorld *world)
{
    tge_view_init(&world->view, MIN_PLAYFIELD_WIDTH, MIN_PLAYFIELD_HEIGHT);
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
    if (world->view.area.w * world->view.area.h - world->body_length <= 0)
        return false;
    for (;;) {
        TGE_Vec2i candidate = tge_rect_random_point(world_bounds(world));
        bool free_spot = true;
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
    int center_x = world->view.area.w / 2;
    int center_y = world->view.area.h / 2;
    world->body_length = 3;
    world->body[0] = tge_vec2i(center_x, center_y);
    world->body[1] = tge_vec2i(center_x - 1, center_y);
    world->body[2] = tge_vec2i(center_x - 2, center_y);
    world->direction = TGE_DIR_RIGHT;
    world->score = 0;
    world->state = SNAKE_RUNNING;
    world->paused = false;
    tge_fixedstep_init(&world->step, MOVE_INTERVAL);
    tge_input_buffer_clear(&world->input);
    world_spawn_food(world);
}

/* Recompute the playfield layout for a new logical grid size (grid_width x
 * grid_height grid cells, the renderer decides how a canvas maps to them).
 * The view decides what the layout means: the first valid one spawns a fresh
 * snake, later resizes keep the current one (clamped into the new bounds,
 * food respawned if it no longer fits), and a too-small size stays inactive
 * until the terminal grows. */
static void world_layout(SnakeWorld *world, int grid_width, int grid_height)
{
    world->last_grid_width = grid_width;
    world->last_grid_height = grid_height;
    TGE_ViewUpdate view_update = tge_view_update(&world->view, grid_width,
                                                 grid_height);

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
    if (world->body_length > world->body_capacity)
        world->body_length = world->body_capacity;

    switch (view_update) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(world);
        break;
    case TGE_VIEW_RESIZED:
        for (int i = 0; i < world->body_length; i++)
            world->body[i] =
                tge_vec2i_clamp_rect(world->body[i], world_bounds(world));
        if (world->food.x >= world->view.area.w ||
            world->food.y >= world->view.area.h) {
            if (!world_spawn_food(world))
                world->state = SNAKE_OVER;
        }
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static bool world_step(SnakeWorld *world)
{
    TGE_Direction direction;
    while (tge_input_buffer_pop(&world->input, &direction)) {
        if (direction == tge_direction_opposite(world->direction) ||
            direction == world->direction)
            continue;
        world->direction = direction;
        break;
    }

    TGE_Vec2i next_head =
        tge_vec2i_add(world->body[0], tge_direction_vec(world->direction));

    if (next_head.x < 0 || next_head.x >= world->view.area.w ||
        next_head.y < 0 || next_head.y >= world->view.area.h)
        return false;

    bool ate = tge_vec2i_eq(next_head, world->food);
    int cells_to_check = world->body_length - (ate ? 0 : 1);
    for (int i = 0; i < cells_to_check; i++) {
        if (tge_vec2i_eq(next_head, world->body[i]))
            return false;
    }

    for (int i = world->body_length; i > 0; i--)
        world->body[i] = world->body[i - 1];
    world->body[0] = next_head;

    if (ate) {
        world->body_length++;
        world->score += 10;
        return world_spawn_food(world);
    }
    return true;
}

static void world_update(SnakeWorld *world, float delta_time)
{
    if (world->state != SNAKE_RUNNING || world->paused || !world->view.valid)
        return;
    tge_fixedstep_update(&world->step, delta_time);
    while (tge_fixedstep_next(&world->step)) {
        if (!world_step(world)) {
            world->state = SNAKE_OVER;
            break;
        }
    }
}

/* --------------------------------------------------------------- renderer */

typedef struct {
    const TGE_GridTheme *theme;
    TGE_GridView view;
} SnakeRenderer;

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

/* Sync the renderer with the canvas currently being drawn. The app swaps its
 * double buffers every frame, so the view is re-attached to the current
 * canvas on every draw; the theme, cell size and origin are configured once
 * at creation and re-applied here. */
static void renderer_bind(SnakeRenderer *renderer, TGE_Canvas *canvas)
{
    tge_grid_view_init(&renderer->view, canvas, renderer->theme,
                       TGE_GRID_SCALE_2X1);
    tge_grid_set_origin(&renderer->view.grid, 0, 1);
}

static void renderer_draw(SnakeRenderer *renderer, TGE_Canvas *canvas,
                          const SnakeWorld *world)
{
    int canvas_width = tge_canvas_width(canvas);
    int canvas_height = tge_canvas_height(canvas);
    renderer_bind(renderer, canvas);

    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_BLACK);

    tge_printf(canvas, 1, 0, TGE_COLOR_YELLOW, TGE_COLOR_BLACK, " SCORE: %d ",
               world->score);

    tge_grid_view_draw_border(&renderer->view, TGE_COLOR_CYAN,
                              TGE_COLOR_BLACK);

    if (!world->view.valid) {
        tge_draw_centered_text(canvas, canvas_height / 2, " too small ",
                               TGE_COLOR_RED, TGE_COLOR_BLACK);
        return;
    }

    for (int i = 1; i < world->body_length; i++) {
        TGE_Vec2i grid_point =
            tge_rect_translate_point(world->view.area, world->body[i]);
        tge_grid_view_set_cell(&renderer->view, grid_point.x, grid_point.y,
                               TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    }
    TGE_Vec2i head_point =
        tge_rect_translate_point(world->view.area, world->body[0]);
    tge_grid_view_put(&renderer->view, head_point.x, head_point.y,
                      &SPRITE_HEAD, TGE_COLOR_GREEN, TGE_COLOR_BLACK);
    TGE_Vec2i food_point =
        tge_rect_translate_point(world->view.area, world->food);
    tge_grid_view_put_attr(&renderer->view, food_point.x, food_point.y,
                           &SPRITE_FOOD, TGE_COLOR_RED, TGE_COLOR_BLACK,
                           TGE_CELL_ATTR_BOLD);

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

/* ----------------------------------------------------------------- scenes */

/* The game as a whole: the world (rules) + the renderer (screen), wired
 * together by the scene callbacks. */
typedef struct {
    SnakeWorld world;
    SnakeRenderer renderer;
} SnakeGame;

static TGE_App *g_app = NULL;

static void game_update(TGE_Scene *scene, float delta_time)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;
    world_update(&game->world, delta_time);
}

static void game_draw(TGE_Scene *scene, TGE_Canvas *canvas)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;
    int canvas_width = tge_canvas_width(canvas);
    int canvas_height = tge_canvas_height(canvas);
    int grid_width, grid_height;
    tge_grid_view_size_for(&game->renderer.view, canvas_width, canvas_height,
                           &grid_width, &grid_height);
    if (game->world.last_grid_width != grid_width ||
        game->world.last_grid_height != grid_height)
        world_layout(&game->world, grid_width, grid_height);
    renderer_draw(&game->renderer, canvas, &game->world);
}

static void game_event(TGE_Scene *scene, TGE_Event *event)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;

    if (event->type == TGE_EVENT_RESIZE) {
        int grid_width, grid_height;
        tge_grid_view_size_for(&game->renderer.view, event->data.resize.w,
                               event->data.resize.h, &grid_width,
                               &grid_height);
        world_layout(&game->world, grid_width, grid_height);
        if (game->world.state != SNAKE_OVER)
            game->world.paused = true;
        return;
    }
    if (tge_input_pause(event)) {
        if (game->world.state != SNAKE_OVER)
            game->world.paused = !game->world.paused;
        return;
    }
    if (game->world.paused && !tge_input_cancel(event))
        return;
    TGE_Direction direction = tge_input_direction(event);
    if (direction != TGE_DIR_NONE) {
        tge_input_buffer_push(&game->world.input, direction);
        return;
    }
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
    if (tge_input_cancel(event)) {
        TGE_PopScene(g_app);
    }
}

static void game_destroy(TGE_Scene *scene)
{
    SnakeGame *game = (SnakeGame *)scene->userdata;
    free(game->world.body);
}

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
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(event)) {
        TGE_Scene *game_scene = NULL;
        SnakeGame *game = (SnakeGame *)tge_scene_create(
            &game_scene, sizeof(SnakeGame), game_update, game_draw,
            game_event, game_destroy);
        game->renderer.theme = &SNAKE_THEME;
        tge_grid_view_init(&game->renderer.view, NULL, &SNAKE_THEME,
                           TGE_GRID_SCALE_2X1);
        tge_grid_set_origin(&game->renderer.view.grid, 0, 1);
        world_init(&game->world);
        TGE_PushScene(g_app, game_scene);
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
