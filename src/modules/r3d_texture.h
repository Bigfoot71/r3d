/* r3d_texture.h -- Internal R3D texture module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_TEXTURE_H
#define R3D_MODULE_TEXTURE_H

#include <raylib.h>
#include <glad.h>

// ========================================
// TEXTURE ENUM
// ========================================

typedef enum {
    R3D_TEXTURE_WHITE,
    R3D_TEXTURE_BLACK,
    R3D_TEXTURE_BLANK,
    R3D_TEXTURE_NORMAL,
    R3D_TEXTURE_IBL_BRDF_LUT,
    R3D_TEXTURE_SSGI_LUT,
    R3D_TEXTURE_COUNT
} r3d_texture_t;

// ========================================
// HELPER MACROS
// ========================================

#define R3D_TEXTURE_SELECT(id, or)                          \
    ((id) != 0) ? (id) : r3d_texture_get(R3D_TEXTURE_##or)  \

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_texture_init(void);

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_texture_quit(void);

/*
 * Indicates whether the provided texture ID corresponds to a texture
 * owned by this module.
 */
bool r3d_texture_is_default(GLuint id);

/*
 * Returns the texture ID of the texture specified as a parameter.
 */
GLuint r3d_texture_get(r3d_texture_t texture);

#endif // R3D_MODULE_TEXTURE_H
