/* r3d_env.h -- Internal R3D environment module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_ENV_H
#define R3D_MODULE_ENV_H

#include <raylib.h>
#include <glad.h>

// ========================================
// CONSTANTS
// ========================================

#define R3D_ENV_IRRADIANCE_SIZE     32
#define R3D_ENV_PREFILTER_SIZE      128
#define R3D_ENV_PREFILTER_MIPS      8       //< 1 + (int)floor(log2(R3D_ENV_PREFILTER_SIZE))
#define R3D_ENV_MAP_INTIIAL_CAP     4

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_env_init(void);

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_env_quit(void);

/*
 * Requests a new irradiance map layer.
 * Returns the layer index, or -1 on failure.
 */
int r3d_env_reserve_irradiance_layer(void);

/*
 * Releases a previously reserved irradiance map layer.
 * After this call, the layer can be reused.
 */
void r3d_env_release_irradiance_layer(int layer);

/*
 * Requests a new prefilter map layer.
 * Returns the layer index, or -1 on failure.
 */
int r3d_env_reserve_prefilter_layer(void);

/*
 * Releases a previously reserved prefilter map layer.
 * After this call, the layer can be reused.
 */
void r3d_env_release_prefilter_layer(int layer);

/*
 * Binds the irradiance framebuffer for the given layer and cubemap face.
 * Automatically sets the appropriate viewport.
 */
void r3d_env_bind_irradiance_fbo(int layer, int face);

/*
 * Binds the prefilter framebuffer for the given layer, cubemap face,
 * and mip level. Automatically sets the appropriate viewport.
 */
void r3d_env_bind_prefilter_fbo(int layer, int face, int mipLevel);

/*
 * Returns the OpenGL ID of the irradiance cubemap array.
 */
GLuint r3d_env_get_irradiance_map(void);

/*
 * Returns the OpenGL ID of the prefiltered cubemap array.
 */
GLuint r3d_env_get_prefilter_map(void);

#endif // R3D_MODULE_ENV_H
