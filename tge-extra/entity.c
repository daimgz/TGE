#include "entity.h"

#include <stdlib.h>

/* ID layout (implementation detail, NOT part of the API):
 *   low  TGE_SLOT_BITS bits: slot index into the parallel arrays
 *   high 16 bits:           generation, bumped on every release
 * Generation starts at 1 so no valid id ever equals TGE_ENTITY_NONE.
 */
#define TGE_SLOT_BITS 16
#define TGE_SLOT_MASK ((1u << TGE_SLOT_BITS) - 1u)
#define TGE_MIN_CAPACITY 16

struct TGE_EntityPool {
    TGE_Entity *slots;       /* payload per slot */
    uint8_t    *occupied;    /* 1 = slot is a live entity */
    uint16_t   *generation;  /* bumped on release; starts at 1 */
    uint32_t   *free_list;   /* LIFO of free slot indices */
    int         capacity;
    int         free_count;
    int         alive;
};

static uint32_t slot_of(TGE_EntityId id)
{
    return id & TGE_SLOT_MASK;
}

static uint32_t generation_of(TGE_EntityId id)
{
    return (uint32_t)(id >> TGE_SLOT_BITS);
}

static TGE_EntityId make_id(uint32_t slot, uint32_t generation)
{
    return (TGE_EntityId)((generation << TGE_SLOT_BITS) | slot);
}

TGE_EntityPool *tge_entity_pool_create(int capacity)
{
    if (capacity < TGE_MIN_CAPACITY)
        capacity = TGE_MIN_CAPACITY;
    TGE_EntityPool *pool = (TGE_EntityPool *)calloc(1, sizeof(*pool));
    if (!pool)
        return NULL;
    pool->capacity = capacity;
    pool->slots = (TGE_Entity *)calloc((size_t)capacity, sizeof(TGE_Entity));
    pool->occupied = (uint8_t *)calloc((size_t)capacity, sizeof(uint8_t));
    pool->generation = (uint16_t *)malloc((size_t)capacity * sizeof(uint16_t));
    pool->free_list = (uint32_t *)malloc((size_t)capacity * sizeof(uint32_t));
    if (!pool->slots || !pool->occupied || !pool->generation || !pool->free_list) {
        free(pool->slots);
        free(pool->occupied);
        free(pool->generation);
        free(pool->free_list);
        free(pool);
        return NULL;
    }
    for (int i = 0; i < capacity; i++) {
        pool->generation[i] = 1;
        pool->free_list[i] = (uint32_t)i;
    }
    pool->free_count = capacity;
    return pool;
}

static bool pool_grow(TGE_EntityPool *pool)
{
    int new_cap = pool->capacity * 2;
    TGE_Entity *ns = (TGE_Entity *)realloc(pool->slots,
                                           (size_t)new_cap * sizeof(TGE_Entity));
    if (!ns)
        return false;
    pool->slots = ns;
    uint16_t *ng = (uint16_t *)realloc(pool->generation,
                                       (size_t)new_cap * sizeof(uint16_t));
    if (!ng)
        return false;
    pool->generation = ng;
    uint32_t *nf = (uint32_t *)realloc(pool->free_list,
                                       (size_t)new_cap * sizeof(uint32_t));
    if (!nf)
        return false;
    pool->free_list = nf;
    uint8_t *no = (uint8_t *)realloc(pool->occupied,
                                     (size_t)new_cap * sizeof(uint8_t));
    if (!no)
        return false;
    pool->occupied = no;
    int added = new_cap - pool->capacity;
    for (int i = 0; i < added; i++) {
        pool->generation[pool->capacity + i] = 1;
        pool->occupied[pool->capacity + i] = 0;
        pool->free_list[pool->free_count + i] = (uint32_t)(pool->capacity + i);
    }
    pool->free_count += added;
    pool->capacity = new_cap;
    return true;
}

void tge_entity_pool_destroy(TGE_EntityPool *pool)
{
    if (!pool)
        return;
    free(pool->slots);
    free(pool->occupied);
    free(pool->generation);
    free(pool->free_list);
    free(pool);
}

TGE_EntityId tge_entity_alloc(TGE_EntityPool *pool, void *userdata)
{
    if (!pool)
        return TGE_ENTITY_NONE;
    if (pool->free_count == 0 && !pool_grow(pool))
        return TGE_ENTITY_NONE;
    uint32_t slot = pool->free_list[--pool->free_count];
    pool->slots[slot].userdata = userdata;
    pool->occupied[slot] = 1;
    pool->alive++;
    return make_id(slot, pool->generation[slot]);
}

TGE_Entity *tge_entity_get(TGE_EntityPool *pool, TGE_EntityId id)
{
    if (!pool || id == TGE_ENTITY_NONE)
        return NULL;
    uint32_t slot = slot_of(id);
    if (slot >= (uint32_t)pool->capacity)
        return NULL;
    if (generation_of(id) != pool->generation[slot])
        return NULL;
    if (!pool->occupied[slot])
        return NULL;
    return &pool->slots[slot];
}

bool tge_entity_exists(TGE_EntityPool *pool, TGE_EntityId id)
{
    return tge_entity_get(pool, id) != NULL;
}

void tge_entity_release(TGE_EntityPool *pool, TGE_EntityId id)
{
    if (!tge_entity_get(pool, id))
        return;
    uint32_t slot = slot_of(id);
    pool->slots[slot].userdata = NULL;
    pool->occupied[slot] = 0;
    pool->generation[slot]++; /* invalidates the id immediately */
    pool->free_list[pool->free_count++] = slot;
    pool->alive--;
}

int tge_entity_count(const TGE_EntityPool *pool)
{
    return pool ? pool->alive : 0;
}

void tge_entity_for_each(TGE_EntityPool *pool, tge_entity_cb fn, void *ctx)
{
    if (!pool || !fn)
        return;
    for (int i = 0; i < pool->capacity; i++) {
        if (pool->occupied[i])
            fn(pool, make_id((uint32_t)i, pool->generation[i]),
               pool->slots[i].userdata, ctx);
    }
}
