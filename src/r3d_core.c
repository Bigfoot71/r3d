/* r3d_core.h -- R3D Core Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_core.h>
#include <raymath.h>
#include <float.h>
#include <rlgl.h>
#include <glad.h>

#include "./r3d_core_state.h"

#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_driver.h"
#include "./modules/r3d_render.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_env.h"

// ========================================
// SHARED CORE STATE
// ========================================

struct r3d_core_state R3D;

// ========================================
// PUBLIC API
// ========================================

bool R3D_Init(int resWidth, int resHeight)
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

    R3D.aaMode = R3D_ANTI_ALIASING_MODE_NONE;
    R3D.aaPreset = R3D_ANTI_ALIASING_PRESET_MEDIUM;
    R3D.aspectMode = R3D_ASPECT_EXPAND;
    R3D.upscaleMode = R3D_UPSCALE_NEAREST;
    R3D.downscaleMode = R3D_DOWNSCALE_NEAREST;
    R3D.outputMode = R3D_OUTPUT_SCENE;

    R3D.textureFilter = TEXTURE_FILTER_TRILINEAR;
    R3D.colorSpace = R3D_COLORSPACE_SRGB;
    R3D.layers = R3D_LAYER_ALL;

    if (!r3d_texture_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init texture module"); return false; }
    if (!r3d_target_init(resWidth, resHeight)) { R3D_TRACELOG(LOG_ERROR, "Failed to init target module"); return false; }
    if (!r3d_shader_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init shader module"); return false; }
    if (!r3d_driver_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init driver module"); return false; }
    if (!r3d_render_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init render module"); return false; }
    if (!r3d_light_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init light module"); return false; }
    if (!r3d_env_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init env module"); return false; }

    R3D_TRACELOG(LOG_INFO, "Initialized successfully (%dx%d)", resWidth, resHeight);

    return true;
}

void R3D_Close(void)
{
    r3d_texture_quit();
    r3d_target_quit();
    r3d_shader_quit();
    r3d_driver_quit();
    r3d_render_quit();
    r3d_light_quit();
    r3d_env_quit();
}

void R3D_GetResolution(int* width, int* height)
{
    r3d_target_get_resolution(width, height, R3D_TARGET_SCENE_0, 0);
}

void R3D_UpdateResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        R3D_TRACELOG(LOG_ERROR, "Invalid resolution given to 'R3D_UpdateResolution'");
        return;
    }

    r3d_target_resize(width, height);
}

R3D_AntiAliasingMode R3D_GetAntiAliasingMode(void)
{
    return R3D.aaMode;
}

void R3D_SetAntiAliasingMode(R3D_AntiAliasingMode mode)
{
    R3D.aaMode = mode;
}

R3D_AntiAliasingPreset R3D_GetAntiAliasingPreset(void)
{
    return R3D.aaPreset;
}

void R3D_SetAntiAliasingPreset(R3D_AntiAliasingPreset preset)
{
    R3D.aaPreset = CLAMP(preset, 0, R3D_ANTI_ALIASING_PRESET_COUNT - 1);
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
    BIT_SET(R3D.layers, bitfield);
}

void R3D_DisableLayers(R3D_Layer bitfield)
{
    BIT_CLEAR(R3D.layers, bitfield);
}
