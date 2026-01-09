/* r3d_core.h -- R3D Core Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_core.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include <assimp/cimport.h>
#include <float.h>

#include "./r3d_core_state.h"

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_opengl.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_draw.h"
#include "./modules/r3d_env.h"

// ========================================
// SHARED CORE STATE
// ========================================

struct r3d_core_state R3D;

// ========================================
// PUBLIC API
// ========================================

void R3D_Init(int resWidth, int resHeight, R3D_Flags flags)
{
    memset(&R3D, 0, sizeof(R3D));

    R3D.matCubeViews[0] = MatrixLookAt((Vector3) {0}, (Vector3) { 1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D.matCubeViews[1] = MatrixLookAt((Vector3) {0}, (Vector3) {-1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D.matCubeViews[2] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  1.0f,  0.0f}, (Vector3) {0.0f,  0.0f,  1.0f});
    R3D.matCubeViews[3] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f, -1.0f,  0.0f}, (Vector3) {0.0f,  0.0f, -1.0f});
    R3D.matCubeViews[4] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  0.0f,  1.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D.matCubeViews[5] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  0.0f, -1.0f}, (Vector3) {0.0f, -1.0f,  0.0f});

    R3D.environment = R3D_ENVIRONMENT_BASE;
    R3D.material = R3D_MATERIAL_BASE;

    R3D.antiAliasing = R3D_ANTI_ALIASING_DISABLED;
    R3D.aspectMode = R3D_ASPECT_EXPAND;
    R3D.upscaleMode = R3D_UPSCALE_NEAREST;
    R3D.outputMode = R3D_OUTPUT_SCENE;

    R3D.textureFilter = TEXTURE_FILTER_TRILINEAR;
    R3D.colorSpace = R3D_COLORSPACE_SRGB;
    R3D.layers = R3D_LAYER_ALL;
    R3D.state = flags;

    r3d_primitive_init();
    r3d_texture_init();
    r3d_target_init(resWidth, resHeight);
    r3d_shader_init();
    r3d_opengl_init();
    r3d_light_init();
    r3d_draw_init();
    r3d_env_init();

    // Defines suitable clipping plane distances for r3d
    rlSetClipPlanes(0.05f, 4000.0f);
}

void R3D_Close(void)
{
    r3d_primitive_quit();
    r3d_texture_quit();
    r3d_target_quit();
    r3d_shader_quit();
    r3d_opengl_quit();
    r3d_light_quit();
    r3d_draw_quit();
    r3d_env_quit();
}

bool R3D_HasState(R3D_Flags flags)
{
    return R3D_CORE_FLAGS_HAS(state, flags);
}

void R3D_SetState(R3D_Flags flags)
{
    R3D_CORE_FLAGS_ASSIGN(state, flags);
}

void R3D_ClearState(R3D_Flags flags)
{
    R3D_CORE_FLAGS_CLEAR(state, flags);
}

void R3D_GetResolution(int* width, int* height)
{
    r3d_target_get_resolution(width, height, R3D_TARGET_SCENE_0, 0);
}

void R3D_UpdateResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        TraceLog(LOG_ERROR, "R3D: Invalid resolution given to 'R3D_UpdateResolution'");
        return;
    }

    r3d_target_resize(width, height);
}

R3D_AntiAliasing R3D_GetAntiAliasing(void)
{
    return R3D.antiAliasing;
}

void R3D_SetAntiAliasing(R3D_AntiAliasing mode)
{
    R3D.antiAliasing = mode;
}

R3D_AspectMode R3D_GetAspectMode(void)
{
    return R3D.aspectMode;
}

void R3D_SetAspectMode(R3D_AspectMode mode)
{
    R3D.aspectMode = mode;
}

R3D_UpscaleMode R3D_GetUpscaleMode(void)
{
    return R3D.upscaleMode;
}

void R3D_SetUpscaleMode(R3D_UpscaleMode mode)
{
    R3D.upscaleMode = mode;
}

R3D_DownscaleMode R3D_GetDownscaleMode(void)
{
    return R3D.downscaleMode;
}

void R3D_SetDownscaleMode(R3D_DownscaleMode mode)
{
    R3D.downscaleMode = mode;
}

R3D_OutputMode R3D_GetOutputMode(void)
{
    return R3D.outputMode;
}

void R3D_SetOutputMode(R3D_OutputMode mode)
{
    R3D.outputMode = mode;
}

void R3D_SetTextureFilter(TextureFilter filter)
{
    R3D.textureFilter = filter;
}

void R3D_SetColorSpace(R3D_ColorSpace space)
{
    R3D.colorSpace = space;
}

R3D_Layer R3D_GetActiveLayers(void)
{
    return R3D.layers;
}

void R3D_SetActiveLayers(R3D_Layer bitfield)
{
    R3D.layers = bitfield;
}

void R3D_EnableLayers(R3D_Layer bitfield)
{
    R3D_CORE_FLAGS_ASSIGN(layers, bitfield);
}

void R3D_DisableLayers(R3D_Layer bitfield)
{
    R3D_CORE_FLAGS_CLEAR(layers, bitfield);
}
