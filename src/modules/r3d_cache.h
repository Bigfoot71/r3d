/* r3d_cache.h -- Internal R3D cache module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_CACHE_H
#define R3D_MODULE_CACHE_H

#include <r3d/r3d_environment.h>
#include <r3d/r3d_core.h>

#include "../details/r3d_frustum.h"

// ========================================
// HELPER MACROS
// ========================================

/*
 * Read access to cached module members.
 * No added value, but it allows for consistency.
 */
#define R3D_CACHE_GET(member)                   \
    (R3D_MOD_CACHE.member)

/*
 * Write access to cached module members.
 * No added value, but it allows for consistency.
 */
#define R3D_CACHE_SET(member, value)            \
    (R3D_MOD_CACHE.member) = (value)

/*
 * Check if all specified flags are set.
 */
#define R3D_CACHE_FLAGS_HAS(flags, mask)        \
    (((R3D_MOD_CACHE.flags) & (mask)) == (mask))

/*
 * Set specified flags (bitwise OR).
 */
#define R3D_CACHE_FLAGS_ASSIGN(flags, mask) do {\
    (R3D_MOD_CACHE.flags) |= (mask);            \
} while (0)

/*
 * Clear specified flags (bitwise AND NOT).
 */
#define R3D_CACHE_FLAGS_CLEAR(flags, mask) do { \
    (R3D_MOD_CACHE.flags) &= ~(mask);           \
} while (0)

// ========================================
// MODULE STATE
// ========================================

/*
 * Current viewport state including view frustum and transforms.
 */
typedef struct {
    r3d_frustum_t frustum;          //< View frustum for culling
    Vector3 viewPosition;           //< Camera position in world space
    Matrix view, invView;           //< View matrix and its inverse
    Matrix proj, invProj;           //< Projection matrix and its inverse
    Matrix viewProj;                //< Combined view-projection matrix
} r3d_viewport_t;

/*
 * Global cache for frequently accessed renderer state.
 * Reduces parameter passing and provides centralized access to common data.
 */
extern struct r3d_mod_cache {
    R3D_Environment environment;    //< Current environment settings
    r3d_viewport_t viewport;        //< Current viewport state

    TextureFilter textureFilter;    //< Default texture filter for model loading
    Matrix matCubeViews[6];         //< Pre-computed view matrices for cubemap faces

    R3D_Layer layers;               //< Active rendering layers
    R3D_Flags state;                //< Renderer state flags
} R3D_MOD_CACHE;

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_mod_cache_init(R3D_Flags flags);
void r3d_mod_cache_quit(void);

#endif // R3D_MODULE_CACHE_H
