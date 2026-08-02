#ifndef TGE_EXTRA_COLLISION_H_
#define TGE_EXTRA_COLLISION_H_

#include <stdbool.h>
#include <stdint.h>

#include "tge/tge_math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Broad-phase spatial hash plus narrow-phase AABB collision world.
 *
 * Depends only on the public TGE API (TGE_Rect from tge/tge_math.h); never
 * on internal headers. IDs are caller-owned opaque uint32_t values (e.g. a
 * TGE_EntityId); the world does not interpret them.
 *
 * Guarantees:
 *   - insert() rejects duplicate ids;
 *   - query() never returns the same id twice, even when a rect spans
 *     several hash cells;
 *   - query() returns only genuinely overlapping rects (narrow-phase),
 *     and never allocates.
 *
 * Rect coordinates follow TGE_Rect conventions: [x, x+w) x [y, y+h), and
 * two rects that only touch edges do NOT overlap. */
typedef struct TGE_CollisionWorld TGE_CollisionWorld;

/* Creates a world with cells of cell_size cells; returns NULL when
 * cell_size <= 0 or on allocation failure. */
TGE_CollisionWorld *tge_collision_world_create(int cell_size);
void                tge_collision_world_destroy(TGE_CollisionWorld *world);

/* Registers id with rect. Returns false when id is already present, the
 * rect has non-positive width/height, or the world is out of memory. */
bool tge_collision_insert(TGE_CollisionWorld *world, uint32_t id, TGE_Rect r);

/* Moves id to a new rect; a no-op when id is not in the world. */
void tge_collision_move(TGE_CollisionWorld *world, uint32_t id, TGE_Rect r);

/* Removes id; a no-op when id is not in the world. */
void tge_collision_remove(TGE_CollisionWorld *world, uint32_t id);

/* Writes up to `max` ids overlapping r (deduplicated, in unspecified order)
 * into out and returns how many were written. Never allocates. */
int  tge_collision_query(const TGE_CollisionWorld *world, TGE_Rect r,
                         uint32_t *out, int max);

/* Narrow-phase AABB test (same half-open semantics as the rest of TGE). */
bool tge_collision_rect_overlap(TGE_Rect a, TGE_Rect b);

#ifdef __cplusplus
}
#endif

#endif
