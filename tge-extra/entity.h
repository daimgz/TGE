#ifndef TGE_EXTRA_ENTITY_H_
#define TGE_EXTRA_ENTITY_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque, stable entity handle.
 *
 * Two properties are guaranteed:
 *   - uniqueness: a live handle refers to exactly one entity;
 *   - staleness detection: once an entity is released, any handle that
 *     previously referred to it identifies nothing (get() returns NULL).
 *
 * The internal bit layout is NOT part of the API and may change freely;
 * only the guarantees above hold. Handles stay valid across pool growth.
 */
typedef uint32_t TGE_EntityId;

/* Invalid handle; also returned by alloc() when the pool is out of memory. */
#define TGE_ENTITY_NONE ((TGE_EntityId)0)

/* User-owned payload. TGE never dereferences userdata; a NULL userdata is
 * a valid payload. */
typedef struct TGE_Entity {
    void *userdata;
} TGE_Entity;

typedef struct TGE_EntityPool TGE_EntityPool;

/* Creates a pool. capacity <= 0 still yields a usable pool (a small minimum
 * capacity is allocated internally and the pool grows on demand). Returns
 * NULL on allocation failure. */
TGE_EntityPool *tge_entity_pool_create(int capacity);
void            tge_entity_pool_destroy(TGE_EntityPool *pool);

/* Allocates an entity with the given userdata; returns its handle, or
 * TGE_ENTITY_NONE on allocation failure. */
TGE_EntityId tge_entity_alloc(TGE_EntityPool *pool, void *userdata);

/* Returns the entity, or NULL when id is stale, invalid or from another
 * pool. */
TGE_Entity *tge_entity_get(TGE_EntityPool *pool, TGE_EntityId id);

/* True when id refers to a live entity in this pool. */
bool tge_entity_exists(TGE_EntityPool *pool, TGE_EntityId id);

/* Releases the entity. Its id becomes stale immediately; later alloc()s may
 * reuse the slot. Releasing an invalid id is a no-op. */
void tge_entity_release(TGE_EntityPool *pool, TGE_EntityId id);

/* Number of live entities. */
int tge_entity_count(const TGE_EntityPool *pool);

/* Visits every live entity once, in unspecified order. userdata is the
 * entity's own payload (may be NULL); ctx is the caller's context,
 * forwarded verbatim from tge_entity_for_each(). Releasing or allocating
 * entities while iterating is undefined behaviour. */
typedef void (*tge_entity_cb)(TGE_EntityPool *pool, TGE_EntityId id,
                              void *userdata, void *ctx);
void tge_entity_for_each(TGE_EntityPool *pool, tge_entity_cb fn, void *ctx);

#ifdef __cplusplus
}
#endif

#endif
