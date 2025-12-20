/* r3d_texture.h -- Internal R3D texture module.
 *
 * Copyright (c) 2025 Le Juez Victor
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
    R3D_TEXTURE_NORMAL,
    R3D_TEXTURE_SSAO_NOISE,
    R3D_TEXTURE_SSAO_KERNEL,
    R3D_TEXTURE_IBL_BRDF_LUT,
    R3D_TEXTURE_COUNT
} r3d_texture_t;

// ========================================
// HELPER MACROS
// ========================================

#define R3D_TEXTURE_SELECT(id, or)                          \
    ((id) != 0) ? (id) : r3d_texture_get(R3D_TEXTURE_##or)  \

// ========================================
// TEXTURE FUNCTIONS
// ========================================

bool r3d_texture_is_default(GLuint id);
GLuint r3d_texture_get(r3d_texture_t texture);

// ========================================
// MODULE FUNCTIONS
// ========================================

void r3d_mod_texture_init(void);
void r3d_mod_texture_quit(void);

#endif // R3D_MODULE_TEXTURE_H
