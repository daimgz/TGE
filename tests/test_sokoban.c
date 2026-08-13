#define TGE_SOKOBAN_TEST
#include "../examples/games/12_sokoban.c"

#include "tge-extra/tilemap.h"
#include "tge-extra/vec2i.h"
#include "tge_test.h"

#include <string.h>

static void make_test_world(SokoWorld *world)
{
    world_init(world);
    world_reset(world); /* loads level 0 (valid) */
}

TGE_TEST(loads_level_dimensions)
{
    SokoWorld world;
    make_test_world(&world);
    TGE_ASSERT(world.map.width == SOKO_W && world.map.height == SOKO_H,
               "map size");
    TGE_ASSERT(world.state == SOKO_PLAYING, "starts in PLAYING");
    TGE_ASSERT(world.moves == 0, "no moves yet");
}

TGE_TEST(all_rows_have_correct_width)
{
    for (int i = 0; i < LEVEL_COUNT; i++) {
        for (int y = 0; y < LEVELS[i].h; y++)
            TGE_ASSERT((int)strlen(LEVELS[i].rows[y]) == LEVELS[i].w,
                       "every level row is exactly SOKO_W wide");
    }
}

TGE_TEST(player_recorded_from_marker)
{
    SokoWorld world;
    make_test_world(&world);
    /* level 0: '@' at (1, 2) */
    TGE_ASSERT(tge_vec2i_eq(world.player, tge_vec2i(1, 2)),
               "player spawn parsed from '@'");
    TGE_ASSERT(tge_tilemap_get(&world.map, 1, 2) == SOKO_FLOOR,
               "player cell keeps its terrain (overlay, not a map role)");
}

TGE_TEST(box_and_player_on_goal_markers)
{
    /* exercises '*' (box on goal) and '+' (player on goal). Boxes == goals:
     * the '+' is an extra goal that must be balanced by an extra box. */
    static const char *const rows[3] = {
        "#####",
        "#+$*#",
        "#####",
    };
    SokoLevelDef def = { 5, 3, rows };
    SokoWorld world;
    world_init(&world);
    TGE_ASSERT(sokoban_load_def(&world, &def), "custom level with * and + loads");
    TGE_ASSERT(tge_tilemap_count(&world.map, SOKO_BOX_ON_GOAL) == 1,
               "box on goal present");
    TGE_ASSERT(tge_tilemap_get(&world.map, 1, 1) == SOKO_GOAL,
               "'+' cell is a goal (player stands on it)");
    TGE_ASSERT(tge_vec2i_eq(world.player, tge_vec2i(1, 1)),
               "player parsed from '+'");
}

TGE_TEST(push_box_onto_goal_wins)
{
    SokoWorld world;
    make_test_world(&world);
    /* level 0: player (1,2), box (2,2), goal (4,2) */
    world_move(&world, TGE_DIR_RIGHT); /* box -> (3,2) */
    TGE_ASSERT(tge_tilemap_get(&world.map, 3, 2) == SOKO_BOX,
               "box pushed one cell right");
    TGE_ASSERT(tge_vec2i_eq(world.player, tge_vec2i(2, 2)),
               "player follows the box");
    world_move(&world, TGE_DIR_RIGHT); /* box -> (4,2) goal */
    TGE_ASSERT(world.state == SOKO_WON, "winning when the last box is on a goal");
    TGE_ASSERT(tge_tilemap_count(&world.map, SOKO_BOX) == 0,
               "no box left off a goal");
}

TGE_TEST(push_against_wall_blocked)
{
    SokoWorld world;
    make_test_world(&world);
    world_move(&world, TGE_DIR_LEFT); /* (0,2) is a wall */
    TGE_ASSERT(tge_vec2i_eq(world.player, tge_vec2i(1, 2)),
               "wall blocks the player move");
    TGE_ASSERT(world.moves == 0, "blocked move is not counted");
}

TGE_TEST(undo_restores_state)
{
    SokoWorld world;
    make_test_world(&world);
    TGE_Vec2i start = world.player;
    world_move(&world, TGE_DIR_RIGHT); /* push the box */
    int moves = world.moves;
    world_undo(&world);
    TGE_ASSERT(tge_vec2i_eq(world.player, start), "player returns to start");
    TGE_ASSERT(tge_tilemap_get(&world.map, 2, 2) == SOKO_BOX,
               "box back at its origin");
    TGE_ASSERT(tge_tilemap_get(&world.map, 3, 2) == SOKO_FLOOR,
               "destination cell cleared");
    TGE_ASSERT(world.moves == moves - 1, "move counter decremented");
}

TGE_TEST(reject_unequal_boxes_goals)
{
    /* 1 box but 2 goals: load must fail (unsolvable) */
    static const char *const rows[3] = {
        "######",
        "#.$ .#",
        "######",
    };
    SokoLevelDef def = { 6, 3, rows };
    SokoWorld world;
    world_init(&world);
    TGE_ASSERT(!sokoban_load_def(&world, &def),
               "level with unequal boxes/goals is rejected");
}

int main(void)
{
    test_loads_level_dimensions();
    test_all_rows_have_correct_width();
    test_player_recorded_from_marker();
    test_box_and_player_on_goal_markers();
    test_push_box_onto_goal_wins();
    test_push_against_wall_blocked();
    test_undo_restores_state();
    test_reject_unequal_boxes_goals();
    return tge_test_report();
}
