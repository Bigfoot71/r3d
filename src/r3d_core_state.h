/* r3d_core_state.h -- Internal R3D Core State
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_CORE_STATE_H
#define R3D_CORE_STATE_H

#include <r3d/r3d_screen_shader.h>
#include <r3d/r3d_environment.h>
#include <r3d/r3d_material.h>
#include <r3d/r3d_core.h>
#include <r3d_config.h>
#include <raylib.h>

#include "./common/r3d_frustum.h"

// ========================================
// CORE STATE
// ========================================

/*
 * Current view state including view frustum and transforms.
 */
typedef struct {
    r3d_frustum_t frustum;  //< View frustum for culling
    Vector3 position;       //< Camera position in world space
    Matrix view, invView;   //< View matrix and its inverse
    Matrix proj, invProj;   //< Projection matrix and its inverse
    Matrix viewProj;        //< Combined view-projection matrix
    int projMode;           //< CameraProjection from raylib
    float aspect;           //< Projection aspect
    float near;             //< Near cull distance
    float far;              //< Far cull distance
} r3d_core_view_t;

/*
 * Core state shared between all public modules.
 */
extern struct r3d_core_state {
    RenderTexture screen;               //< Texture target (screen if null id)
    R3D_ScreenShader* screenShaders[R3D_MAX_SCREEN_SHADERS]; //< Chain of screen shaders
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
} R3D;

#endif // R3D_CORE_STATE_H
