/* r3d_pool.c -- Generic contiguous object pool with versioned handles.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_pool.h"

#include <stdlib.h>
#include <string.h>

#define POOL_MAX_GEN 0xFFFFu // 65535; gen wraps 65535 -> 1, never 0

// ========================================
// INTERNAL HELPERS
// ========================================

static inline size_t pool_total_size(uint32_t capacity, uint32_t objSize)
{
    return sizeof(r3d_pool_t)
        + capacity * sizeof(uint16_t) // gens
        + capacity * sizeof(uint16_t) // gensRef
        + capacity * sizeof(uint16_t) // freeStack
        + (size_t)capacity * objSize; // objects
}

static inline void pool_set_pointers(r3d_pool_t* pool, uint32_t capacity)
{
    uint8_t* base = (uint8_t*)pool + sizeof(r3d_pool_t);
    pool->gens = (uint16_t*)base;
    pool->gensRef = pool->gens + capacity;
    pool->freeStack = pool->gensRef + capacity;
    pool->objects = (void*)(pool->freeStack + capacity);
}

// ========================================
// POOL FUNCTIONS
// ========================================

r3d_pool_t* r3d_pool_create(uint32_t objSize, uint32_t capacity)
{
    if (objSize == 0 || capacity == 0) {
        return NULL;
    }

    uint8_t* mem = RL_MALLOC(pool_total_size(capacity, objSize));
    if (!mem) return NULL;

    r3d_pool_t* pool = (r3d_pool_t*)mem;
    pool->objSize = objSize;
    pool->count = 0;
    pool->capacity = capacity;
    pool->freeCount = 0;

    pool_set_pointers(pool, capacity);

    memset(pool->gens, 0, capacity * sizeof(uint16_t));
    memset(pool->gensRef, 0, capacity * sizeof(uint16_t));

    return pool;
}

void r3d_pool_destroy(r3d_pool_t* pool)
{
    RL_FREE(pool);
}

r3d_pool_t* r3d_pool_grow(r3d_pool_t* pool)
{
    uint32_t oldCap = pool->capacity;
    uint32_t newCap = oldCap * 2;
    if (newCap <= oldCap) return NULL; // Overflow or already at limit

    uint8_t* newMem = RL_MALLOC(pool_total_size(newCap, pool->objSize));
    if (!newMem) return NULL;

    r3d_pool_t* newPool = (r3d_pool_t*)newMem;
    *newPool = *pool;
    newPool->capacity = newCap;

    pool_set_pointers(newPool, newCap);

    // Copy existing data
    memcpy(newPool->gens, pool->gens, oldCap * sizeof(uint16_t));
    memcpy(newPool->gensRef, pool->gensRef, oldCap * sizeof(uint16_t));
    memcpy(newPool->freeStack, pool->freeStack, pool->freeCount * sizeof(uint16_t));
    memcpy(newPool->objects, pool->objects, (size_t)pool->count * pool->objSize);

    // Zero new slots
    memset(newPool->gens + oldCap, 0, (newCap - oldCap) * sizeof(uint16_t));
    memset(newPool->gensRef + oldCap, 0, (newCap - oldCap) * sizeof(uint16_t));

    RL_FREE(pool);
    return newPool;
}

bool r3d_pool_valid(const r3d_pool_t* pool, r3d_pool_id_t id)
{
    if (id == R3D_POOL_ID_NULL) return false;
    uint32_t index = R3D_POOL_ID_INDEX(id);
    if (index >= pool->capacity) return false;
    // A slot is live if its current gen matches the gen recorded at insertion
    return pool->gens[index] != 0
        && pool->gens[index] == pool->gensRef[index];
}

void* r3d_pool_get(const r3d_pool_t* pool, r3d_pool_id_t id)
{
    if (!r3d_pool_valid(pool, id)) return NULL;
    return (uint8_t*)pool->objects + R3D_POOL_ID_INDEX(id) * pool->objSize;
}

r3d_pool_id_t r3d_pool_insert(r3d_pool_t** poolPtr)
{
    r3d_pool_t* pool = *poolPtr;

    // Grow if no free slot and dense array is full
    if (pool->freeCount == 0 && pool->count >= pool->capacity) {
        pool = r3d_pool_grow(pool);
        if (!pool) return R3D_POOL_ID_NULL;
        *poolPtr = pool;
    }

    // Pick a slot: recycle or append
    uint32_t index = (pool->freeCount > 0)
        ? pool->freeStack[--pool->freeCount]
        : pool->count;

    // Bump generation (wraps 65535 -> 1, never 0)
    uint16_t gen = (uint16_t)((pool->gens[index] % POOL_MAX_GEN) + 1u);
    pool->gens[index] = gen;
    pool->gensRef[index] = gen; // Record gen for future validation
    pool->count++;

    return R3D_POOL_ID_MAKE(index);
}

void r3d_pool_remove(r3d_pool_t* pool, r3d_pool_id_t id)
{
    if (!r3d_pool_valid(pool, id)) return;

    uint32_t index = R3D_POOL_ID_INDEX(id);

    // Invalidate: bump gens but leave gensRef at the old value
    pool->gens[index] = 0;
    pool->freeStack[pool->freeCount++] = (uint16_t)index;
    pool->count--;
}
