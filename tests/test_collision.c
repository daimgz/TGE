#include "tge-extra/collision.h"

#include "tge_test.h"

static int count_in(const uint32_t *out, int n, uint32_t id)
{
    int k = 0;
    for (int i = 0; i < n; i++)
        if (out[i] == id)
            k++;
    return k;
}

TGE_TEST(create_rejects_bad_cell_size)
{
    TGE_ASSERT(tge_collision_world_create(0) == NULL, "cell 0 rejected");
    TGE_ASSERT(tge_collision_world_create(-3) == NULL, "negative rejected");
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    TGE_ASSERT(w != NULL, "positive accepted");
    tge_collision_world_destroy(w);
}

TGE_TEST(insert_and_query_overlap)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    TGE_Rect r = tge_rect(2, 2, 2, 2);
    TGE_ASSERT(tge_collision_insert(w, 1, r), "insert ok");
    uint32_t out[8];
    int n = tge_collision_query(w, r, out, 8);
    TGE_ASSERT(n == 1 && out[0] == 1, "query finds overlap");
    tge_collision_world_destroy(w);
}

TGE_TEST(distant_rect_not_returned)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    tge_collision_insert(w, 1, tge_rect(2, 2, 2, 2));
    tge_collision_insert(w, 2, tge_rect(60, 2, 2, 2));
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(2, 2, 2, 2), out, 8);
    TGE_ASSERT(n == 1 && out[0] == 1, "only nearby id returned");
    tge_collision_world_destroy(w);
}

TGE_TEST(move_updates_cells)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    tge_collision_insert(w, 1, tge_rect(2, 2, 2, 2));
    tge_collision_move(w, 1, tge_rect(60, 2, 2, 2));
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(2, 2, 2, 2), out, 8);
    TGE_ASSERT(n == 0, "old position empty");
    n = tge_collision_query(w, tge_rect(60, 2, 2, 2), out, 8);
    TGE_ASSERT(n == 1 && out[0] == 1, "new position found");
    tge_collision_world_destroy(w);
}

TGE_TEST(remove_excludes)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    tge_collision_insert(w, 1, tge_rect(2, 2, 2, 2));
    tge_collision_remove(w, 1);
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(2, 2, 2, 2), out, 8);
    TGE_ASSERT(n == 0, "removed id gone");
    tge_collision_remove(w, 99);
    tge_collision_world_destroy(w);
}

TGE_TEST(duplicate_id_rejected)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    TGE_ASSERT(tge_collision_insert(w, 1, tge_rect(2, 2, 2, 2)), "first insert");
    TGE_ASSERT(!tge_collision_insert(w, 1, tge_rect(9, 9, 2, 2)), "duplicate rejected");
    tge_collision_world_destroy(w);
}

TGE_TEST(invalid_rect_rejected)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    TGE_ASSERT(!tge_collision_insert(w, 1, tge_rect(2, 2, 0, 2)), "w 0 rejected");
    TGE_ASSERT(!tge_collision_insert(w, 1, tge_rect(2, 2, 2, -1)), "h negative rejected");
    tge_collision_world_destroy(w);
}

TGE_TEST(max_truncates)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    for (int i = 0; i < 5; i++)
        tge_collision_insert(w, (uint32_t)(i + 1), tge_rect(2 + i, 2, 1, 1));
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(2, 2, 6, 1), out, 3);
    TGE_ASSERT(n == 3, "max respected");
    tge_collision_world_destroy(w);
}

TGE_TEST(huge_rect_dedup)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    tge_collision_insert(w, 1, tge_rect(0, 0, 10, 1)); /* spans cells 0..2 */
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(0, 0, 10, 1), out, 8);
    TGE_ASSERT(n == 1, "one result");
    TGE_ASSERT(count_in(out, n, 1) == 1, "no duplicates across cells");
    tge_collision_world_destroy(w);
}

TGE_TEST(edge_touching_does_not_overlap)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    tge_collision_insert(w, 1, tge_rect(0, 0, 2, 2));
    tge_collision_insert(w, 2, tge_rect(2, 0, 2, 2)); /* shares edge x=2 */
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(0, 0, 2, 2), out, 8);
    TGE_ASSERT(n == 1 && out[0] == 1, "edge-only contact excluded");
    tge_collision_world_destroy(w);
}

TGE_TEST(same_cell_non_overlapping_filtered)
{
    TGE_CollisionWorld *w = tge_collision_world_create(8);
    tge_collision_insert(w, 1, tge_rect(0, 0, 1, 1));
    tge_collision_insert(w, 2, tge_rect(2, 0, 1, 1)); /* same cell, no overlap */
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(0, 0, 1, 1), out, 8);
    TGE_ASSERT(n == 1 && out[0] == 1, "narrow-phase filters same-cell non-overlap");
    tge_collision_world_destroy(w);
}

TGE_TEST(rect_overlap_helper)
{
    TGE_ASSERT(tge_collision_rect_overlap(tge_rect(0, 0, 2, 2), tge_rect(1, 1, 2, 2)), "corner overlap");
    TGE_ASSERT(!tge_collision_rect_overlap(tge_rect(0, 0, 2, 2), tge_rect(2, 0, 2, 2)), "edge touch no overlap");
    TGE_ASSERT(!tge_collision_rect_overlap(tge_rect(0, 0, 1, 1), tge_rect(5, 5, 1, 1)), "far apart");
}

TGE_TEST(empty_world_and_null_safety)
{
    TGE_CollisionWorld *w = tge_collision_world_create(4);
    uint32_t out[8];
    int n = tge_collision_query(w, tge_rect(0, 0, 4, 4), out, 8);
    TGE_ASSERT(n == 0, "empty world");
    TGE_ASSERT(tge_collision_query(NULL, tge_rect(0, 0, 4, 4), out, 8) == 0, "NULL world");
    TGE_ASSERT(tge_collision_query(w, tge_rect(0, 0, 4, 4), NULL, 8) == 0, "NULL out");
    TGE_ASSERT(tge_collision_query(w, tge_rect(0, 0, 4, 4), out, 0) == 0, "max 0");
    TGE_ASSERT(!tge_collision_insert(NULL, 1, tge_rect(0, 0, 2, 2)), "insert NULL");
    tge_collision_move(NULL, 1, tge_rect(0, 0, 2, 2));
    tge_collision_remove(NULL, 1);
    tge_collision_world_destroy(NULL);
    tge_collision_world_destroy(w);
}

TGE_TEST(stress_many_entities)
{
    TGE_CollisionWorld *w = tge_collision_world_create(2);
    for (int i = 0; i < 200; i++) {
        int x = (i % 20) * 3;
        int y = (i / 20) * 3;
        TGE_ASSERT(tge_collision_insert(w, (uint32_t)i, tge_rect(x, y, 1, 1)), "insert ok");
    }
    uint32_t out[256];
    int n = tge_collision_query(w, tge_rect(0, 0, 60, 30), out, 256);
    TGE_ASSERT(n == 200, "all 200 found");
    for (int i = 0; i < 200; i++)
        TGE_ASSERT(count_in(out, n, (uint32_t)i) == 1, "each id once");
    for (int i = 0; i < 50; i++)
        tge_collision_remove(w, (uint32_t)i);
    n = tge_collision_query(w, tge_rect(0, 0, 60, 30), out, 256);
    TGE_ASSERT(n == 150, "150 after removals");
    for (int i = 0; i < 50; i++)
        TGE_ASSERT(count_in(out, n, (uint32_t)i) == 0, "removed ids absent");
    tge_collision_world_destroy(w);
}

int main(void)
{
    test_create_rejects_bad_cell_size();
    test_insert_and_query_overlap();
    test_distant_rect_not_returned();
    test_move_updates_cells();
    test_remove_excludes();
    test_duplicate_id_rejected();
    test_invalid_rect_rejected();
    test_max_truncates();
    test_huge_rect_dedup();
    test_edge_touching_does_not_overlap();
    test_same_cell_non_overlapping_filtered();
    test_rect_overlap_helper();
    test_empty_world_and_null_safety();
    test_stress_many_entities();
    return tge_test_report();
}
