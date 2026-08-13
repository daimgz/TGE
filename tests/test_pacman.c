#define TGE_PACMAN_TEST
#include "../examples/games/11_pacman.c"

#include "tge/tge_canvas.h"
#include "tge-extra/tilemap.h"
#include "tge_test.h"

#include <string.h>

static void make_test_world(PacmanWorld *world)
{
    world_init(world);
    world_reset(world);
    world->playable = true; /* world_reset no longer touches playable */
}

TGE_TEST(maze_loads_correct_dimensions)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_ASSERT(world.map.width == MAZE_W && world.map.height == MAZE_H,
               "map size");
    TGE_ASSERT(world.level.pellets_left > 0, "has pellets");
}

TGE_TEST(level_markers_parsed_from_maze)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_ASSERT(tge_vec2i_eq(world.level.pac_spawn, tge_vec2i(13, 23)),
                "P marker is the pacman spawn");
    TGE_ASSERT(tge_vec2i_eq(world.level.ghost_home, tge_vec2i(14, 13)),
                "H marker is the ghost home");
    /* 1=Blinky (outside, above the door), 2=Pinky, 3=Inky, 4=Clyde. */
    static const TGE_Vec2i GHOST_POS[GHOST_COUNT] = {
        { 13, 11 }, { 13, 13 }, { 11, 13 }, { 16, 13 },
    };
    for (int i = 0; i < GHOST_COUNT; i++)
        TGE_ASSERT(tge_vec2i_eq(world.level.ghost_spawns[i], GHOST_POS[i]),
                    "1-4 markers are the ghost spawns");
    /* A=TR, B=TL, C=BR, D=BL (one per ghost index). */
    static const TGE_Vec2i SCATTER[GHOST_COUNT] = {
        { MAZE_W - 1, 0 }, { 0, 0 },
        { MAZE_W - 1, MAZE_H - 1 }, { 0, MAZE_H - 1 },
    };
    for (int i = 0; i < GHOST_COUNT; i++)
        TGE_ASSERT(tge_vec2i_eq(world.level.scatter_targets[i], SCATTER[i]),
                    "A-D markers are the frame corners");
    int expected = 0;
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++)
            if (MAZE[y][x] == '.' || MAZE[y][x] == 'o')
                expected++;
    TGE_ASSERT(world.level.pellets_left == expected,
                "pellets_left matches the maze");
}

TGE_TEST(maze_roles_at_key_positions)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_ASSERT(tge_tilemap_get(&world.map, 0, 0) == ROLE_WALL,
               "corner wall (scatter marker keeps the wall)");
    TGE_ASSERT(tge_tilemap_get(&world.map, 10, 0) == ROLE_WALL,
               "top middle wall");
    TGE_ASSERT(tge_tilemap_get(&world.map, 1, 3) == ROLE_POWER,
               "power pellet top-left");
    TGE_ASSERT(tge_tilemap_get(&world.map, 26, 3) == ROLE_POWER,
               "power pellet top-right");
    TGE_ASSERT(tge_tilemap_get(&world.map, 13, 12) == ROLE_DOOR,
               "ghost house door");
    TGE_ASSERT(tge_tilemap_get(&world.map, 13, 14) == ROLE_FLOOR,
               "ghost home cell is floor");
    TGE_ASSERT(tge_tilemap_get(&world.map, 0, 14) == ROLE_TUNNEL,
               "left tunnel edge");
    TGE_ASSERT(tge_tilemap_get(&world.map, MAZE_W - 1, 14) == ROLE_TUNNEL,
               "right tunnel edge");
    TGE_ASSERT(tge_tilemap_get(&world.map, 10, 8) == ROLE_PELLET,
               "corridor pellet");
}

TGE_TEST(pacman_move_blocked_by_wall)
{
    PacmanWorld world;
    make_test_world(&world);
    world.pac.actor.position = tge_vec2i(1, 1); /* corridor */
    world.pac.direction = TGE_DIR_LEFT;
    TGE_Vec2i next = step_position(&world.map, world.pac.actor.position,
                                   world.pac.direction, true);
    TGE_ASSERT(tge_vec2i_eq(next, tge_vec2i(1, 1)),
               "wall blocks left from (1,1)");
}

TGE_TEST(pacman_idles_until_input)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_ASSERT(world.pac.direction == TGE_DIR_NONE,
               "spawns idle, waiting for the first input");
    TGE_ASSERT(tge_vec2i_eq(world.pac.actor.position, world.level.pac_spawn),
               "at the spawn cell");
    world_step(&world);
    TGE_ASSERT(tge_vec2i_eq(world.pac.actor.position, world.level.pac_spawn),
               "does not auto-run with no input");
}

TGE_TEST(pacman_move_on_floor)
{
    PacmanWorld world;
    make_test_world(&world);
    world.pac.actor.position = tge_vec2i(9, 8); /* pellet corridor */
    world.pac.direction = TGE_DIR_RIGHT;
    TGE_Vec2i next = step_position(&world.map, world.pac.actor.position,
                                   world.pac.direction, true);
    TGE_ASSERT(tge_vec2i_eq(next, tge_vec2i(10, 8)),
               "moves right on floor");
}

TGE_TEST(pacman_eats_pellet)
{
    PacmanWorld world;
    make_test_world(&world);
    world.pac.actor.position = tge_vec2i(9, 8);
    world.pac.direction = TGE_DIR_RIGHT;
    int before = world.score;
    world.pac.actor.position = step_position(&world.map, world.pac.actor.position,
                                              world.pac.direction, true);
    eat_cell(&world);
    TGE_ASSERT(world.score == before + 10, "score +10");
    TGE_ASSERT(tge_tilemap_get(&world.map, 10, 8) == ROLE_FLOOR,
               "pellet becomes floor");
}

TGE_TEST(pacman_eats_power_pellet)
{
    PacmanWorld world;
    make_test_world(&world);
    world.pac.actor.position = tge_vec2i(1, 3); /* power pellet */
    int before = world.score;
    eat_cell(&world);
    TGE_ASSERT(world.score == before + 50, "score +50");
    TGE_ASSERT(world.mode == GHOST_FRIGHTENED, "enters frightened");
    TGE_ASSERT(world.mode_timer > 0.0f, "frightened timer set");
    for (int i = 0; i < GHOST_COUNT; i++)
        TGE_ASSERT(world.ghosts[i].state == GHOST_ACTIVE,
                   "power pellet does not touch ghost states");
}

TGE_TEST(tunnel_wrap_left_from_left_edge)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_Vec2i p = tge_vec2i(0, 14); /* tunnel cell */
    TGE_Vec2i next = step_position(&world.map, p, TGE_DIR_LEFT, true);
    TGE_ASSERT(tge_vec2i_eq(next, tge_vec2i(MAZE_W - 1, 14)),
               "wraps to right edge on same row");
}

TGE_TEST(tunnel_wrap_right_from_right_edge)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_Vec2i p = tge_vec2i(MAZE_W - 1, 14);
    TGE_Vec2i next = step_position(&world.map, p, TGE_DIR_RIGHT, true);
    TGE_ASSERT(tge_vec2i_eq(next, tge_vec2i(0, 14)),
               "wraps to left edge");
}

TGE_TEST(tunnel_no_wrap_on_wall)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_Vec2i p = tge_vec2i(0, 0); /* frame corner, not a tunnel */
    TGE_Vec2i next = step_position(&world.map, p, TGE_DIR_LEFT, true);
    TGE_ASSERT(tge_vec2i_eq(next, p), "no wrap on non-tunnel");
}

TGE_TEST(ghost_can_enter_walks_through_door_when_released)
{
    PacmanWorld world;
    make_test_world(&world);
    world.ghosts[0].state = GHOST_ACTIVE;
    world.ghosts[0].released = true; /* Blinky starts released */
    TGE_ASSERT(ghost_can_enter(&world, &world.ghosts[0], tge_vec2i(13, 12)),
                "released ghost walks through the door");
}

TGE_TEST(ghost_can_enter_blocks_door_for_unreleased)
{
    PacmanWorld world;
    make_test_world(&world);
    /* Inky (index 2) is unreleased at spawn; Clyde (index 3) too. */
    world.ghosts[2].state = GHOST_ACTIVE;
    world.ghosts[2].released = false;
    TGE_ASSERT(!ghost_can_enter(&world, &world.ghosts[2], tge_vec2i(13, 12)),
                "unreleased ghost cannot cross the door");
    /* Eaten ghosts (eyes) always cross the door. */
    world.ghosts[2].state = GHOST_EATEN;
    TGE_ASSERT(ghost_can_enter(&world, &world.ghosts[2], tge_vec2i(13, 12)),
                "eaten ghost (eyes) crosses the door");
}

TGE_TEST(pacman_blocked_by_door)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_Vec2i p = tge_vec2i(13, 11); /* room above the door, facing down */
    TGE_Vec2i next = step_position(&world.map, p, TGE_DIR_DOWN, true);
    TGE_ASSERT(tge_vec2i_eq(next, p), "pacman cannot re-enter the house");
}

TGE_TEST(pacman_leaves_house_through_door)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_Vec2i p = tge_vec2i(13, 13); /* interior, moving up through the door */
    TGE_Vec2i next = step_position(&world.map, p, TGE_DIR_UP, true);
    TGE_ASSERT(tge_vec2i_eq(next, tge_vec2i(13, 12)),
                "door lets pacman leave the house");
}

TGE_TEST(ghost_chooses_nearest_to_target)
{
    PacmanWorld world;
    make_test_world(&world);
    PacmanGhost *g = &world.ghosts[0];
    g->actor.position = tge_vec2i(6, 5);
    g->direction = TGE_DIR_UP;
    world.mode = GHOST_CHASE; /* chase targets pacman */
    world.pac.actor.position = tge_vec2i(6, 4); /* target one up */
    TGE_Direction d = ghost_choose_direction(&world, g);
    TGE_ASSERT(d == TGE_DIR_UP, "chooses UP toward target");
}

TGE_TEST(eat_ghost_in_frightened)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_FRIGHTENED;
    world.mode_timer = 5.0f;
    PacmanGhost *g = &world.ghosts[0];
    g->state = GHOST_ACTIVE;
    g->actor.position = world.pac.actor.position; /* on top of pacman */
    int before = world.score;
    eat_ghost(&world, g);
    TGE_ASSERT(world.score == before + 200, "score +200");
    TGE_ASSERT(g->state == GHOST_EATEN, "ghost marked EATEN");
    TGE_ASSERT(tge_vec2i_eq(g->actor.position, world.pac.actor.position),
               "ghost stays put and walks home from there");
}

TGE_TEST(die_in_chase)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    int lives = world.lives;
    world_die(&world);
    TGE_ASSERT(world.lives == lives - 1, "life lost");
    TGE_ASSERT(world.mode == GHOST_SCATTER, "back to scatter");
}

TGE_TEST(win_when_pellets_zero)
{
    PacmanWorld world;
    make_test_world(&world);
    world.level.pellets_left = 1;
    world.pac.actor.position = tge_vec2i(10, 8); /* some pellet cell */
    eat_cell(&world);
    TGE_ASSERT(world.state == PACMAN_WON, "state is WON");
}

TGE_TEST(world_step_cycles_modes)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_SCATTER;
    world.mode_timer = 0.01f; /* expires immediately on step */
    world_step(&world);
    TGE_ASSERT(world.mode == GHOST_CHASE, "scatter -> chase");
    world.mode_timer = 0.01f;
    world_step(&world);
    TGE_ASSERT(world.mode == GHOST_SCATTER, "chase -> scatter");
}

TGE_TEST(ghost_returns_home_when_eaten)
{
    PacmanWorld world;
    make_test_world(&world);
    PacmanGhost *g = &world.ghosts[0];
    g->state = GHOST_EATEN;
    g->actor.position = tge_vec2i(2, 1); /* far from the house */
    world_step(&world);
    TGE_ASSERT(g->state == GHOST_EATEN, "still walking home");
    TGE_ASSERT(!tge_vec2i_eq(g->actor.position, tge_vec2i(2, 1)),
               "moved toward home");
}

TGE_TEST(ghost_respawns_on_arrival)
{
    PacmanWorld world;
    make_test_world(&world);
    PacmanGhost *g = &world.ghosts[0];
    g->state = GHOST_EATEN;
    g->actor.position = tge_vec2i(13, 13); /* directly above the home cell */
    g->direction = TGE_DIR_DOWN;
    world_step(&world);
    TGE_ASSERT(tge_vec2i_eq(g->actor.position, world.level.ghost_home),
               "walked into the house center this tick");
    TGE_ASSERT(g->state == GHOST_ACTIVE, "respawned exactly on arrival");
}

TGE_TEST(ghost_dormant_inside_house)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    PacmanGhost *g = &world.ghosts[0];
    g->actor.position = world.level.ghost_home;
    g->direction = TGE_DIR_RIGHT;
    world.pac.actor.position = tge_vec2i(14, 14);
    int lives = world.lives;
    world_step(&world);
    TGE_ASSERT(world.lives == lives,
               "ghost inside the house cannot hurt pacman");
}

TGE_TEST(ghost_dangerous_outside_house)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    PacmanGhost *g = &world.ghosts[0];
    g->actor.position = tge_vec2i(1, 1);
    g->direction = TGE_DIR_RIGHT;
    world.pac.actor.position = tge_vec2i(2, 1);
    int lives = world.lives;
    world_step(&world);
    TGE_ASSERT(world.lives == lives - 1,
               "ghost outside the house can hurt pacman");
}

TGE_TEST(crossing_ghost_collision)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    world.pac.actor.position = tge_vec2i(15, 1); /* corridor row 1 */
    world.pac.direction = TGE_DIR_RIGHT;
    PacmanGhost *g = &world.ghosts[0];
    g->state = GHOST_ACTIVE;
    g->actor.position = tge_vec2i(16, 1);
    g->direction = TGE_DIR_LEFT;
    int lives = world.lives;
    world_step(&world);
    TGE_ASSERT(world.lives == lives - 1,
               "head-on swap counts as a collision");
}

TGE_TEST(frightened_ghost_moves_deterministically)
{
    PacmanWorld a, b;
    make_test_world(&a);
    make_test_world(&b);
    a.mode = b.mode = GHOST_FRIGHTENED;
    PacmanGhost *ga = &a.ghosts[0];
    PacmanGhost *gb = &b.ghosts[0];
    ga->actor.position = gb->actor.position = tge_vec2i(2, 1);
    ga->direction = gb->direction = TGE_DIR_DOWN;
    ga->rng = gb->rng = 0x13579BDFu;
    TGE_Direction da = ghost_choose_direction(&a, ga);
    TGE_Direction db = ghost_choose_direction(&b, gb);
    TGE_ASSERT(da == db, "same seed yields same wander choice");
    TGE_ASSERT(da != TGE_DIR_UP, "does not reverse into the wall");
}

TGE_TEST(world_reset_keeps_playable)
{
    PacmanWorld world;
    world_init(&world);
    world.playable = false;
    world_reset(&world);
    TGE_ASSERT(!world.playable, "playable only set by world_resize");
}

TGE_TEST(ghost_blinky_targets_pacman)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    world.pac.actor.position = tge_vec2i(5, 5);
    PacmanGhost *g = &world.ghosts[0];
    g->state = GHOST_ACTIVE;
    g->released = true;
    TGE_Vec2i t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(5, 5)), "Blinky targets Pac-Man in chase");
}

TGE_TEST(ghost_pinky_four_ahead_and_upbug)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    world.pac.actor.position = tge_vec2i(10, 10);
    PacmanGhost *g = &world.ghosts[1];
    g->state = GHOST_ACTIVE;
    g->released = true;
    g->actor.position = tge_vec2i(5, 5); /* outside the house, so the
                                            personality target applies */
    world.pac.direction = TGE_DIR_RIGHT;
    TGE_Vec2i t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(14, 10)), "Pinky aims 4 tiles ahead");
    world.pac.direction = TGE_DIR_UP;
    t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(6, 6)),
                "Pinky's UP overflow bug aims 4 up and 4 left");
}

TGE_TEST(ghost_inky_reflection)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    world.pac.actor.position = tge_vec2i(10, 10);
    world.pac.direction = TGE_DIR_RIGHT;   /* tile two ahead = (12, 10) */
    world.ghosts[0].actor.position = tge_vec2i(8, 10); /* Blinky */
    PacmanGhost *g = &world.ghosts[2];
    g->state = GHOST_ACTIVE;
    g->released = true;
    g->actor.position = tge_vec2i(5, 5); /* outside the house */
    TGE_Vec2i t = ghost_target(&world, g);
    /* target = 2*tile2 - blinky = 2*(12,10) - (8,10) = (16, 10) */
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(16, 10)), "Inky reflects Blinky");
}

TGE_TEST(ghost_clyde_far_vs_close)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    PacmanGhost *g = &world.ghosts[3];
    g->state = GHOST_ACTIVE;
    g->released = true;
    world.pac.actor.position = tge_vec2i(2, 2);
    g->actor.position = tge_vec2i(20, 20); /* far: dist2 = 648 > 64 */
    TGE_Vec2i t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(2, 2)), "Clyde far -> chases Pac-Man");
    g->actor.position = tge_vec2i(2, 4);   /* close: dist2 = 4 <= 64 */
    t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, world.level.scatter_targets[3]),
                "Clyde close -> flees to his corner");
}

TGE_TEST(ghosts_reverse_on_mode_change)
{
    PacmanWorld world;
    make_test_world(&world);
    for (int i = 0; i < GHOST_COUNT; i++) {
        PacmanGhost *g = &world.ghosts[i];
        g->state = GHOST_ACTIVE;
        g->released = true;
        g->actor.position = tge_vec2i(1, 1);
        g->direction = TGE_DIR_RIGHT;
        g->just_reversed = false;
    }
    world.mode = GHOST_SCATTER;
    world.mode_phase = 0;
    world.mode_timer = 0.01f; /* expires on the next step */
    /* Capture the direction each ghost would choose this step (mode is still
     * SCATTER, so the choice is deterministic); the forced reversal on the
     * scatter->chase transition must flip exactly that direction. */
    TGE_Direction pre[GHOST_COUNT];
    for (int i = 0; i < GHOST_COUNT; i++)
        pre[i] = ghost_choose_direction(&world, &world.ghosts[i]);
    world_step(&world);        /* scatter -> chase, forces reversal */
    for (int i = 0; i < GHOST_COUNT; i++) {
        TGE_ASSERT(world.ghosts[i].just_reversed,
                    "active ghost flagged for the forced reversal on mode "
                    "change");
        TGE_ASSERT(world.ghosts[i].direction == tge_direction_opposite(pre[i]),
                    "active ghost reversed away from its chosen direction");
    }
}

TGE_TEST(world_die_preserves_dots_eaten)
{
    PacmanWorld world;
    make_test_world(&world);
    world.dots_eaten = 42;
    int lives = world.lives;
    world_die(&world);
    TGE_ASSERT(world.lives == lives - 1, "life lost");
    TGE_ASSERT(world.dots_eaten == 42, "dots already eaten stay eaten");
}

TGE_TEST(maze_all_rows_have_correct_width)
{
    for (int y = 0; y < MAZE_H; y++) {
        TGE_ASSERT((int)strlen(MAZE[y]) == MAZE_W,
                   "maze row has correct width");
    }
}

TGE_TEST(frightened_pauses_mode_timer)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_SCATTER;
    world.mode_phase = 0;
    world.mode_timer = 2.0f;
    world.frightened_timer = 0.0f;
    /* Eating a power pellet must not touch the Scatter/Chase phase timer:
     * that one is paused and resumes unchanged when frightened ends. */
    world.pac.actor.position = tge_vec2i(1, 3); /* a power pellet cell */
    eat_cell(&world);
    TGE_ASSERT(world.mode == GHOST_FRIGHTENED,
                "power pellet enters frightened");
    TGE_ASSERT(world.frightened_timer == FRIGHTENED_DURATION,
                "frightened timer starts at full duration");
    TGE_ASSERT(world.mode_timer == 2.0f,
                "mode_timer (phase) is left paused on frightened entry");
    /* Step until frightened ends. */
    for (int i = 0; i < 200 && world.mode == GHOST_FRIGHTENED; i++)
        world_step(&world);
    TGE_ASSERT(world.mode != GHOST_FRIGHTENED, "frightened ends");
    TGE_ASSERT(world.mode_timer == 2.0f,
                "phase resumes its paused value, not a full restart");
}

TGE_TEST(ghost_pinky_none_direction)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    world.pac.direction = TGE_DIR_NONE;
    world.pac.actor.position = tge_vec2i(5, 5);
    PacmanGhost *g = &world.ghosts[1]; /* Pinky */
    g->state = GHOST_ACTIVE;
    g->released = true;
    g->actor.position = tge_vec2i(5, 5);
    TGE_Vec2i t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(5, 5)),
                "Pinky targets Pac-Man directly when he is stationary (NONE)");
}

TGE_TEST(ghost_inky_none_direction)
{
    PacmanWorld world;
    make_test_world(&world);
    world.mode = GHOST_CHASE;
    world.pac.direction = TGE_DIR_NONE;
    world.pac.actor.position = tge_vec2i(5, 5);
    PacmanGhost *g = &world.ghosts[2]; /* Inky */
    g->state = GHOST_ACTIVE;
    g->released = true;
    g->actor.position = tge_vec2i(5, 5);
    TGE_Vec2i t = ghost_target(&world, g);
    TGE_ASSERT(tge_vec2i_eq(t, tge_vec2i(5, 5)),
                "Inky targets Pac-Man directly when he is stationary (NONE)");
}

TGE_TEST(released_ghost_exits_house)
{
    PacmanWorld world;
    make_test_world(&world);
    world.lives = 9999; /* keep a stray collision from resetting the test */
    /* Inky (2) and Clyde (3) released, as if their dot thresholds were hit. */
    for (int i = 2; i < GHOST_COUNT; i++) {
        world.ghosts[i].state = GHOST_ACTIVE;
        world.ghosts[i].released = true;
    }
    bool exited[GHOST_COUNT] = { false, false, false, false };
    for (int i = 0; i < 100; i++) {
        world_step(&world);
        for (int g = 2; g < GHOST_COUNT; g++)
            if (!cell_in_ghost_house(world.ghosts[g].actor.position.x,
                                     world.ghosts[g].actor.position.y))
                exited[g] = true;
        if (exited[2] && exited[3])
            break;
    }
    TGE_ASSERT(exited[2], "Inky physically leaves the house once released");
    TGE_ASSERT(exited[3], "Clyde physically leaves the house once released");
}

TGE_TEST(release_threshold_sets_released)
{
    PacmanWorld world;
    make_test_world(&world);
    world.dots_eaten = 29;
    world.pac.actor.position = tge_vec2i(10, 8); /* a pellet cell */
    eat_cell(&world);
    TGE_ASSERT(world.dots_eaten == 30, "the pellet is counted");
    TGE_ASSERT(world.ghosts[2].released, "Inky releases at 30 dots");
    TGE_ASSERT(!world.ghosts[3].released, "Clyde still waits at 30 dots");

    world.dots_eaten = 59;
    world.pac.actor.position = tge_vec2i(1, 8); /* another, uneaten pellet */
    eat_cell(&world);
    TGE_ASSERT(world.dots_eaten == 60, "the pellet is counted");
    TGE_ASSERT(world.ghosts[3].released, "Clyde releases at 60 dots");
}

/* Visual maze layer: every row must decode to exactly MAZE_W*2 codepoints
 * (display columns), and each logical cell to a 2-glyph (6-byte) sprite. The
 * renderer indexes visual_sprites by logical cell, never by raw byte offset,
 * so the dimensions must be exact. */
TGE_TEST(maze_visual_has_exact_dimensions)
{
    TGE_ASSERT(maze_visual_init(), "MAZE_VISUAL initializes (every row has an even codepoint count)");
    for (int y = 0; y < MAZE_H; y++) {
        const char *p = MAZE_VISUAL[y];
        int n = 0;
        while (*p) { maze_utf8_decode(&p); n++; }
        TGE_ASSERT(n == MAZE_W * 2, "row has exactly MAZE_W*2 codepoints");
        for (int x = 0; x < MAZE_W; x++) {
            const char *p = visual_utf8[y][x];
            int c = 0;
            while (*p) { maze_utf8_decode(&p); c++; }
            TGE_ASSERT(c == 2, "each cell is exactly 2 display codepoints");
        }
    }
}

/* The logic maze and the TileMap must agree: walls from '#', the single door
 * from '=', and open interior cells stay non-wall. Connectivity is a logic
 * concern only; wall appearance lives entirely in MAZE_VISUAL. */
TGE_TEST(tilemap_matches_maze_logic)
{
    PacmanWorld world;
    make_test_world(&world);
    TGE_ASSERT(tge_tilemap_get(&world.map, 0, 0) == ROLE_WALL,
               "outer-border corner is a wall");
    TGE_ASSERT(tge_tilemap_get(&world.map, 0, 1) == ROLE_WALL,
               "left border is a wall");
    TGE_ASSERT(tge_tilemap_get(&world.map, 1, 1) != ROLE_WALL,
               "interior cell is not a wall");
    int doors = 0;
    for (int y = 0; y < world.map.height; y++)
        for (int x = 0; x < world.map.width; x++)
            if (tge_tilemap_get(&world.map, x, y) == ROLE_DOOR) doors++;
    TGE_ASSERT(doors == 2, "two-cell door from the '==' marker");
}

int main(void)
{
    test_maze_loads_correct_dimensions();
    test_level_markers_parsed_from_maze();
    test_maze_roles_at_key_positions();
    test_maze_visual_has_exact_dimensions();
    test_tilemap_matches_maze_logic();
    test_pacman_move_blocked_by_wall();
    test_pacman_idles_until_input();
    test_pacman_move_on_floor();
    test_pacman_eats_pellet();
    test_pacman_eats_power_pellet();
    test_tunnel_wrap_left_from_left_edge();
    test_tunnel_wrap_right_from_right_edge();
    test_tunnel_no_wrap_on_wall();
    test_ghost_can_enter_walks_through_door_when_released();
    test_ghost_can_enter_blocks_door_for_unreleased();
    test_pacman_blocked_by_door();
    test_pacman_leaves_house_through_door();
    test_ghost_chooses_nearest_to_target();
    test_eat_ghost_in_frightened();
    test_die_in_chase();
    test_win_when_pellets_zero();
    test_world_step_cycles_modes();
    test_ghost_returns_home_when_eaten();
    test_ghost_respawns_on_arrival();
    test_ghost_dormant_inside_house();
    test_ghost_dangerous_outside_house();
    test_crossing_ghost_collision();
    test_frightened_ghost_moves_deterministically();
    test_ghost_blinky_targets_pacman();
    test_ghost_pinky_four_ahead_and_upbug();
    test_ghost_inky_reflection();
    test_ghost_clyde_far_vs_close();
    test_ghosts_reverse_on_mode_change();
    test_world_die_preserves_dots_eaten();
    test_frightened_pauses_mode_timer();
    test_ghost_pinky_none_direction();
    test_ghost_inky_none_direction();
    test_released_ghost_exits_house();
    test_release_threshold_sets_released();
    test_world_reset_keeps_playable();
    test_maze_all_rows_have_correct_width();
    return tge_test_report();
}
