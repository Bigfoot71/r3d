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
#include <glad.h>

#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_driver.h"
#include "./modules/r3d_render.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_env.h"
#include "./r3d_core_state.h"
#include "common/r3d_stack.h"

// ========================================
// CONSTANTS
// ========================================

static const int R3D_HINT_DEFAULTS[R3D_HINT_COUNT] = {
    [R3D_HINT_MESH_VERTEX_BUFFER_CAPACITY] = 65536,
    [R3D_HINT_MESH_INDEX_BUFFER_CAPACITY]  = 131072,
    [R3D_HINT_MESH_STREAMING_CAPACITY]     = 128,
    [R3D_HINT_DRAW_CALL_CAPACITY]          = 1024,
    [R3D_HINT_FORWARD_LIGHT_PER_MESH]      = 16,
    [R3D_HINT_PROBE_MAX_ACTIVE]            = 8,
    [R3D_HINT_PROBE_CAPTURE_SIZE]          = 256,
    [R3D_HINT_SHADOW_DIR_SIZE]             = 4096,
    [R3D_HINT_SHADOW_SPOT_SIZE]            = 2048,
    [R3D_HINT_SHADOW_OMNI_SIZE]            = 2048,
    [R3D_HINT_IBL_IRRADIANCE_SIZE]         = 32,
    [R3D_HINT_IBL_PREFILTER_SIZE]          = 128,
};

// ========================================
// SHARED CORE STATE
// ========================================

struct r3d_core_state R3D = {0};

// ========================================
// PUBLIC API
// ========================================

void R3D_SetHint(R3D_Hint hint, int value)
{
    if (R3D.initialized || hint < 0 || hint > R3D_HINT_COUNT) return;

    const int MIN_BUFFER_SZ   = 8;
    const int MIN_DRAW_CALLS  = 1;
    const int MIN_FREE_SLOTS  = 1;
    const int MIN_TEXMAP_SIZE = 2;

    static bool maxTexSizeFetched = false;
    static GLint maxTexSize = 1024;

    if (!maxTexSizeFetched) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
        maxTexSizeFetched = true;
    }

    switch (hint) {
    case R3D_HINT_MESH_VERTEX_BUFFER_CAPACITY:
        value = MAX(value, MIN_BUFFER_SZ);
        break;
    case R3D_HINT_MESH_INDEX_BUFFER_CAPACITY:
        value = MAX(value, MIN_BUFFER_SZ);
        break;
    case R3D_HINT_MESH_STREAMING_CAPACITY:
        value = MAX(value, MIN_FREE_SLOTS);
        break;
    case R3D_HINT_DRAW_CALL_CAPACITY:
        value = MAX(value, MIN_DRAW_CALLS);
        break;
    case R3D_HINT_FORWARD_LIGHT_PER_MESH:
        value = CLAMP(value, 1, R3D_SHADER_LIGHT_FORWARD_UBO_CAP);
        break;
    case R3D_HINT_PROBE_MAX_ACTIVE:
        value = CLAMP(value, 1, R3D_SHADER_PROBE_UBO_CAP);
        break;
    case R3D_HINT_PROBE_CAPTURE_SIZE:
        value = CLAMP(value, MIN_TEXMAP_SIZE, maxTexSize);
        break;
    case R3D_HINT_SHADOW_DIR_SIZE:
        value = CLAMP(value, MIN_TEXMAP_SIZE, maxTexSize);
        break;
    case R3D_HINT_SHADOW_SPOT_SIZE:
        value = CLAMP(value, MIN_TEXMAP_SIZE, maxTexSize);
        break;
    case R3D_HINT_SHADOW_OMNI_SIZE:
        value = CLAMP(value, MIN_TEXMAP_SIZE, maxTexSize);
        break;
    case R3D_HINT_IBL_IRRADIANCE_SIZE:
        value = CLAMP(value, MIN_TEXMAP_SIZE, maxTexSize);
        break;
    case R3D_HINT_IBL_PREFILTER_SIZE:
        value = CLAMP(value, MIN_TEXMAP_SIZE, maxTexSize);
        break;
    case R3D_HINT_COUNT:
        break;
    }

    R3D.hints[hint] = (r3d_hint_t) {
        .value = value, .override = true,
    };
}

int R3D_GetHint(R3D_Hint hint)
{
    if (hint < 0 || hint > R3D_HINT_COUNT) return 0;
    r3d_hint_t h = R3D.hints[hint];

    if (!R3D.initialized && !h.override) {
        return R3D_HINT_DEFAULTS[hint];
    }
    return h.value;
}

bool R3D_Init(int resWidth, int resHeight)
{
    R3D.matCubeViews[0] = MatrixLookAt((Vector3) {0}, (Vector3) { 1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D.matCubeViews[1] = MatrixLookAt((Vector3) {0}, (Vector3) {-1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D.matCubeViews[2] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  1.0f,  0.0f}, (Vector3) {0.0f,  0.0f,  1.0f});
    R3D.matCubeViews[3] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f, -1.0f,  0.0f}, (Vector3) {0.0f,  0.0f, -1.0f});
    R3D.matCubeViews[4] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  0.0f,  1.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D.matCubeViews[5] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  0.0f, -1.0f}, (Vector3) {0.0f, -1.0f,  0.0f});

    R3D.environment = R3D_ENVIRONMENT_BASE;
    R3D.material = R3D_MATERIAL_BASE;

    R3D.viewState.camera = R3D_CAMERA_BASE;
    R3D.viewState.view = R3D_MATRIX_IDENTITY;
    R3D.viewState.invView = R3D_MATRIX_IDENTITY;
    R3D.viewState.proj = R3D_MATRIX_IDENTITY;
    R3D.viewState.invProj = R3D_MATRIX_IDENTITY;
    R3D.viewState.viewProj = R3D_MATRIX_IDENTITY;
    R3D.viewState.aspect = 1.0;

    R3D.aaMode = R3D_ANTI_ALIASING_MODE_NONE;
    R3D.aaPreset = R3D_ANTI_ALIASING_PRESET_MEDIUM;
    R3D.aspectMode = R3D_ASPECT_EXPAND;
    R3D.upscaleMode = R3D_UPSCALE_NEAREST;
    R3D.downscaleMode = R3D_DOWNSCALE_NEAREST;
    R3D.outputMode = R3D_OUTPUT_SCENE;

    R3D.textureFilter = TEXTURE_FILTER_TRILINEAR;
    R3D.textureWrap = TEXTURE_WRAP_CLAMP;
    R3D.colorSpace = R3D_COLORSPACE_SRGB;

    for (int i = 0; i < R3D_HINT_COUNT; i++) {
        if (!R3D.hints[i].override) {
            R3D.hints[i].value = R3D_HINT_DEFAULTS[i];
        }
    }

    R3D.stack = r3d_stack_create(1024 * 1024);
    if (R3D.stack == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to create internal stack allocator");
        return false;
    }

    if (!r3d_texture_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init texture module"); return false; }
    if (!r3d_target_init(resWidth, resHeight)) { R3D_TRACELOG(LOG_ERROR, "Failed to init target module"); return false; }
    if (!r3d_shader_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init shader module"); return false; }
    if (!r3d_driver_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init driver module"); return false; }
    if (!r3d_render_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init render module"); return false; }
    if (!r3d_light_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init light module"); return false; }
    if (!r3d_env_init()) { R3D_TRACELOG(LOG_ERROR, "Failed to init env module"); return false; }

    R3D.initialized = true;

    R3D_TRACELOG(LOG_INFO, "Initialized successfully (%dx%d)", resWidth, resHeight);

    return true;
}

void R3D_Close(void)
{
    r3d_stack_destroy(R3D.stack);
    r3d_texture_quit();
    r3d_target_quit();
    r3d_shader_quit();
    r3d_driver_quit();
    r3d_render_quit();
    r3d_light_quit();
    r3d_env_quit();

    memset(&R3D, 0, sizeof(R3D));
}

void R3D_GetResolution(int* width, int* height)
{
    if (width) *width = R3D_TARGET_SIZE_W;
    if (height) *height = R3D_TARGET_SIZE_H;
}

void R3D_SetResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        R3D_TRACELOG(LOG_ERROR, "Invalid resolution given to 'R3D_SetResolution'");
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

void R3D_SetTextureWrap(TextureWrap wrap)
{
    R3D.textureWrap = wrap;
}

void R3D_SetColorSpace(R3D_ColorSpace space)
{
    R3D.colorSpace = space;
}
