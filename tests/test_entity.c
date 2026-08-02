#include "tge-extra/entity.h"

#include "tge_test.h"

#include <stddef.h>

typedef struct {
    TGE_EntityId ids[64];
    int n;
} Collect;

static void collect_cb(TGE_EntityPool *pool, TGE_EntityId id, void *userdata,
                       void *ctx)
{
    Collect *c = (Collect *)ctx;
    TGE_ASSERT(userdata == tge_entity_get(pool, id)->userdata,
               "userdata is the entity payload");
    if (c->n < 64)
        c->ids[c->n++] = id;
}

static bool id_present(Collect *c, TGE_EntityId id)
{
    for (int i = 0; i < c->n; i++)
        if (c->ids[i] == id)
            return true;
    return false;
}

TGE_TEST(alloc_returns_live_distinct_ids)
{
    TGE_EntityPool *p = tge_entity_pool_create(2);
    TGE_EntityId a = tge_entity_alloc(p, (void *)1);
    TGE_EntityId b = tge_entity_alloc(p, (void *)2);
    TGE_EntityId c = tge_entity_alloc(p, (void *)3);
    TGE_ASSERT(a != TGE_ENTITY_NONE, "a allocated");
    TGE_ASSERT(b != TGE_ENTITY_NONE, "b allocated");
    TGE_ASSERT(c != TGE_ENTITY_NONE, "c allocated");
    TGE_ASSERT(a != b && a != c && b != c, "ids distinct");
    TGE_ASSERT(tge_entity_count(p) == 3, "count is 3");
    tge_entity_pool_destroy(p);
}

TGE_TEST(get_roundtrip_including_null_userdata)
{
    TGE_EntityPool *p = tge_entity_pool_create(2);
    int data = 42;
    TGE_EntityId id = tge_entity_alloc(p, &data);
    TGE_Entity *e = tge_entity_get(p, id);
    TGE_ASSERT(e != NULL, "get returns entity");
    TGE_ASSERT(e->userdata == &data, "userdata roundtrip");
    TGE_EntityId nid = tge_entity_alloc(p, NULL);
    TGE_Entity *ne = tge_entity_get(p, nid);
    TGE_ASSERT(ne != NULL, "NULL userdata entity is still gettable");
    TGE_ASSERT(ne->userdata == NULL, "payload is NULL");
    tge_entity_pool_destroy(p);
}

TGE_TEST(exists_flags)
{
    TGE_EntityPool *p = tge_entity_pool_create(2);
    TGE_EntityId id = tge_entity_alloc(p, NULL);
    TGE_ASSERT(tge_entity_exists(p, id), "live id exists");
    TGE_ASSERT(!tge_entity_exists(p, TGE_ENTITY_NONE), "NONE never exists");
    TGE_ASSERT(!tge_entity_exists(p, (TGE_EntityId)0xFFFFF123), "out-of-range id missing");
    TGE_ASSERT(!tge_entity_exists(NULL, id), "NULL pool, no exists");
    tge_entity_pool_destroy(p);
}

TGE_TEST(stale_id_returns_null)
{
    TGE_EntityPool *p = tge_entity_pool_create(2);
    TGE_EntityId a = tge_entity_alloc(p, (void *)1);
    TGE_EntityId b = tge_entity_alloc(p, (void *)2);
    tge_entity_release(p, a);
    TGE_ASSERT(tge_entity_get(p, a) == NULL, "released id is stale");
    TGE_ASSERT(!tge_entity_exists(p, a), "stale id not exists");
    TGE_ASSERT(tge_entity_get(p, b) != NULL, "other entity unaffected");
    TGE_EntityId c = tge_entity_alloc(p, (void *)3); /* likely reuses a's slot */
    TGE_ASSERT(tge_entity_get(p, a) == NULL, "old handle stays stale after reuse");
    TGE_ASSERT(c != a, "new id differs from stale id");
    TGE_ASSERT(tge_entity_get(p, c) != NULL, "reused slot is live under new id");
    tge_entity_pool_destroy(p);
}

TGE_TEST(release_invalid_is_noop)
{
    TGE_EntityPool *p = tge_entity_pool_create(2);
    TGE_EntityId a = tge_entity_alloc(p, NULL);
    tge_entity_release(p, TGE_ENTITY_NONE);
    tge_entity_release(p, (TGE_EntityId)0xDEADBEEF);
    tge_entity_release(p, a);
    tge_entity_release(p, a);
    TGE_ASSERT(tge_entity_count(p) == 0, "count 0 after releases");
    tge_entity_pool_destroy(p);
}

TGE_TEST(stress_many_reuse_cycles)
{
    TGE_EntityPool *p = tge_entity_pool_create(1);
    for (int cyc = 0; cyc < 500; cyc++) {
        TGE_EntityId first[16];
        for (int i = 0; i < 16; i++) {
            first[i] = tge_entity_alloc(p, NULL);
            TGE_ASSERT(first[i] != TGE_ENTITY_NONE, "alloc ok");
        }
        TGE_ASSERT(tge_entity_count(p) == 16, "count 16");
        for (int i = 0; i < 16; i++) {
            TGE_ASSERT(tge_entity_get(p, first[i]) != NULL, "all live");
            tge_entity_release(p, first[i]);
        }
        TGE_ASSERT(tge_entity_count(p) == 0, "count 0");
        for (int i = 0; i < 16; i++)
            TGE_ASSERT(tge_entity_get(p, first[i]) == NULL, "all stale");
    }
    tge_entity_pool_destroy(p);
}

TGE_TEST(grow_preserves_existing_ids)
{
    TGE_EntityPool *p = tge_entity_pool_create(2); /* min capacity 16, grows to 32 */
    TGE_EntityId first[40];
    for (int i = 0; i < 40; i++) {
        first[i] = tge_entity_alloc(p, (void *)(size_t)(i + 1));
        TGE_ASSERT(first[i] != TGE_ENTITY_NONE, "alloc ok through growth");
    }
    TGE_ASSERT(tge_entity_count(p) == 40, "count 40");
    for (int i = 0; i < 40; i++) {
        TGE_Entity *e = tge_entity_get(p, first[i]);
        TGE_ASSERT(e != NULL, "early id still valid after grow");
        TGE_ASSERT(e->userdata == (void *)(size_t)(i + 1), "userdata intact");
    }
    tge_entity_pool_destroy(p);
}

TGE_TEST(for_each_visits_alive_only)
{
    TGE_EntityPool *p = tge_entity_pool_create(2);
    TGE_EntityId all[5];
    for (int i = 0; i < 5; i++)
        all[i] = tge_entity_alloc(p, (void *)(size_t)(i + 1));
    tge_entity_release(p, all[1]);
    tge_entity_release(p, all[3]);
    Collect c;
    c.n = 0;
    tge_entity_for_each(p, collect_cb, &c);
    TGE_ASSERT(c.n == 3, "visits 3 alive entities");
    TGE_ASSERT(id_present(&c, all[0]), "0 visited");
    TGE_ASSERT(!id_present(&c, all[1]), "released not visited");
    TGE_ASSERT(id_present(&c, all[2]), "2 visited");
    TGE_ASSERT(!id_present(&c, all[3]), "released not visited");
    TGE_ASSERT(id_present(&c, all[4]), "4 visited");
    tge_entity_for_each(p, NULL, &c);
    tge_entity_for_each(NULL, collect_cb, &c);
    tge_entity_pool_destroy(p);
}

TGE_TEST(null_safety)
{
    TGE_ASSERT(tge_entity_get(NULL, (TGE_EntityId)123) == NULL, "get NULL pool");
    TGE_ASSERT(tge_entity_alloc(NULL, NULL) == TGE_ENTITY_NONE, "alloc NULL pool");
    TGE_ASSERT(tge_entity_count(NULL) == 0, "count NULL pool");
    tge_entity_release(NULL, (TGE_EntityId)1);
    tge_entity_pool_destroy(NULL);
}

int main(void)
{
    test_alloc_returns_live_distinct_ids();
    test_get_roundtrip_including_null_userdata();
    test_exists_flags();
    test_stale_id_returns_null();
    test_release_invalid_is_noop();
    test_stress_many_reuse_cycles();
    test_grow_preserves_existing_ids();
    test_for_each_visits_alive_only();
    test_null_safety();
    return tge_test_report();
}
