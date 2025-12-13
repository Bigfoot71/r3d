/* r3d_decal.h -- R3D Decal Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_DECAL_H
#define R3D_DECAL_H

#include "./r3d_material.h"

/**
 * @defgroup Decal
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a decal and its properties.
 *
 * This structure defines a decal that can be projected onto geometry that has already been rendered.
 *
 * @note Decals are drawn using deferred screen space rendering and do not interact with any
 * forward rendered or non-opaque objects.
 */
typedef struct R3D_Decal {
    R3D_Material material;         /**< The material used for rendering the decal. */
} R3D_Decal;

/** @} */ // end of Decal

#endif // R3D_DECAL_H
