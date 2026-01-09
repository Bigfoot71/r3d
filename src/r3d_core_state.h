/* r3d_core_state.h -- Internal R3D Core State
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_CORE_STATE_H
#define R3D_CORE_STATE_H

#include <r3d/r3d_environment.h>
#include <r3d/r3d_material.h>
#include <r3d/r3d_core.h>
#include <raylib.h>

#include "./common/r3d_frustum.h"

// ========================================
// CORE MACROS
// ========================================

/*
 * Check if all specified flags are set.
 */
#define R3D_CORE_FLAGS_HAS(flags, mask)         \
    (((R3D.flags) & (mask)) == (mask))

/*
 * Set specified flags (bitwise OR).
 */
#define R3D_CORE_FLAGS_ASSIGN(flags, mask) do { \
    (R3D.flags) |= (mask);                      \
} while (0)

/*
 * Clear specified flags (bitwise AND NOT).
 */
#define R3D_CORE_FLAGS_CLEAR(flags, mask) do {  \
    (R3D.flags) &= ~(mask);                     \
} while (0)

// ========================================
// CORE STATE
// ========================================

/*
 * Current view state including view frustum and transforms.
 */
typedef struct {
    r3d_frustum_t frustum;  //< View frustum for culling
    Vector3 viewPosition;   //< Camera position in world space
    Matrix view, invView;   //< View matrix and its inverse
    Matrix proj, invProj;   //< Projection matrix and its inverse
    Matrix viewProj;        //< Combined view-projection matrix
    float aspect;           //< Projection aspect
    float near;             //< Near cull distance
    float far;              //< Far cull distance
} r3d_core_view_t;

/*
 * Core state shared between all public modules.
 */
extern struct r3d_core_state {
    RenderTexture screen;
    R3D_Environment environment;        //< Current environment settings
    R3D_Material material;              //< Default material to use
    r3d_core_view_t viewState;          //< Current view state
    R3D_AntiAliasing antiAliasing;      //< Defines how the aspect ratio is calculated
    R3D_AspectMode aspectMode;          //< Defines how the aspect ratio is calculated
    R3D_UpscaleMode upscaleMode;        //< Upscaling mode used during the final blit
    R3D_DownscaleMode downscaleMode;    //< Downscaling mode used during the final blit
    R3D_OutputMode outputMode;          //< Defines which buffer we should output in R3D_End()
    TextureFilter textureFilter;        //< Default texture filter for model loading
    R3D_ColorSpace colorSpace;          //< Color space that must be considered for supplied colors or surface colors
    Matrix matCubeViews[6];             //< Pre-computed view matrices for cubemap faces
    R3D_Layer layers;                   //< Active rendering layers
    R3D_Flags state;                    //< Renderer state flags
} R3D;

#endif // R3D_CORE_STATE_H
