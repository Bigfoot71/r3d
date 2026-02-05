/* r3d_driver.h -- Internal R3D OpenGL cache module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_OPENGL_H
#define R3D_MODULE_OPENGL_H

#include <r3d/r3d_material.h>
#include <r3d/r3d_mesh.h>
#include <raylib.h>
#include <glad.h>

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_driver_init(void);
void r3d_driver_quit(void);

bool r3d_driver_check_ext(const char* name);
bool r3d_driver_has_anisotropy(float* max);

void r3d_driver_clear_errors(void);
bool r3d_driver_check_error(const char* msg);

void r3d_driver_enable(GLenum state);
void r3d_driver_disable(GLenum state);

/*
 * Applies the given stencil state.
 * Assumes that GL_STENCIL_TEST is already enabled.
 */
void r3d_driver_set_stencil(R3D_StencilState state);

/*
 * Applies the given blend mode.
 * Assumes that GL_BLEND is already enabled.
 * Some modes like MIX or ADD behave differently depending on the transparency mode.
 */
void r3d_driver_set_blend(R3D_BlendMode blend, R3D_TransparencyMode transparency);

/*
 * Applies the given depth state.
 * Assumes that GL_DEPTH_TEST is already enabled.
 */
void r3d_driver_set_depth(R3D_CompareMode mode);

/*
 * Applies the given cull mode.
 * Automatically enables or disables GL_CULL_FACE.
 */
void r3d_driver_set_cull(R3D_CullMode mode);

/*
 * Applies the given cull mode depending on shadow casting mode.
 * Automatically enables or disables GL_CULL_FACE.
 */
void r3d_driver_set_cull_shadow(R3D_ShadowCastMode castMode, R3D_CullMode cullMode);

void r3d_driver_invalidate(void);

#endif // R3D_MODULE_OPENGL_H
