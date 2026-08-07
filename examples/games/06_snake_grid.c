#include "tge/tge.h"

#include "tge-extra/array.h"
#include "tge-extra/direction.h"
#include "tge-extra/fixedstep.h"
#include "tge-extra/game.h"
#include "tge-extra/grid.h"
#include "tge-extra/grid_view.h"
#include "tge-extra/input.h"
#include "tge-extra/input_buffer.h"
#include "tge-extra/ui.h"
#include "tge-extra/vec2i.h"
#include "tge-extra/view.h"

#include <stdlib.h>

#define MIN_PLAYFIELD_WIDTH 10
#define MIN_PLAYFIELD_HEIGHT 6
#define MOVE_INTERVAL 0.10f
#define DIRECTION_QUEUE_SIZE 4

typedef enum { SNAKE_RUNNING = 0, SNAKE_OVER } SnakeState;

typedef struct {
    int score;

    TGE_Vec2i *body;
    int body_capacity;
    int body_length;

    TGE_Direction direction;
    TGE_InputBuffer input;
    TGE_Vec2i food_position;

    SnakeState state;
    bool paused;
    TGE_FixedStep step;
    TGE_View view;
} SnakeWorld;

typedef struct {
    TGE_GridView view;
    TGE_GridLayout layout;
} SnakeRenderer;

typedef struct {
    TGE_GameContext ctx;
    SnakeWorld world;
    SnakeRenderer renderer;
} SnakeGame;

static TGE_App *g_app = NULL;

static void game_world_resize(void *userdata, int grid_width, int grid_height);
static void game_event  (TGE_GameContext *ctx, TGE_Event *event);
static void game_update (TGE_GameContext *ctx, float delta_time);
static void game_draw   (TGE_GameContext *ctx, TGE_Canvas *canvas);
static void game_destroy(TGE_GameContext *ctx);

static bool world_handle_input(SnakeWorld *world, TGE_Event *event);
static void world_update      (SnakeWorld *world, float delta_time);
static bool world_step        (SnakeWorld *world);
static void world_init        (SnakeWorld *world);
static void world_resize      (SnakeWorld *world, int grid_width, int grid_height);
static void world_reset       (SnakeWorld *world);

static void renderer_draw          (SnakeRenderer *renderer, TGE_Canvas *canvas, const SnakeWorld *world);
static void renderer_begin         (SnakeRenderer *renderer, TGE_Canvas *canvas);
static void renderer_draw_playfield(SnakeRenderer *renderer, TGE_Canvas *canvas, const SnakeWorld *world);
static void renderer_draw_snake    (SnakeRenderer *renderer, const SnakeWorld *world);
static void renderer_draw_overlay  (TGE_Canvas    *canvas  , const SnakeWorld *world);

static void init_app(TGE_App *app);
static void title_draw(TGE_Scene *scene, TGE_Canvas *canvas);
static void title_event(TGE_Scene *scene, TGE_Event *event);

static const TGE_Sprite SPRITE_EMPTY  = TGE_SPRITE(2, 1, "  "                      , NULL);
static const TGE_Sprite SPRITE_BODY   = TGE_SPRITE(2, 1, "\xE2\x96\x93\xE2\x96\x93", "..");
static const TGE_Sprite SPRITE_WALL   = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
static const TGE_Sprite SPRITE_HEAD   = TGE_SPRITE(2, 1, "\xE2\x96\x88\xE2\x96\x88", "##");
static const TGE_Sprite SPRITE_FOOD   = TGE_SPRITE(2, 1, "\xE2\x96\x93\xE2\x96\x93", "@@");

static const TGE_GridTheme SNAKE_THEME = {
    .empty          = &SPRITE_EMPTY,
    .default_sprite = &SPRITE_BODY,
    .border         = &SPRITE_WALL,
};

static const TGE_GameCallbacks snake_callbacks = {
    .update  = game_update,
    .draw    = game_draw,
    .event   = game_event,
    .destroy = game_destroy,
};

int main(void)
{
    TGE_App *app = TGE_Create(
        2 * (MIN_PLAYFIELD_WIDTH + 2),
        MIN_PLAYFIELD_HEIGHT + 3,
        "TGE Snake 2x1"
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

    const char *title    = " SNAKE 2X1 ";
    const char *subtitle = " square pixels, grid adapts to terminal ";
    const char *controls = " Arrows/WASD move  [P] pause ";
    const char *start    = " [ENTER] start  [ESC]/[Q] quit ";

    tge_draw_frame(canvas, 0, 0, canvas_width, canvas_height      , TGE_COLOR_CYAN  , TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 - 4, title   , TGE_COLOR_GREEN , TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 - 2, subtitle, TGE_COLOR_CYAN  , TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 + 1, controls, TGE_COLOR_WHITE , TGE_COLOR_DEFAULT);
    tge_draw_centered_text(canvas, canvas_height / 2 + 3, start   , TGE_COLOR_YELLOW, TGE_COLOR_DEFAULT);

    TGE_GridView grid_view;
    tge_grid_view_init(&grid_view, &SNAKE_THEME, TGE_GRID_SCALE_2X1);
    tge_grid_view_attach(&grid_view, canvas);
    tge_grid_set_origin(&grid_view.grid, 0, 15);
    tge_grid_view_put(&grid_view, 6, 0, &SPRITE_HEAD, TGE_COLOR_GREEN,
                      TGE_COLOR_DEFAULT);
    for (int i = 0; i < 3; i++)
        tge_grid_view_set_cell(&grid_view, 7 + i, 0, TGE_COLOR_GREEN,
                               TGE_COLOR_DEFAULT);
    tge_grid_view_put(&grid_view, 12, 0, &SPRITE_FOOD, TGE_COLOR_RED,
                      TGE_COLOR_DEFAULT);
}

static void title_event(TGE_Scene *scene, TGE_Event *event)
{
    (void)scene;
    if (tge_input_cancel(event) || tge_input_quit(event)) {
        TGE_Quit(g_app);
        return;
    }
    if (tge_input_confirm(event)) {
        SnakeGame *game = (SnakeGame *)tge_game_create(g_app, sizeof(SnakeGame),
                                                       &snake_callbacks);
        if (!game)
            return;
        tge_grid_view_init(&game->renderer.view, &SNAKE_THEME,
                           TGE_GRID_SCALE_2X1);
        tge_grid_set_origin(&game->renderer.view.grid, 0, 1);
        tge_grid_layout_init(&game->renderer.layout, &game->renderer.view);
        world_init(&game->world);
    }
}

static void game_draw(TGE_GameContext *ctx, TGE_Canvas *canvas)
{
    SnakeGame *game = (SnakeGame *)tge_game_instance(ctx);
    tge_grid_layout_sync(&game->renderer.layout, tge_canvas_width(canvas),
                         tge_canvas_height(canvas), game_world_resize,
                         &game->world);
    renderer_draw(&game->renderer, canvas, &game->world);
}

static void game_update(TGE_GameContext *ctx, float delta_time)
{
    SnakeGame *game = (SnakeGame *)tge_game_instance(ctx);
    world_update(&game->world, delta_time);
}

static void game_event(TGE_GameContext *ctx, TGE_Event *event)
{
    SnakeGame *game = (SnakeGame *)tge_game_instance(ctx);

    if (event->type == TGE_EVENT_RESIZE) {
        tge_grid_layout_sync(&game->renderer.layout, event->data.resize.w,
                             event->data.resize.h, game_world_resize,
                             &game->world);
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
    if (world_handle_input(&game->world, event))
        return;
    if (tge_input_quit(event)) {
        if (game->world.state == SNAKE_OVER)
            TGE_Quit(ctx->app);
        return;
    }
    if (tge_input_confirm(event)) {
        if (game->world.state == SNAKE_OVER && game->world.view.valid)
            world_reset(&game->world);
        return;
    }
    if (tge_input_cancel(event))
        TGE_PopScene(ctx->app);
}

static void game_destroy(TGE_GameContext *ctx)
{
    SnakeGame *game = (SnakeGame *)tge_game_instance(ctx);
    free(game->world.body);
}

static void game_world_resize(void *userdata, int grid_width, int grid_height)
{
    world_resize((SnakeWorld *)userdata, grid_width, grid_height);
}

static void renderer_draw(
    SnakeRenderer *renderer,
    TGE_Canvas *canvas,
    const SnakeWorld *world
) {
    renderer_begin(renderer, canvas);
    tge_printf(
        canvas,
        1, 0,
        TGE_COLOR_YELLOW,
        TGE_COLOR_DEFAULT,
        " SCORE: %d ", world->score
    );

    renderer_draw_playfield(renderer, canvas, world);
    renderer_draw_snake(renderer, world);
    tge_grid_view_put_attr_local(&renderer->view, &world->view, world->food_position,
                                 &SPRITE_FOOD, TGE_COLOR_RED, TGE_COLOR_DEFAULT,
                                 TGE_CELL_ATTR_BOLD);
    renderer_draw_overlay(canvas, world);
}

static void renderer_begin(SnakeRenderer *renderer, TGE_Canvas *canvas)
{
    tge_grid_view_attach(&renderer->view, canvas);
    tge_clear(canvas, ' ', TGE_COLOR_BLACK, TGE_COLOR_DEFAULT);
}

static void renderer_draw_playfield(SnakeRenderer *renderer, TGE_Canvas *canvas,
                                    const SnakeWorld *world)
{
    tge_grid_view_draw_border(&renderer->view, TGE_COLOR_CYAN,
                              TGE_COLOR_DEFAULT);
    if (!world->view.valid) {
        tge_draw_centered_text(canvas, tge_canvas_height(canvas) / 2,
                               " too small ", TGE_COLOR_RED, TGE_COLOR_DEFAULT);
    }
}

static void renderer_draw_snake(SnakeRenderer *renderer,
                                const SnakeWorld *world)
{
    for (int i = 1; i < world->body_length; i++)
        tge_grid_view_set_cell_local(
            &renderer->view,
            &world->view,
            world->body[i],
            TGE_COLOR_GREEN, TGE_COLOR_DEFAULT
        );
    tge_grid_view_put_local(
        &renderer->view,
        &world->view,
        world->body[0],
        &SPRITE_HEAD,
        TGE_COLOR_GREEN, TGE_COLOR_DEFAULT
        );
}

static void renderer_draw_overlay(TGE_Canvas *canvas, const SnakeWorld *world)
{
    if (world->state == SNAKE_OVER) {
        const char *again = " [ENTER] restart  [ESC] menu  [Q] quit ";
        tge_draw_modal(canvas, " GAME OVER ", again, TGE_COLOR_RED);
    } else if (world->paused) {
        const char *again = " [P] resume ";
        tge_draw_modal(canvas, " PAUSED ", again, TGE_COLOR_YELLOW);
    }
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

static bool world_spawn_food(SnakeWorld *world)
{
    if (world->view.area.w * world->view.area.h - world->body_length <= 0)
        return false;
    for (;;) {
        TGE_Vec2i candidate = tge_view_random_point(&world->view);
        bool free_spot = true;
        for (int i = 0; i < world->body_length; i++) {
            if (tge_vec2i_eq(world->body[i], candidate)) {
                free_spot = false;
                break;
            }
        }
        if (free_spot) {
            world->food_position = candidate;
            return true;
        }
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

    if (!tge_view_contains(&world->view, next_head))
        return false;

    bool ate = tge_vec2i_eq(next_head, world->food_position);
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

static bool world_handle_input(SnakeWorld *world, TGE_Event *event)
{
    TGE_Direction direction = tge_input_direction(event);
    if (direction == TGE_DIR_NONE)
        return false;
    tge_input_buffer_push(&world->input, direction);
    return true;
}

static void world_init(SnakeWorld *world)
{
    tge_view_init(&world->view, MIN_PLAYFIELD_WIDTH, MIN_PLAYFIELD_HEIGHT);
    tge_input_buffer_init(&world->input, DIRECTION_QUEUE_SIZE);
}

static void world_resize(SnakeWorld *world, int grid_width, int grid_height)
{
    TGE_ViewUpdate view_update = tge_view_update(&world->view, grid_width,
                                                 grid_height);

    int capacity = world->view.area.w * world->view.area.h;
    tge_array_resize((void **)&world->body, &world->body_capacity, capacity,
                     sizeof(TGE_Vec2i));
    if (world->body_length > world->body_capacity)
        world->body_length = world->body_capacity;

    switch (view_update) {
    case TGE_VIEW_FIRST_VALID:
        world_reset(world);
        break;
    case TGE_VIEW_RESIZED:
        for (int i = 0; i < world->body_length; i++)
            world->body[i] = tge_vec2i_clamp_rect(
                world->body[i], tge_view_local_bounds(&world->view));
        if (!tge_view_contains(&world->view, world->food_position)) {
            if (!world_spawn_food(world))
                world->state = SNAKE_OVER;
        }
        break;
    case TGE_VIEW_INVALID:
    default:
        break;
    }
}

static void world_reset(SnakeWorld *world)
{
    int center_x = world->view.area.w / 2;
    int center_y = world->view.area.h / 2;
    world->body_length = 3;
    world->body[0] = tge_vec2i(center_x    , center_y);
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
