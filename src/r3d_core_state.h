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
#include <r3d/r3d_frustum.h>
#include <r3d/r3d_camera.h>
#include <r3d/r3d_core.h>
#include <r3d_config.h>
#include <raylib.h>

#include "./common/r3d_stack.h"

// ========================================
// HELPER MACROS
// ========================================

#define R3D_HINT(hint) (R3D.hints[hint].value)

// ========================================
// CORE STATE
// ========================================

/*
 * Storage for a single hint value.
 */
typedef struct {
    int value;      //< Effective value (user-provided if overridden, default otherwise)
    bool override;  //< True if explicitly set by the user via R3D_SetHint()
} r3d_hint_t;

/*
 * Current view state including view frustum and transforms.
 */
typedef struct {
    R3D_Camera camera;      //< Complete camera data
    Rectangle viewport;     //< Viewport of the pass
    R3D_Frustum frustum;    //< View frustum for culling
    Matrix view, invView;   //< View matrix and its inverse
    Matrix proj, invProj;   //< Projection matrix and its inverse
    Matrix viewProj;        //< Combined view-projection matrix
    double aspect;          //< Camera aspect (based on target)
} r3d_core_view_t;

/*
 * Core state shared between all public modules.
 */
extern struct r3d_core_state {
    RenderTexture screen;               //< Texture target (screen if null id)
    R3D_ScreenShader* screenShaders     //< Chain of screen shaders
        [R3D_SCREEN_SHADER_STAGE_COUNT]
        [R3D_MAX_SCREEN_SHADERS];
    R3D_Environment environment;        //< Current environment settings
    R3D_Material material;              //< Default material to use
    r3d_core_view_t viewState;          //< Current view state
    R3D_AntiAliasingMode aaMode;        //< Defines the anti aliasing mode
    R3D_AntiAliasingPreset aaPreset;    //< Defines the anti aliasing quality preset
    R3D_AspectMode aspectMode;          //< Defines how the aspect ratio is calculated
    R3D_UpscaleMode upscaleMode;        //< Upscaling mode used during the final blit
    R3D_DownscaleMode downscaleMode;    //< Downscaling mode used during the final blit
    R3D_OutputMode outputMode;          //< Defines which buffer we should output in R3D_End()
    TextureFilter textureFilter;        //< Default texture filter for model loading
    TextureWrap textureWrap;            //< Default texture wrap for material map loading
    R3D_ColorSpace colorSpace;          //< Color space that must be considered for supplied colors or surface colors
    Matrix matCubeViews[6];             //< Pre-computed view matrices for cubemap faces
    r3d_hint_t hints[R3D_HINT_COUNT];   //< User-configurable hints, resolved at R3D_Init()
    r3d_stack_t* stack;                 //< Main thread stack allocator
    bool initialized;                   //< Indicates if R3D has been initialized successfully
} R3D;

#endif // R3D_CORE_STATE_H
