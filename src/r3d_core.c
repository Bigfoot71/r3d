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

#include "./details/containers/r3d_registry.h"
#include "./details/containers/r3d_array.h"
#include "./details/r3d_primitives.h"
#include "./details/r3d_drawcall.h"
#include "./details/r3d_light.h"

#include "./modules/r3d_shader.h"
#include "./r3d_state.h"

// ========================================
// PUBLIC API
// ========================================

void R3D_Init(int resWidth, int resHeight, unsigned int flags)
{
    // Set parameter flags
    R3D.state.flags = flags;

    // Check GPU supports
    r3d_supports_check();

    // Load draw call arrays
    R3D.container.aDrawForward = r3d_array_create(128, sizeof(r3d_drawcall_t));
    R3D.container.aDrawDeferred = r3d_array_create(128, sizeof(r3d_drawcall_t));
    R3D.container.aDrawForwardInst = r3d_array_create(8, sizeof(r3d_drawcall_t));
    R3D.container.aDrawDeferredInst = r3d_array_create(8, sizeof(r3d_drawcall_t));
    R3D.container.aDrawDecals = r3d_array_create(128, sizeof(r3d_drawcall_t));
    R3D.container.aDrawDecalsInst = r3d_array_create(128, sizeof(r3d_drawcall_t));

    // Load lights registry
    R3D.container.rLights = r3d_registry_create(8, sizeof(r3d_light_t));
    R3D.container.aLightBatch = r3d_array_create(8, sizeof(r3d_light_batched_t));

    // Environment data
    R3D.env.backgroundColor = (Vector4) { 0.2f, 0.2f, 0.2f, 1.0f };
    R3D.env.ambientColor = (Color){ 0, 0, 0, 255 };
    R3D.env.ambientEnergy = 1.0f;
    R3D.env.ambientLight = (Vector3){ 0.0f, 0.0f, 0.0f };
    R3D.env.quatSky = QuaternionIdentity();
    R3D.env.useSky = false;
    R3D.env.skyBackgroundIntensity = 1.0f;
    R3D.env.skyAmbientIntensity = 1.0f;
    R3D.env.skyReflectIntensity = 1.0f;
    R3D.env.ssaoEnabled = false;
    R3D.env.ssaoRadius = 0.5f;
    R3D.env.ssaoBias = 0.025f;
    R3D.env.ssaoIterations = 1;
    R3D.env.ssaoIntensity = 1.0f;
    R3D.env.ssaoPower = 1.0f;
    R3D.env.ssaoLightAffect = 0.0f;
    R3D.env.bloomMode = R3D_BLOOM_DISABLED;
    R3D.env.bloomLevels = 7;
    R3D.env.bloomIntensity = 0.05f;
    R3D.env.bloomFilterRadius = 0;
    R3D.env.bloomThreshold = 0.0f;
    R3D.env.bloomSoftThreshold = 0.5f;
    R3D.env.fogMode = R3D_FOG_DISABLED;
    R3D.env.ssrEnabled = false;
    R3D.env.ssrMaxRaySteps = 64;
    R3D.env.ssrBinarySearchSteps = 8;
    R3D.env.ssrRayMarchLength = 8.0f;
    R3D.env.ssrDepthThickness = 0.2f;
    R3D.env.ssrDepthTolerance = 0.005f;
    R3D.env.ssrEdgeFadeStart = 0.7f;
    R3D.env.ssrEdgeFadeEnd = 1.0f;
    R3D.env.fogColor = (Vector3) { 1.0f, 1.0f, 1.0f };
    R3D.env.fogStart = 1.0f;
    R3D.env.fogEnd = 50.0f;
    R3D.env.fogDensity = 0.05f;
    R3D.env.fogSkyAffect = 0.5f;
    R3D.env.dofMode = R3D_DOF_DISABLED;
    R3D.env.dofFocusPoint = 10.0f;
    R3D.env.dofFocusScale = 1.0f;
    R3D.env.dofMaxBlurSize = 20.0f;
    R3D.env.dofDebugMode = false;
    R3D.env.tonemapMode = R3D_TONEMAP_LINEAR;
    R3D.env.tonemapExposure = 1.0f;
    R3D.env.tonemapWhite = 1.0f;
    R3D.env.brightness = 1.0f;
    R3D.env.contrast = 1.0f;
    R3D.env.saturation = 1.0f;

    // Init resolution state
    R3D.state.resolution.width = resWidth;
    R3D.state.resolution.height = resHeight;
    R3D.state.resolution.texel.x = 1.0f / resWidth;
    R3D.state.resolution.texel.y = 1.0f / resHeight;
    R3D.state.resolution.maxLevel = 1 + (int)floor(log2((float)fmax(resWidth, resHeight)));

    // Init scene data
    R3D.state.scene.bounds = (BoundingBox) {
        (Vector3) { -100, -100, -100 },
        (Vector3) {  100,  100,  100 }
    };

    // Init default loading parameters
    R3D.state.loading.textureFilter = TEXTURE_FILTER_TRILINEAR;

    // Init default rendering layers
    R3D.state.layers = R3D_LAYER_ALL;

    // Load primitive shapes
    glGenVertexArrays(1, &R3D.primitive.dummyVAO);
    R3D.primitive.quad = r3d_primitive_load_quad();
    R3D.primitive.cube = r3d_primitive_load_cube();

    // Init misc data
    R3D.misc.matCubeViews[0] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  1.0f,  0.0f,  0.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });
    R3D.misc.matCubeViews[1] = MatrixLookAt((Vector3) { 0 }, (Vector3) { -1.0f,  0.0f,  0.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });
    R3D.misc.matCubeViews[2] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f,  1.0f,  0.0f }, (Vector3) { 0.0f,  0.0f,  1.0f });
    R3D.misc.matCubeViews[3] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f, -1.0f,  0.0f }, (Vector3) { 0.0f,  0.0f, -1.0f });
    R3D.misc.matCubeViews[4] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f,  0.0f,  1.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });
    R3D.misc.matCubeViews[5] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f,  0.0f, -1.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });

    R3D.misc.meshDecalBounds = R3D_GenMeshCube(1.0f, 1.0f, 1.0f);
    R3D.misc.meshDecalBounds.shadowCastMode = R3D_SHADOW_CAST_DISABLED;

    // Load GL Objects - framebuffers, textures, shaders...
    // NOTE: The initialization of these resources is based
    //       on the global state and should be performed last.
    r3d_framebuffers_load(resWidth, resHeight);
    r3d_textures_load();
    r3d_storages_load();
    r3d_mod_shader_init();

    // Defines suitable clipping plane distances for r3d
    rlSetClipPlanes(0.05f, 4000.0f);
}

void R3D_Close(void)
{
    r3d_framebuffers_unload();
    r3d_textures_unload();
    r3d_storages_unload();
    r3d_mod_shader_quit();

    r3d_array_destroy(&R3D.container.aDrawForward);
    r3d_array_destroy(&R3D.container.aDrawDeferred);
    r3d_array_destroy(&R3D.container.aDrawForwardInst);
    r3d_array_destroy(&R3D.container.aDrawDeferredInst);
    r3d_array_destroy(&R3D.container.aDrawDecals);
    r3d_array_destroy(&R3D.container.aDrawDecalsInst);

    r3d_registry_destroy(&R3D.container.rLights);
    r3d_array_destroy(&R3D.container.aLightBatch);

    glDeleteVertexArrays(1, &R3D.primitive.dummyVAO);
    r3d_primitive_unload(&R3D.primitive.quad);
    r3d_primitive_unload(&R3D.primitive.cube);

    R3D_UnloadMesh(&R3D.misc.meshDecalBounds);
}

bool R3D_HasState(unsigned int flag)
{
    return R3D.state.flags & flag;
}

void R3D_SetState(unsigned int flags)
{
    if (flags & R3D_FLAG_8_BIT_NORMALS) {
        TraceLog(LOG_WARNING, "R3D: Cannot set 'R3D_FLAG_8_BIT_NORMALS'; this flag must be set during R3D initialization");
        flags &= ~R3D_FLAG_8_BIT_NORMALS;
    }

    if (flags & R3D_FLAG_LOW_PRECISION_BUFFERS) {
        TraceLog(LOG_WARNING, "R3D: Cannot set 'R3D_FLAG_LOW_PRECISION_BUFFERS'; this flag must be set during R3D initialization");
        flags &= ~R3D_FLAG_LOW_PRECISION_BUFFERS;
    }

    R3D.state.flags |= flags;
}

void R3D_ClearState(unsigned int flags)
{
    if (flags & R3D_FLAG_8_BIT_NORMALS) {
        TraceLog(LOG_WARNING, "R3D: Cannot clear 'R3D_FLAG_8_BIT_NORMALS'; this flag must be set during R3D initialization");
        flags &= ~R3D_FLAG_8_BIT_NORMALS;
    }

    if (flags & R3D_FLAG_LOW_PRECISION_BUFFERS) {
        TraceLog(LOG_WARNING, "R3D: Cannot clear 'R3D_FLAG_LOW_PRECISION_BUFFERS'; this flag must be set during R3D initialization");
        flags &= ~R3D_FLAG_LOW_PRECISION_BUFFERS;
    }

    R3D.state.flags &= ~flags;
}

void R3D_GetResolution(int* width, int* height)
{
    if (width) *width = R3D.state.resolution.width;
    if (height) *height = R3D.state.resolution.height;
}

void R3D_UpdateResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        TraceLog(LOG_ERROR, "R3D: Invalid resolution given to 'R3D_UpdateResolution'");
        return;
    }

    if (width == R3D.state.resolution.width && height == R3D.state.resolution.height) {
        return;
    }

    r3d_framebuffers_unload();
    r3d_framebuffers_load(width, height);

    R3D.state.resolution.width = width;
    R3D.state.resolution.height = height;
    R3D.state.resolution.texel.x = 1.0f / width;
    R3D.state.resolution.texel.y = 1.0f / height;
    R3D.state.resolution.maxLevel = 1 + (int)floor(log2((float)fmax(width, height)));
}

void R3D_SetSceneBounds(BoundingBox sceneBounds)
{
    R3D.state.scene.bounds = sceneBounds;
}

void R3D_SetTextureFilter(TextureFilter filter)
{
    R3D.state.loading.textureFilter = filter;
}

R3D_Layer R3D_GetActiveLayers(void)
{
    return R3D.state.layers;
}

void R3D_SetActiveLayers(R3D_Layer layers)
{
    R3D.state.layers = layers;
}

void R3D_EnableLayers(R3D_Layer bitfield)
{
    R3D.state.layers |= bitfield;
}

void R3D_DisableLayers(R3D_Layer bitfield)
{
    R3D.state.layers &= ~bitfield;
}
