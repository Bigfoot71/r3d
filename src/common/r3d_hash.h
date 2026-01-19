/* r3d_hash.h -- Common R3D Hash Functions
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>

// ========================================
// CONSTANTS
// ========================================

#define R3D_HASH_FNV_OFFSET_BASIS_32 2166136261U
#define R3D_HASH_FNV_PRIME_32        16777619U

#define R3D_HASH_FNV_OFFSET_BASIS_64 14695981039346656037ULL
#define R3D_HASH_FNV_PRIME_64        1099511628211ULL

// ========================================
// INLINED FUNCTIONS
// ========================================

static inline uint32_t r3d_hash_fnv1a_32(const void *data, size_t len)
{
    uint32_t hash = R3D_HASH_FNV_OFFSET_BASIS_32;
    const uint8_t *ptr = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        hash ^= ptr[i];
        hash *= R3D_HASH_FNV_PRIME_32;
    }
    return hash;
}

static inline uint64_t r3d_hash_fnv1a_64(const void *data, size_t len)
{
    uint64_t hash = R3D_HASH_FNV_OFFSET_BASIS_64;
    const uint8_t *ptr = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        hash ^= ptr[i];
        hash *= R3D_HASH_FNV_PRIME_64;
    }
    return hash;
}

static inline uint32_t r3d_hash_fnv1a_32_str(const char *str)
{
    uint32_t hash = R3D_HASH_FNV_OFFSET_BASIS_32;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= R3D_HASH_FNV_PRIME_32;
    }
    return hash;
}

static inline uint64_t r3d_hash_fnv1a_64_str(const char *str)
{
    uint64_t hash = R3D_HASH_FNV_OFFSET_BASIS_64;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= R3D_HASH_FNV_PRIME_64;
    }
    return hash;
}
