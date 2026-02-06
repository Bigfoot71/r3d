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
#include <stdint.h>
#include <glad.h>

// ========================================
// MODULE FUNCTIONS
// ========================================


/* Initialize module (called once during R3D_Init) */
bool r3d_driver_init(void);

/* Deinitialize module (called once during R3D_Close) */
void r3d_driver_quit(void);

/*
 * Checks if an OpenGL extension is supported. Results are cached.
 */
bool r3d_driver_check_ext(const char* name);

/*
 * Checks if anisotropic filtering is supported.
 * Optionally returns the max level.
 */
bool r3d_driver_has_anisotropy(float* max);

/*
 * Clears all pending OpenGL errors.
 */
void r3d_driver_clear_errors(void);

/*
 * Checks for OpenGL errors and logs them if present.
 * Returns true if an error occurred.
 */
bool r3d_driver_check_error(const char* msg);

/*
 * Enables an OpenGL capability.
 * Cached to avoid redundant state changes.
 */
void r3d_driver_enable(GLenum state);

/*
 * Disables an OpenGL capability.
 * Cached to avoid redundant state changes.
 */
void r3d_driver_disable(GLenum state);

/*
 * Sets which faces to cull (GL_FRONT, GL_BACK, GL_FRONT_AND_BACK).
 */
void r3d_driver_set_cull_face(GLenum face);

/*
 * Sets the depth comparison function.
 */
void r3d_driver_set_depth_func(GLenum func);

/*
 * Enables or disables writing to the depth buffer.
 */
void r3d_driver_set_depth_mask(GLboolean mask);

/*
 * Sets the depth range mapping.
 */
void r3d_driver_set_depth_range(float dNear, float dFar);

/*
 * Sets the depth polygon offset.
 * Automatically enables or disables GL_POLYGON_OFFSET_XXX.
 */
void r3d_driver_set_depth_offset(float units, float factor);

/*
 * Sets the stencil test function, reference value, and mask.
 */
void r3d_driver_set_stencil_func(GLenum func, uint8_t ref, uint8_t mask);

/*
 * Sets stencil operations for different test outcomes.
 */
void r3d_driver_set_stencil_op(GLenum fail, GLenum zFail, GLenum zPass);

/*
 * Sets the blend equation and blend factors.
 */
void r3d_driver_set_blend_func(GLenum equation, GLenum srcFactor, GLenum dstFactor);

/*
 * Applies the given depth state.
 * Assumes that GL_DEPTH_TEST is already enabled.
 * Automatically enables or disables GL_POLYGON_OFFSET_XXX.
 */
void r3d_driver_set_depth_state(R3D_DepthState state);

/*
 * Applies the given stencil state.
 * Assumes that GL_STENCIL_TEST is already enabled.
 */
void r3d_driver_set_stencil_state(R3D_StencilState state);

/*
 * Applies the given blend mode.
 * Assumes that GL_BLEND is already enabled.
 * Some modes like MIX or ADD behave differently depending on the transparency mode.
 */
void r3d_driver_set_blend_mode(R3D_BlendMode blend, R3D_TransparencyMode transparency);

/*
 * Applies the given cull mode.
 * Automatically enables or disables GL_CULL_FACE.
 */
void r3d_driver_set_cull_mode(R3D_CullMode mode);

/*
 * Applies the given cull mode depending on shadow casting mode.
 * Automatically enables or disables GL_CULL_FACE.
 */
void r3d_driver_set_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode);

/*
 * Invalidates the entire pipeline cache.
 */
void r3d_driver_invalidate(void);

#endif // R3D_MODULE_OPENGL_H
