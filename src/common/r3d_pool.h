/* r3d_pool.h -- Generic contiguous object pool with versioned handles.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_POOL_H
#define R3D_POOL_H

#include <raylib.h>
#include <stddef.h>
#include <stdint.h>

// ========================================
// HANDLE TYPE & HELPERS
// ========================================

/* Public handle: simply index + 1. 0 is always null/invalid */
typedef uint32_t r3d_pool_id_t;

#define R3D_POOL_ID_NULL        0u
#define R3D_POOL_ID_MAX_INDEX   0xFFFFFFFFu

/* Extract the slot index from a handle (0-based) */
#define R3D_POOL_ID_INDEX(id)   ((uint32_t)((id) - 1u))

/* Build a handle from a 0-based slot index */
#define R3D_POOL_ID_MAKE(index) ((r3d_pool_id_t)((index) + 1u))

// ========================================
// POOL STRUCT
// ========================================

/* Memory layout (single allocation):
 *
 *   [ r3d_pool_t | gens[capacity] | gensRef[capacity] | freeStack[capacity] | objects[capacity * objSize] ]
 *
 *   gens[]      : current generation per slot (0 = free)
 *   gensRef[]   : generation at insertion time, used to validate handles
 *   freeStack[] : LIFO stack of recycled slot indices
 *   objects[]   : packed object bytes
 */
typedef struct {
    uint32_t objSize;       // Size of one object in bytes
    uint32_t count;         // Number of live objects
    uint32_t capacity;      // Total allocated slots
    uint32_t freeCount;     // Number of entries in freeStack
    uint16_t* gens;         // Current generation per slot
    uint16_t* gensRef;      // Generation at last insertion, per slot
    uint16_t* freeStack;    // Recycled slot indices
    void* objects;          // Packed object storage
} r3d_pool_t;

// ========================================
// POOL ITERATION
// ========================================

/* Iterate over all live objects (skips free slots)
 * 'pool'    : r3d_pool_t*
 * 'Type'    : concrete object type (eg r3d_light_t)
 * 'varname' : loop variable name, declared as Type*
 * 'idx'     : uint32_t slot index, usable inside the loop body (eg to rebuild a handle) */
#define R3D_POOL_FOR_EACH(pool, Type, varname, idx)         \
    for (uint32_t idx = 0; idx < (pool)->capacity; idx++)   \
        if ((pool)->gens[idx] != 0)                         \
            for (Type* varname =                            \
                     (Type*)((uint8_t*)(pool)->objects      \
                             + idx * (pool)->objSize);      \
                 varname;                                   \
                 varname = NULL)

// ========================================
// POOL FUNCTIONS
// ========================================

/* Allocate a pool with the given object size and initial capacity
 * Returns NULL on allocation failure */
r3d_pool_t* r3d_pool_create(uint32_t objSize, uint32_t capacity);

/* Free all memory used by the pool. */
void r3d_pool_destroy(r3d_pool_t* pool);

/* Grow the pool to at least double its current capacity
 * Returns the new pool pointer (the old one is invalid after this call)
 * Returns NULL on failure (old pool remains valid in that case) */
r3d_pool_t* r3d_pool_grow(r3d_pool_t* pool);

/* Return whether a handle is valid (O(1)) */
bool r3d_pool_valid(const r3d_pool_t* pool, r3d_pool_id_t id);

/* Return a pointer to the object for the given handle, or NULL if invalid */
void* r3d_pool_get(const r3d_pool_t* pool, r3d_pool_id_t id);

/* Allocate a new slot and return its handle
 * *poolPtr may be updated if the pool had to grow
 * Returns R3D_POOL_ID_NULL on failure */
r3d_pool_id_t r3d_pool_insert(r3d_pool_t** poolPtr);

/* Release the slot held by the given handle
 * Does nothing if the handle is invalid */
void r3d_pool_remove(r3d_pool_t* pool, r3d_pool_id_t id);

#endif // R3D_POOL_H
