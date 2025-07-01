/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#include "r3d.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include <float.h>

#include "./r3d_state.h"
#include "./details/r3d_light.h"
#include "./details/r3d_drawcall.h"
#include "./details/r3d_billboard.h"
#include "./details/r3d_collision.h"
#include "./details/r3d_primitives.h"
#include "./details/r3d_projection.h"
#include "./details/containers/r3d_array.h"
#include "./details/containers/r3d_registry.h"

/* === Internal declarations === */

static bool r3d_has_deferred_calls(void);
static bool r3d_has_forward_calls(void);

static void r3d_sprite_get_uv_scale_offset(const R3D_Sprite* sprite, Vector2* uvScale, Vector2* uvOffset, float sgnX, float sgnY);
static void r3d_shadow_apply_cast_mode(R3D_ShadowCastMode mode);

static R3D_RenderMode r3d_render_auto_detect_mode(const Material* material);
static void r3d_render_apply_blend_mode(R3D_BlendMode mode);

static void r3d_gbuffer_enable_stencil_write(void);
static void r3d_gbuffer_enable_stencil_test(bool passOnGeometry);
static void r3d_gbuffer_disable_stencil(void);

static void r3d_prepare_sort_drawcalls(void);
static void r3d_prepare_process_lights_and_batch(void);

static void r3d_pass_shadow_maps(void);
static void r3d_pass_gbuffer(void);
static void r3d_pass_ssao(void);

static void r3d_pass_deferred_ambient(void);
static void r3d_pass_deferred_lights(void);

static void r3d_pass_scene_background(void);
static void r3d_pass_scene_deferred(void);
static void r3d_pass_scene_forward_depth_prepass(void);
static void r3d_pass_scene_forward(void);

static void r3d_pass_post_init(unsigned int fb, unsigned srcAttach);
static void r3d_pass_post_bloom(void);
static void r3d_pass_post_fog(void);
static void r3d_pass_post_tonemap(void);
static void r3d_pass_post_adjustment(void);
static void r3d_pass_post_fxaa(void);

static void r3d_pass_final_blit(void);

static void r3d_reset_raylib_state(void);

/* === Public functions === */

void R3D_Init(int resWidth, int resHeight, unsigned int flags)
{
    // Set parameter flags
    R3D.state.flags = flags;

    // Check GPU supports
    {
        R3D.support.TEX_RG16F = r3d_check_texture_format_support(GL_RG16F);
        if (R3D.support.TEX_RG16F) TraceLog(LOG_INFO, "R3D: RG16F is supported");
        else TraceLog(LOG_WARNING, "R3D: RG16F is NOT supported");

        R3D.support.TEX_RGB16F = r3d_check_texture_format_support(GL_RGB16F);
        if (R3D.support.TEX_RGB16F) TraceLog(LOG_INFO, "R3D: RGB16F is supported");
        else TraceLog(LOG_WARNING, "R3D: RGB16F is NOT supported");

        R3D.support.TEX_RGB32F = r3d_check_texture_format_support(GL_RGB32F);
        if (R3D.support.TEX_RGB32F) TraceLog(LOG_INFO, "R3D: RGB32F is supported");
        else TraceLog(LOG_WARNING, "R3D: RGB32F is NOT supported");

        R3D.support.TEX_R11G11B10F = r3d_check_texture_format_support(GL_R11F_G11F_B10F);
        if (R3D.support.TEX_R11G11B10F) TraceLog(LOG_INFO, "R3D: R11F_G11F_B10F is supported");
        else TraceLog(LOG_WARNING, "R3D: R11F_G11F_B10F is NOT supported");
    }

    // Load draw call arrays
    R3D.container.aDrawForward = r3d_array_create(128, sizeof(r3d_drawcall_t));
    R3D.container.aDrawDeferred = r3d_array_create(128, sizeof(r3d_drawcall_t));
    R3D.container.aDrawForwardInst = r3d_array_create(8, sizeof(r3d_drawcall_t));
    R3D.container.aDrawDeferredInst = r3d_array_create(8, sizeof(r3d_drawcall_t));

    // Load lights registry
    R3D.container.rLights = r3d_registry_create(8, sizeof(r3d_light_t));
    R3D.container.aLightBatch = r3d_array_create(8, sizeof(r3d_light_batched_t));

    // Environment data
    R3D.env.backgroundColor = (Vector3) { 0.2f, 0.2f, 0.2f };
    R3D.env.ambientColor = (Vector3) { 0.2f, 0.2f, 0.2f };
    R3D.env.quatSky = QuaternionIdentity();
    R3D.env.useSky = false;
    R3D.env.ssaoEnabled = false;
    R3D.env.ssaoRadius = 0.5f;
    R3D.env.ssaoBias = 0.025f;
    R3D.env.ssaoIterations = 10;
    R3D.env.bloomMode = R3D_BLOOM_DISABLED;
    R3D.env.bloomIntensity = 0.05f;
    R3D.env.bloomFilterRadius = 0;
    R3D.env.bloomThreshold = 0.0f;
    R3D.env.bloomSoftThreshold = 0.5f;
    R3D.env.fogMode = R3D_FOG_DISABLED;
    R3D.env.fogColor = (Vector3) { 1.0f, 1.0f, 1.0f };
    R3D.env.fogStart = 1.0f;
    R3D.env.fogEnd = 50.0f;
    R3D.env.fogDensity = 0.05f;
    R3D.env.tonemapMode = R3D_TONEMAP_LINEAR;
    R3D.env.tonemapExposure = 1.0f;
    R3D.env.tonemapWhite = 1.0f;
    R3D.env.brightness = 1.0f;
    R3D.env.contrast = 1.0f;
    R3D.env.saturation = 1.0f;

    // Init resolution state
    R3D.state.resolution.width = resWidth;
    R3D.state.resolution.height = resHeight;
    R3D.state.resolution.texelX = 1.0f / resWidth;
    R3D.state.resolution.texelY = 1.0f / resHeight;

    // Init rendering mode configs
    R3D.state.render.mode = R3D_RENDER_AUTO_DETECT;
    R3D.state.render.blendMode = R3D_BLEND_ALPHA;
    R3D.state.render.shadowCastMode = R3D_SHADOW_CAST_FRONT_FACES;
    R3D.state.render.billboardMode = R3D_BILLBOARD_DISABLED;
    R3D.state.render.alphaScissorThreshold = 0.01f;

    // Init scene data
    R3D.state.scene.bounds = (BoundingBox) {
        (Vector3) { -100, -100, -100 },
        (Vector3) {  100,  100,  100 }
    };

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

    // Load GL Objects - framebuffers, textures, shaders...
    // NOTE: The initialization of these resources is based
    //       on the global state and should be performed last.
    r3d_framebuffers_load(resWidth, resHeight);
    r3d_textures_load();
    r3d_shaders_load();

    // Defines suitable clipping plane distances for r3d
    rlSetClipPlanes(0.05f, 4000.0f);
}

void R3D_Close(void)
{
    r3d_framebuffers_unload();
    r3d_textures_unload();
    r3d_shaders_unload();

    r3d_array_destroy(&R3D.container.aDrawForward);
    r3d_array_destroy(&R3D.container.aDrawDeferred);
    r3d_array_destroy(&R3D.container.aDrawForwardInst);
    r3d_array_destroy(&R3D.container.aDrawDeferredInst);

    r3d_registry_destroy(&R3D.container.rLights);
    r3d_array_destroy(&R3D.container.aLightBatch);

    glDeleteVertexArrays(1, &R3D.primitive.dummyVAO);
    r3d_primitive_unload(&R3D.primitive.quad);
    r3d_primitive_unload(&R3D.primitive.cube);
}

bool R3D_HasState(unsigned int flag)
{
    return R3D.state.flags & flag;
}

void R3D_SetState(unsigned int flags)
{
    if (flags & R3D_FLAG_8_BIT_NORMALS) {
        TraceLog(LOG_WARNING, "R3D: Cannot set 'R3D_FLAG_8_BIT_NORMALS'; this flag must be set before R3D initialization");
        flags &= ~R3D_FLAG_8_BIT_NORMALS;
    }

    R3D.state.flags |= flags;

    if (flags & R3D_FLAG_FXAA) {
        if (R3D.shader.screen.fxaa.id == 0) {
            r3d_shader_load_screen_fxaa();
        }
    }
}

void R3D_ClearState(unsigned int flags)
{
    if (flags & R3D_FLAG_8_BIT_NORMALS) {
        TraceLog(LOG_WARNING, "R3D: Cannot clear 'R3D_FLAG_8_BIT_NORMALS'; this flag must be set before R3D initialization");
        flags &= ~R3D_FLAG_8_BIT_NORMALS;
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
    R3D.state.resolution.texelX = 1.0f / width;
    R3D.state.resolution.texelY = 1.0f / height;
}

void R3D_SetRenderTarget(const RenderTexture* target)
{
    if (target == NULL) {
        memset(&R3D.framebuffer.customTarget, 0, sizeof(RenderTexture));
        return;
    }

    R3D.framebuffer.customTarget = *target;
}

void R3D_SetSceneBounds(BoundingBox sceneBounds)
{
    R3D.state.scene.bounds = sceneBounds;
}

void R3D_ApplyRenderMode(R3D_RenderMode mode)
{
    R3D.state.render.mode = mode;
}

void R3D_ApplyBlendMode(R3D_BlendMode mode)
{
    R3D.state.render.blendMode = mode;
}

void R3D_ApplyShadowCastMode(R3D_ShadowCastMode mode)
{
    R3D.state.render.shadowCastMode = mode;
}

void R3D_ApplyBillboardMode(R3D_BillboardMode mode)
{
    R3D.state.render.billboardMode = mode;
}

void R3D_ApplyAlphaScissorThreshold(float threshold)
{
    R3D.state.render.alphaScissorThreshold = threshold;
}

void R3D_Begin(Camera3D camera)
{
    // Render the batch before proceeding
    rlDrawRenderBatchActive();

    // Clear the previous draw call array state
    r3d_array_clear(&R3D.container.aDrawForward);
    r3d_array_clear(&R3D.container.aDrawDeferred);
    r3d_array_clear(&R3D.container.aDrawForwardInst);
    r3d_array_clear(&R3D.container.aDrawDeferredInst);

    // Store camera position
    R3D.state.transform.position = camera.position;

    // Compute aspect ratio
    float aspect = 1.0f;
    if (R3D.state.flags & R3D_FLAG_ASPECT_KEEP) {
        aspect = (float)R3D.state.resolution.width / R3D.state.resolution.height;
    }
    else {
        aspect = (float)GetScreenWidth() / GetScreenHeight();
    }

    // Compute projection matrix
    if (camera.projection == CAMERA_PERSPECTIVE) {
        double top = rlGetCullDistanceNear() * tan(camera.fovy * 0.5 * DEG2RAD);
        double right = top * aspect;
        R3D.state.transform.proj = MatrixFrustum(
            -right, right, -top, top,
            rlGetCullDistanceNear(),
            rlGetCullDistanceFar()
        );
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double top = camera.fovy / 2.0;
        double right = top * aspect;
        R3D.state.transform.proj = MatrixOrtho(
            -right, right, -top, top,
            rlGetCullDistanceNear(),
            rlGetCullDistanceFar()
        );
    }

    // Compute view matrix
    R3D.state.transform.view = MatrixLookAt(
        camera.position,
        camera.target,
        camera.up
    );

    // Store inverse matrices
    R3D.state.transform.invProj = MatrixInvert(R3D.state.transform.proj);
    R3D.state.transform.invView = MatrixInvert(R3D.state.transform.view);

    // Compute frustum
    Matrix matMV = MatrixMultiply(R3D.state.transform.view, R3D.state.transform.proj);
    R3D.state.frustum.aabb = r3d_frustum_get_bounding_box(matMV);
    R3D.state.frustum.shape = r3d_frustum_create(matMV);
}

void R3D_End(void)
{
    r3d_prepare_sort_drawcalls();
    r3d_prepare_process_lights_and_batch();

    r3d_pass_shadow_maps();

    if (r3d_has_deferred_calls()) {
        r3d_pass_gbuffer();
    }

    if (R3D.env.ssaoEnabled) {
        r3d_pass_ssao();
    }

    if (r3d_has_deferred_calls()) {
        r3d_pass_deferred_ambient();
        r3d_pass_deferred_lights();
    }

    r3d_pass_scene_background();

    if (r3d_has_deferred_calls()) {
        r3d_pass_scene_deferred();
    }

    if (r3d_has_forward_calls()) {
        if (R3D.state.flags & R3D_FLAG_DEPTH_PREPASS) {
            r3d_pass_scene_forward_depth_prepass();
        }
        r3d_pass_scene_forward();
    }

    r3d_pass_post_init(
        R3D.framebuffer.scene.id,
        GL_COLOR_ATTACHMENT0
    );

    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        r3d_pass_post_bloom();
    }

    if (R3D.env.fogMode != R3D_FOG_DISABLED) {
        r3d_pass_post_fog();
    }

    if (R3D.env.tonemapMode != R3D_TONEMAP_LINEAR || R3D.env.tonemapExposure != 1.0f) {
        r3d_pass_post_tonemap();
    }

    r3d_pass_post_adjustment();

    if (R3D.state.flags & R3D_FLAG_FXAA) {
        r3d_pass_post_fxaa();
    }

    r3d_pass_final_blit();

    r3d_reset_raylib_state();
}

void R3D_DrawMesh(Mesh mesh, Material material, Matrix transform)
{
    r3d_drawcall_t drawCall = { 0 };

    if (R3D.state.render.billboardMode == R3D_BILLBOARD_FRONT) {
        r3d_billboard_mode_front(&transform, &R3D.state.transform.invView);
    }
    else if (R3D.state.render.billboardMode == R3D_BILLBOARD_Y_AXIS) {
        r3d_billboard_mode_y(&transform, &R3D.state.transform.invView);
    }

    drawCall.transform = transform;
    drawCall.material = material;
    drawCall.geometry.mesh = mesh;
    drawCall.geometryType = R3D_DRAWCALL_GEOMETRY_MESH;
    drawCall.shadowCastMode = R3D.state.render.shadowCastMode;

    R3D_RenderMode mode = R3D.state.render.mode;

    if (mode == R3D_RENDER_AUTO_DETECT) {
        mode = r3d_render_auto_detect_mode(&material);
    }

    r3d_array_t* arr = &R3D.container.aDrawDeferred;

    if (mode == R3D_RENDER_FORWARD) {
        drawCall.forward.alphaScissorThreshold = R3D.state.render.alphaScissorThreshold;
        drawCall.forward.blendMode = R3D.state.render.blendMode;
        arr = &R3D.container.aDrawForward;
    }

    r3d_array_push_back(arr, &drawCall);
}

void R3D_DrawMeshInstanced(Mesh mesh, Material material, Matrix* instanceTransforms, int instanceCount)
{
    R3D_DrawMeshInstancedPro(mesh, material, MatrixIdentity(), instanceTransforms, 0, NULL, 0, instanceCount);
}

void R3D_DrawMeshInstancedEx(Mesh mesh, Material material, Matrix* instanceTransforms, Color* instanceColors, int instanceCount)
{
    R3D_DrawMeshInstancedPro(mesh, material, MatrixIdentity(), instanceTransforms, 0, instanceColors, 0, instanceCount);
}

void R3D_DrawMeshInstancedPro(Mesh mesh, Material material, Matrix transform,
                              Matrix* instanceTransforms, int transformsStride,
                              Color* instanceColors, int colorsStride,
                              int instanceCount)
{
    r3d_drawcall_t drawCall = { 0 };

    if (instanceCount == 0 || instanceTransforms == NULL) {
        return;
    }

    drawCall.transform = transform;
    drawCall.material = material;
    drawCall.geometry.mesh = mesh;
    drawCall.geometryType = R3D_DRAWCALL_GEOMETRY_MESH;
    drawCall.shadowCastMode = R3D.state.render.shadowCastMode;

    drawCall.instanced.billboardMode = R3D.state.render.billboardMode;
    drawCall.instanced.transforms = instanceTransforms;
    drawCall.instanced.transStride = transformsStride;
    drawCall.instanced.colStride = colorsStride;
    drawCall.instanced.colors = instanceColors;
    drawCall.instanced.count = instanceCount;

    R3D_RenderMode mode = R3D.state.render.mode;

    if (mode == R3D_RENDER_AUTO_DETECT) {
        mode = r3d_render_auto_detect_mode(&material);
    }

    r3d_array_t* arr = &R3D.container.aDrawDeferredInst;

    if (mode == R3D_RENDER_FORWARD) {
        drawCall.forward.alphaScissorThreshold = R3D.state.render.alphaScissorThreshold;
        drawCall.forward.blendMode = R3D.state.render.blendMode;
        arr = &R3D.container.aDrawForwardInst;
    }

    r3d_array_push_back(arr, &drawCall);
}

void R3D_DrawModel(Model model, Vector3 position, float scale)
{
    Vector3 vScale = { scale, scale, scale };
    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    R3D_DrawModelEx(model, position, rotationAxis, 0.0f, vScale);
}

void R3D_DrawModelEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale)
{
    Matrix matScale = MatrixScale(scale.x, scale.y, scale.z);
    Matrix matRotation = MatrixRotate(rotationAxis, rotationAngle * DEG2RAD);
    Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);
    Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

    model.transform = MatrixMultiply(model.transform, matTransform);

    for (int i = 0; i < model.meshCount; i++) {
        R3D_DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], model.transform);
    }
}

void R3D_DrawSprite(R3D_Sprite sprite, Vector3 position)
{
    R3D_DrawSpritePro(sprite, position, (Vector2) { 1.0f, 1.0f }, (Vector3) { 0, 1, 0 }, 0.0f);
}

void R3D_DrawSpriteEx(R3D_Sprite sprite, Vector3 position, Vector2 size, float rotation)
{
    R3D_DrawSpritePro(sprite, position, size, (Vector3) { 0, 1, 0 }, rotation);
}

void R3D_DrawSpritePro(R3D_Sprite sprite, Vector3 position, Vector2 size, Vector3 rotationAxis, float rotationAngle)
{
    Matrix matScale = MatrixScale(fabsf(size.x) * 0.5f, -fabsf(size.y) * 0.5f, 1.0f);
    Matrix matRotation = MatrixRotate(rotationAxis, rotationAngle * DEG2RAD);
    Matrix matTranslation = MatrixTranslate(position.x, position.y, position.z);
    Matrix matTransform = MatrixMultiply(MatrixMultiply(matScale, matRotation), matTranslation);

    r3d_drawcall_t drawCall = { 0 };

    if (R3D.state.render.billboardMode == R3D_BILLBOARD_FRONT) {
        r3d_billboard_mode_front(&matTransform, &R3D.state.transform.invView);
    }
    else if (R3D.state.render.billboardMode == R3D_BILLBOARD_Y_AXIS) {
        r3d_billboard_mode_y(&matTransform, &R3D.state.transform.invView);
    }

    drawCall.transform = matTransform;
    drawCall.material = sprite.material;
    drawCall.geometryType = R3D_DRAWCALL_GEOMETRY_SPRITE;
    drawCall.shadowCastMode = R3D.state.render.shadowCastMode;

    r3d_sprite_get_uv_scale_offset(
        &sprite, &drawCall.geometry.sprite.uvScale, &drawCall.geometry.sprite.uvOffset,
        (size.x > 0) ? 1.0f : -1.0f, (size.y > 0) ? 1.0f : -1.0f
    );

    R3D_RenderMode mode = R3D.state.render.mode;

    if (mode == R3D_RENDER_AUTO_DETECT) {
        mode = r3d_render_auto_detect_mode(&sprite.material);
    }

    r3d_array_t* arr = &R3D.container.aDrawDeferred;

    if (mode == R3D_RENDER_FORWARD) {
        drawCall.forward.alphaScissorThreshold = R3D.state.render.alphaScissorThreshold;
        drawCall.forward.blendMode = R3D.state.render.blendMode;
        arr = &R3D.container.aDrawForward;
    }

    r3d_array_push_back(arr, &drawCall);
}

void R3D_DrawParticleSystem(const R3D_ParticleSystem* system, Mesh mesh, Material material)
{
    R3D_DrawParticleSystemEx(system, mesh, material, MatrixIdentity());
}

void R3D_DrawParticleSystemEx(const R3D_ParticleSystem* system, Mesh mesh, Material material, Matrix transform)
{
    R3D_DrawMeshInstancedPro(
        mesh, material, transform,
        &system->particles->transform, sizeof(R3D_Particle),
        &system->particles->color, sizeof(R3D_Particle),
        system->count
    );
}


/* === Internal functions === */

static bool r3d_has_deferred_calls(void)
{
    return (R3D.container.aDrawDeferred.count > 0 || R3D.container.aDrawDeferredInst.count > 0);
}

static bool r3d_has_forward_calls(void)
{
    return (R3D.container.aDrawForward.count > 0 || R3D.container.aDrawForwardInst.count > 0);
}

void r3d_sprite_get_uv_scale_offset(const R3D_Sprite* sprite, Vector2* uvScale, Vector2* uvOffset, float sgnX, float sgnY)
{
    uvScale->x = sgnX / sprite->xFrameCount;
    uvScale->y = sgnY / sprite->yFrameCount;

    int frameIndex = (int)sprite->currentFrame % (sprite->xFrameCount * sprite->yFrameCount);
    int frameX = frameIndex % sprite->xFrameCount;
    int frameY = frameIndex / sprite->xFrameCount;

    uvOffset->x = frameX * uvScale->x;
    uvOffset->y = frameY * uvScale->y;
}

void r3d_shadow_apply_cast_mode(R3D_ShadowCastMode mode)
{
    switch (mode)
    {
    case R3D_SHADOW_CAST_FRONT_FACES:
        rlEnableBackfaceCulling();
        rlSetCullFace(RL_CULL_FACE_BACK);
        break;
    case R3D_SHADOW_CAST_BACK_FACES:
        rlEnableBackfaceCulling();
        rlSetCullFace(RL_CULL_FACE_FRONT);
        break;
    case R3D_SHADOW_CAST_ALL_FACES:
        rlDisableBackfaceCulling();
        break;
    default:
        break;
    }
}

R3D_RenderMode r3d_render_auto_detect_mode(const Material* material)
{
    // If the desired mode is opaque, then there is no need to perform further tests
    if (R3D.state.render.blendMode == R3D_BLEND_OPAQUE) {
        return R3D_RENDER_DEFERRED;
    }

    // If the blend mode is not alpha but also not opaque
    // (such as additive or multiply), then we still need the forward render mode
    if (R3D.state.render.blendMode != R3D_BLEND_ALPHA) {
        return R3D_RENDER_FORWARD;
    }

    // Obtaining the albedo texture used in the material
    unsigned int texId = material->maps[MATERIAL_MAP_ALBEDO].texture.id;

    // Detecting if it is a default texture
    bool defaultTex = (texId == 0 || texId == rlGetTextureIdDefault());

    // Detecting if a transparency value is used for the albedo
    bool alphaColor = (material->maps[MATERIAL_MAP_ALBEDO].color.a < 255);

    // Detecting if the format of the used texture supports transparency
    // NOTE: By default, we know that default textures do not contain transparency
    bool alphaFormat = false;
    if (!defaultTex) {
        switch (material->maps[MATERIAL_MAP_ALBEDO].texture.format) {
        case PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:
        case PIXELFORMAT_UNCOMPRESSED_R5G5B5A1:
        case PIXELFORMAT_UNCOMPRESSED_R4G4B4A4:
        case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:
        case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:
        case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:
        case PIXELFORMAT_COMPRESSED_DXT1_RGBA:
        case PIXELFORMAT_COMPRESSED_DXT3_RGBA:
        case PIXELFORMAT_COMPRESSED_DXT5_RGBA:
        case PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA:
        case PIXELFORMAT_COMPRESSED_PVRT_RGBA:
        case PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA:
        case PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA:
            alphaFormat = true;
            break;
        default:
            break;
        }
    }

    // If the color contains transparency or the texture format supports it
    // and the current blend mode is alpha, then we will use the forward mode
    if ((alphaColor || alphaFormat) && R3D.state.render.blendMode == R3D_BLEND_ALPHA) {
        return R3D_RENDER_FORWARD;
    }

    // Here, the alpha blend mode is requested but transparency is not possible,
    // so we can perform the rendering in deferred mode
    return R3D_RENDER_DEFERRED;
}

void r3d_render_apply_blend_mode(R3D_BlendMode mode)
{
    switch (mode)
    {
    case R3D_BLEND_OPAQUE:
        glDisable(GL_BLEND);
        break;
    case R3D_BLEND_ALPHA:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case R3D_BLEND_ADDITIVE:
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case R3D_BLEND_MULTIPLY:
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    default:
        break;
    }
}

void r3d_gbuffer_enable_stencil_write(void)
{
    // Re-attach the depth/stencil buffer to the framebuffer
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_TEXTURE_2D, R3D.framebuffer.gBuffer.depth, 0
    );

    // Setup the stencil: write 1 everywhere where geometry exists
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);                        // Permit writing to stencil buffer
    glStencilFunc(GL_ALWAYS, 1, 0xFF);          // Always pass the test, write 1
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);  // Replace stencil value with 1
}

void r3d_gbuffer_enable_stencil_test(bool passOnGeometry)
{
    // Attach the depth/stencil texture of the G-Buffer
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_TEXTURE_2D, R3D.framebuffer.gBuffer.depth, 0
    );

    // Setup the stencil
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0x00);                            // Disable writing to the stencil buffer
    glStencilFunc(GL_EQUAL, passOnGeometry, 0xFF);  // Pass the test only when the value is 0 or 1 (void or geometry)
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);         // Do not modify the stencil buffer
}

void r3d_gbuffer_disable_stencil(void)
{
    glDisable(GL_STENCIL_TEST);
}

void r3d_prepare_sort_drawcalls(void)
{
    // Sort front-to-back for deferred rendering
    // This optimizes the depth test
    r3d_drawcall_sort_front_to_back(
        (r3d_drawcall_t*)R3D.container.aDrawDeferred.data,
        R3D.container.aDrawDeferred.count
    );

    // Sort back-to-front for forward rendering
    // Ensures better transparency handling
    r3d_drawcall_sort_back_to_front(
        (r3d_drawcall_t*)R3D.container.aDrawForward.data,
        R3D.container.aDrawForward.count
    );
}

void r3d_prepare_process_lights_and_batch(void)
{
    // Clear the previous light batch
    r3d_array_clear(&R3D.container.aLightBatch);

    // Compute view / projection matrix
    Matrix viewProj = MatrixMultiply(R3D.state.transform.view, R3D.state.transform.proj);

    for (int id = 1; id <= (int)r3d_registry_get_allocated_count(&R3D.container.rLights); id++) {
        // Check if the light in the registry is still valid
        if (!r3d_registry_is_valid(&R3D.container.rLights, id)) continue;

        // Get the valid light and check if it is active
        r3d_light_t* light = r3d_registry_get(&R3D.container.rLights, id);
        if (!light->enabled) continue;

        // Process shadow update mode
        if (light->shadow.enabled) {
            r3d_light_process_shadow_update(light);
        }

        // Compute the projected area of the light into the screen
        Rectangle dstRect = { 0 };
        switch (light->type) {
        case R3D_LIGHT_DIR:
            dstRect.x = 0, dstRect.y = 0;
            dstRect.width = (float)R3D.state.resolution.width;
            dstRect.height = (float)R3D.state.resolution.height;
            break;
        case R3D_LIGHT_SPOT: {
            dstRect = r3d_project_cone_bounding_box(
                light->position, light->direction, light->range, fabsf(light->range * light->outerCutOff), //< r = h * cos(phi)
                R3D.state.transform.position, viewProj, R3D.state.resolution.width, R3D.state.resolution.height
            );
        } break;
        case R3D_LIGHT_OMNI:
            dstRect = r3d_project_sphere_bounding_box(
                light->position, light->range, R3D.state.transform.position, viewProj,
                R3D.state.resolution.width, R3D.state.resolution.height
            );
            break;
        }

        // Determine if the light illuminates a part visible to the screen
        int screenW = R3D.state.resolution.width;
        int screenH = R3D.state.resolution.height;
        if (!CheckCollisionRecs(dstRect, (Rectangle) { 0, 0, (float)screenW, (float)screenH })) {
            continue;
        }

        // Clamp light screen area to the screen dimensions
        if (dstRect.x < 0) dstRect.width += dstRect.x, dstRect.x = 0;
        if (dstRect.y < 0) dstRect.height += dstRect.y, dstRect.y = 0;
        dstRect.width = Clamp(dstRect.width, 0, screenW - dstRect.x);
        dstRect.height = Clamp(dstRect.height, 0, screenH - dstRect.y);

        // Here the light is supposed to be visible
        r3d_light_batched_t batched = { .data = light, .dstRect = dstRect };
        r3d_array_push_back(&R3D.container.aLightBatch, &batched);
    }
}

void r3d_pass_shadow_maps(void)
{
    // Config context state
    rlDisableColorBlend();
    rlEnableDepthTest();

    // Push new projection matrix
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();

    // Iterate through all lights to render all geometries
    for (int i = 0; i < R3D.container.aLightBatch.count; i++) {
        r3d_light_batched_t* light = r3d_array_at(&R3D.container.aLightBatch, i);

        // Skip light if it doesn't produce shadows
        if (!light->data->shadow.enabled) continue;

        // Skip if it's not time to update shadows
        if (!light->data->shadow.updateConf.shoudlUpdate) continue;
        else r3d_light_indicate_shadow_update(light->data);

        // TODO: The lights could be sorted to avoid too frequent
        //       state changes, just like with shaders.

        // TODO: The draw calls could also be sorted
        //       according to the shadow cast mode.

        // Start rendering to shadow map
        rlEnableFramebuffer(light->data->shadow.map.id);
        {
            rlViewport(0, 0, light->data->shadow.map.resolution, light->data->shadow.map.resolution);

            if (light->data->type == R3D_LIGHT_OMNI) {
                // Set up projection matrix for omni-directional light
                rlMatrixMode(RL_PROJECTION);
                rlSetMatrixProjection(r3d_light_get_matrix_proj_omni(light->data));

                // Render geometries for each face of the cubemap
                for (int j = 0; j < 6; j++) {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, light->data->shadow.map.depth, 0);
                    glClear(GL_DEPTH_BUFFER_BIT);

                    // Set view matrix for the current cubemap face
                    rlMatrixMode(RL_MODELVIEW);
                    rlLoadIdentity();
                    rlMultMatrixf(MatrixToFloat(r3d_light_get_matrix_view_omni(light->data, j)));

                    // Rasterize geometries for depth rendering
                    r3d_shader_enable(raster.depthCubeInst);
                    {
                        r3d_shader_set_float(raster.depthCubeInst, uAlphaScissorThreshold, R3D.state.render.alphaScissorThreshold);
                        r3d_shader_set_vec3(raster.depthCubeInst, uViewPosition, light->data->position);
                        r3d_shader_set_float(raster.depthCubeInst, uFar, light->data->far);

                        for (size_t k = 0; k < R3D.container.aDrawDeferredInst.count; k++) {
                            r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferredInst.data + k;
                            if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                                r3d_shadow_apply_cast_mode(call->shadowCastMode);
                                r3d_drawcall_raster_depth_cube_inst(call);
                            }
                        }

                        for (size_t k = 0; k < R3D.container.aDrawForwardInst.count; k++) {
                            r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForwardInst.data + k;
                            if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                                r3d_shadow_apply_cast_mode(call->shadowCastMode);
                                r3d_drawcall_raster_depth_cube_inst(call);
                            }
                        }
                    }
                    r3d_shader_enable(raster.depthCube);
                    {
                        r3d_shader_set_float(raster.depthCube, uAlphaScissorThreshold, R3D.state.render.alphaScissorThreshold);
                        r3d_shader_set_vec3(raster.depthCube, uViewPosition, light->data->position);
                        r3d_shader_set_float(raster.depthCube, uFar, light->data->far);

                        for (size_t k = 0; k < R3D.container.aDrawDeferred.count; k++) {
                            r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferred.data + k;
                            if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                                r3d_shadow_apply_cast_mode(call->shadowCastMode);
                                r3d_drawcall_raster_depth_cube(call);
                            }
                        }

                        for (size_t k = 0; k < R3D.container.aDrawForward.count; k++) {
                            r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForward.data + k;
                            if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                                r3d_shadow_apply_cast_mode(call->shadowCastMode);
                                r3d_drawcall_raster_depth_cube(call);
                            }
                        }
                    }
                }
            }
            else {
                // Clear depth buffer for other light types
                glClear(GL_DEPTH_BUFFER_BIT);

                Matrix matView = { 0 };
                Matrix matProj = { 0 };

                if (light->data->type == R3D_LIGHT_DIR) {
                    r3d_light_get_matrix_vp_dir(light->data, R3D.state.scene.bounds, &matView, &matProj);
                }
                else if (light->data->type == R3D_LIGHT_SPOT) {
                    matView = r3d_light_get_matrix_view_spot(light->data);
                    matProj = r3d_light_get_matrix_proj_spot(light->data);
                }

                // Store combined view and projection matrix for the shadow map
                light->data->shadow.matVP = MatrixMultiply(matView, matProj);

                // Set up projection matrix
                rlMatrixMode(RL_PROJECTION);
                rlSetMatrixProjection(matProj);

                // Set up view matrix
                rlMatrixMode(RL_MODELVIEW);
                rlLoadIdentity();
                rlMultMatrixf(MatrixToFloat(matView));

                // Rasterize geometry for depth rendering
                r3d_shader_enable(raster.depthInst);
                {
                    r3d_shader_set_float(raster.depthInst, uAlphaScissorThreshold, R3D.state.render.alphaScissorThreshold);

                    for (size_t j = 0; j < R3D.container.aDrawDeferredInst.count; j++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferredInst.data + j;
                        if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_shadow_apply_cast_mode(call->shadowCastMode);
                            r3d_drawcall_raster_depth_inst(call);
                        }
                    }
                    for (size_t j = 0; j < R3D.container.aDrawForwardInst.count; j++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForwardInst.data + j;
                        if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_shadow_apply_cast_mode(call->shadowCastMode);
                            r3d_drawcall_raster_depth_inst(call);
                        }
                    }
                }
                r3d_shader_enable(raster.depth);
                {
                    r3d_shader_set_float(raster.depth, uAlphaScissorThreshold, R3D.state.render.alphaScissorThreshold);

                    for (size_t j = 0; j < R3D.container.aDrawDeferred.count; j++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferred.data + j;
                        if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_shadow_apply_cast_mode(call->shadowCastMode);
                            r3d_drawcall_raster_depth(call);
                        }
                    }
                    for (size_t j = 0; j < R3D.container.aDrawForward.count; j++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForward.data + j;
                        if (call->shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_shadow_apply_cast_mode(call->shadowCastMode);
                            r3d_drawcall_raster_depth(call);
                        }
                    }
                }
            }
            r3d_shader_disable();
        }
    }
    rlDisableFramebuffer();

    // Reset face to cull
    rlSetCullFace(RL_CULL_FACE_BACK);

    // Pop projection matrix
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();

    // Reset model-view matrix
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void r3d_pass_gbuffer(void)
{
    rlEnableFramebuffer(R3D.framebuffer.gBuffer.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlEnableBackfaceCulling();
        rlDisableColorBlend();
        rlEnableDepthTest();
        rlEnableDepthMask();

        // Enbale geometry stencil write
        r3d_gbuffer_enable_stencil_write();

        // Clear the buffers
        glClearStencil(0);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Setup projection matrix
        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlSetMatrixProjection(R3D.state.transform.proj);

        // Setup view matrix
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
        rlMultMatrixf(MatrixToFloat(R3D.state.transform.view));

        // Draw geometry with the stencil buffer activated
        r3d_shader_enable(raster.geometryInst);
        {
            for (size_t i = 0; i < R3D.container.aDrawDeferredInst.count; i++) {
                r3d_drawcall_raster_geometry_inst((r3d_drawcall_t*)R3D.container.aDrawDeferredInst.data + i);
            }
        }
        r3d_shader_enable(raster.geometry);
        {
            for (size_t i = 0; i < R3D.container.aDrawDeferred.count; i++) {
                r3d_drawcall_raster_geometry((r3d_drawcall_t*)R3D.container.aDrawDeferred.data + i);
            }
        }
        r3d_shader_disable();

        // Reset projection matrix
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();

        // Reset view matrix
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
    }
}

void r3d_pass_ssao(void)
{
    rlEnableFramebuffer(R3D.framebuffer.pingPongSSAO.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width / 2, R3D.state.resolution.height / 2);
        rlDisableColorBlend();
        rlDisableDepthTest();

        // Enable gbuffer stencil test (render on geometry)
        if (R3D.state.flags & R3D_FLAG_STENCIL_TEST) {
            r3d_gbuffer_enable_stencil_test(true);
        }
        else {
            r3d_gbuffer_disable_stencil();
        }

        // Bind first SSAO output texture
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            R3D.framebuffer.pingPongSSAO.target, 0
        );

        // Render SSAO
        r3d_shader_enable(screen.ssao);
        {
            r3d_framebuffer_swap_pingpong(R3D.framebuffer.pingPongSSAO);

            r3d_shader_set_mat4(screen.ssao, uMatInvProj, R3D.state.transform.invProj);
            r3d_shader_set_mat4(screen.ssao, uMatInvView, R3D.state.transform.invView);
            r3d_shader_set_mat4(screen.ssao, uMatProj, R3D.state.transform.proj);
            r3d_shader_set_mat4(screen.ssao, uMatView, R3D.state.transform.view);

            r3d_shader_set_vec2(screen.ssao, uResolution, ((Vector2) {
                (float)R3D.state.resolution.width / 2,
                (float)R3D.state.resolution.height / 2
            }));

            r3d_shader_set_float(screen.ssao, uNear, (float)rlGetCullDistanceNear());
            r3d_shader_set_float(screen.ssao, uFar, (float)rlGetCullDistanceFar());

            r3d_shader_set_float(screen.ssao, uRadius, R3D.env.ssaoRadius);
            r3d_shader_set_float(screen.ssao, uBias, R3D.env.ssaoBias);

            r3d_shader_bind_sampler2D(screen.ssao, uTexDepth, R3D.framebuffer.gBuffer.depth);
            r3d_shader_bind_sampler2D(screen.ssao, uTexNormal, R3D.framebuffer.gBuffer.normal);
            r3d_shader_bind_sampler1D(screen.ssao, uTexKernel, R3D.texture.ssaoKernel);
            r3d_shader_bind_sampler2D(screen.ssao, uTexNoise, R3D.texture.ssaoNoise);

            r3d_primitive_draw_screen();

            r3d_shader_unbind_sampler2D(screen.ssao, uTexDepth);
            r3d_shader_unbind_sampler2D(screen.ssao, uTexNormal);
            r3d_shader_unbind_sampler1D(screen.ssao, uTexKernel);
            r3d_shader_unbind_sampler2D(screen.ssao, uTexNoise);
        }
        r3d_shader_disable();

        // Blur SSAO
        r3d_shader_enable(generate.gaussianBlurDualPass)
        {
            for (int i = 0, horizontal = true; i < R3D.env.ssaoIterations; i++, horizontal = !horizontal) {
                r3d_framebuffer_swap_pingpong(R3D.framebuffer.pingPongSSAO);
                r3d_shader_set_vec2(generate.gaussianBlurDualPass, uTexelDir,
                    ((horizontal)
                        ? (Vector2) { R3D.state.resolution.texelX, 0 }
                        : (Vector2) { 0, R3D.state.resolution.texelY })
                );
                r3d_shader_bind_sampler2D(
                    generate.gaussianBlurDualPass, uTexture,
                    R3D.framebuffer.pingPongSSAO.source
                );
                r3d_primitive_draw_screen();
            }
        }
        r3d_shader_disable();
    }
}

void r3d_pass_deferred_ambient(void)
{
    rlEnableFramebuffer(R3D.framebuffer.deferred.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlClearScreenBuffers();
        rlDisableColorBlend();
        rlDisableDepthTest();
        rlDisableDepthMask();

        // Enable gbuffer stencil test (render on geometry)
        if (R3D.state.flags & R3D_FLAG_STENCIL_TEST) {
            r3d_gbuffer_enable_stencil_test(true);
        }
        else {
            r3d_gbuffer_disable_stencil();
        }

        if (R3D.env.useSky)
        {
            rlActiveDrawBuffers(2);

            // Compute skybox IBL
            r3d_shader_enable(screen.ambientIbl);
            {
                r3d_shader_bind_sampler2D(screen.ambientIbl, uTexAlbedo, R3D.framebuffer.gBuffer.albedo);
                r3d_shader_bind_sampler2D(screen.ambientIbl, uTexNormal, R3D.framebuffer.gBuffer.normal);
                r3d_shader_bind_sampler2D(screen.ambientIbl, uTexDepth, R3D.framebuffer.gBuffer.depth);
                r3d_shader_bind_sampler2D(screen.ambientIbl, uTexORM, R3D.framebuffer.gBuffer.orm);

                if (R3D.env.ssaoEnabled) {
                    r3d_shader_bind_sampler2D(
                        screen.ambientIbl, uTexSSAO,
                        R3D.framebuffer.pingPongSSAO.target
                    );
                }
                else {
                    r3d_shader_bind_sampler2D(
                        screen.ambientIbl, uTexSSAO,
                        R3D.texture.white
                    );
                }

                r3d_shader_bind_samplerCube(screen.ambientIbl, uCubeIrradiance, R3D.env.sky.irradiance.id);
                r3d_shader_bind_samplerCube(screen.ambientIbl, uCubePrefilter, R3D.env.sky.prefilter.id);
                r3d_shader_bind_sampler2D(screen.ambientIbl, uTexBrdfLut, R3D.texture.iblBrdfLut);

                r3d_shader_set_vec3(screen.ambientIbl, uViewPosition, R3D.state.transform.position);
                r3d_shader_set_mat4(screen.ambientIbl, uMatInvProj, R3D.state.transform.invProj);
                r3d_shader_set_mat4(screen.ambientIbl, uMatInvView, R3D.state.transform.invView);
                r3d_shader_set_vec4(screen.ambientIbl, uQuatSkybox, R3D.env.quatSky);

                r3d_primitive_draw_screen();

                r3d_shader_unbind_sampler2D(screen.ambientIbl, uTexAlbedo);
                r3d_shader_unbind_sampler2D(screen.ambientIbl, uTexNormal);
                r3d_shader_unbind_sampler2D(screen.ambientIbl, uTexDepth);
                r3d_shader_unbind_sampler2D(screen.ambientIbl, uTexSSAO);
                r3d_shader_unbind_sampler2D(screen.ambientIbl, uTexORM);

                r3d_shader_unbind_samplerCube(screen.ambientIbl, uCubeIrradiance);
                r3d_shader_unbind_samplerCube(screen.ambientIbl, uCubePrefilter);
                r3d_shader_unbind_sampler2D(screen.ambientIbl, uTexBrdfLut);
            }
            r3d_shader_disable();
        }
        // If no skybox is set, we simply render ambient tint on the meshes.
        else
        {
            rlActiveDrawBuffers(1);

            r3d_shader_enable(screen.ambient);
            {
                r3d_shader_bind_sampler2D(screen.ambient, uTexORM, R3D.framebuffer.gBuffer.orm);

                if (R3D.env.ssaoEnabled) {
                    r3d_shader_bind_sampler2D(
                        screen.ambient, uTexSSAO,
                        R3D.framebuffer.pingPongSSAO.target
                    );
                }
                else {
                    r3d_shader_bind_sampler2D(
                        screen.ambient, uTexSSAO,
                        R3D.texture.white
                    );
                }

                r3d_shader_set_vec4(screen.ambient, uColor, ((Vector4) {
                    R3D.env.ambientColor.x,
                    R3D.env.ambientColor.y,
                    R3D.env.ambientColor.z,
                    0.0f
                }));

                r3d_primitive_draw_screen();

                r3d_shader_unbind_sampler2D(screen.ambient, uTexSSAO);
                r3d_shader_unbind_sampler2D(screen.ambient, uTexORM);
            }
            r3d_shader_disable();
        }
    }
}

void r3d_pass_deferred_lights(void)
{
    rlEnableFramebuffer(R3D.framebuffer.deferred.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableDepthTest();

        rlEnableColorBlend();
        rlSetBlendMode(RL_BLEND_ADDITIVE);

        // Enable gbuffer stencil test (render on geometry)
        if (R3D.state.flags & R3D_FLAG_STENCIL_TEST) {
            r3d_gbuffer_enable_stencil_test(true);
        }
        else {
            r3d_gbuffer_disable_stencil();
        }

        r3d_shader_enable(screen.lighting);
        {
            r3d_shader_set_mat4(screen.lighting, uMatInvProj, R3D.state.transform.invProj);
            r3d_shader_set_mat4(screen.lighting, uMatInvView, R3D.state.transform.invView);
            r3d_shader_set_vec3(screen.lighting, uViewPosition, R3D.state.transform.position);

            r3d_shader_bind_sampler2D(screen.lighting, uTexAlbedo, R3D.framebuffer.gBuffer.albedo);
            r3d_shader_bind_sampler2D(screen.lighting, uTexNormal, R3D.framebuffer.gBuffer.normal);
            r3d_shader_bind_sampler2D(screen.lighting, uTexDepth, R3D.framebuffer.gBuffer.depth);
            r3d_shader_bind_sampler2D(screen.lighting, uTexORM, R3D.framebuffer.gBuffer.orm);
            r3d_shader_bind_sampler2D(screen.lighting, uTexNoise, R3D.texture.blueNoise);

            for (int i = 0; i < R3D.container.aLightBatch.count; i++) {
                r3d_light_batched_t* light = r3d_array_at(&R3D.container.aLightBatch, i);

                // Send common data
                r3d_shader_set_vec3(screen.lighting, uLight.color, light->data->color);
                r3d_shader_set_float(screen.lighting, uLight.specular, light->data->specular);
                r3d_shader_set_float(screen.lighting, uLight.energy, light->data->energy);
                r3d_shader_set_int(screen.lighting, uLight.type, light->data->type);

                // Send specific data
                if (light->data->type == R3D_LIGHT_DIR) {
                    r3d_shader_set_vec3(screen.lighting, uLight.direction, light->data->direction);
                }
                else if (light->data->type == R3D_LIGHT_SPOT) {
                    r3d_shader_set_vec3(screen.lighting, uLight.position, light->data->position);
                    r3d_shader_set_vec3(screen.lighting, uLight.direction, light->data->direction);
                    r3d_shader_set_float(screen.lighting, uLight.range, light->data->range);
                    r3d_shader_set_float(screen.lighting, uLight.attenuation, light->data->attenuation);
                    r3d_shader_set_float(screen.lighting, uLight.innerCutOff, light->data->innerCutOff);
                    r3d_shader_set_float(screen.lighting, uLight.outerCutOff, light->data->outerCutOff);
                }
                else if (light->data->type == R3D_LIGHT_OMNI) {
                    r3d_shader_set_vec3(screen.lighting, uLight.position, light->data->position);
                    r3d_shader_set_float(screen.lighting, uLight.range, light->data->range);
                    r3d_shader_set_float(screen.lighting, uLight.attenuation, light->data->attenuation);
                }

                // Send shadow map data
                if (light->data->shadow.enabled) {
                    if (light->data->type == R3D_LIGHT_OMNI) {
                        r3d_shader_bind_samplerCube(screen.lighting, uLight.shadowCubemap, light->data->shadow.map.depth);
                    }
                    else {
                        r3d_shader_set_float(screen.lighting, uLight.shadowMapTxlSz, light->data->shadow.map.texelSize);
                        r3d_shader_bind_sampler2D(screen.lighting, uLight.shadowMap, light->data->shadow.map.depth);
                        r3d_shader_set_mat4(screen.lighting, uLight.matVP, light->data->shadow.matVP);
                        if (light->data->type == R3D_LIGHT_DIR) {
                            // NOTE: The position of the directional lights is automatically calculated
                            //       in `r3d_light_get_matrix_vp_dir`, and is used for shadows
                            r3d_shader_set_vec3(screen.lighting, uLight.position, light->data->position);
                        }
                    }
                    r3d_shader_set_float(screen.lighting, uLight.shadowSoftness, light->data->shadow.softness);
                    r3d_shader_set_float(screen.lighting, uLight.shadowBias, light->data->shadow.bias);
                    r3d_shader_set_float(screen.lighting, uLight.near, light->data->near);
                    r3d_shader_set_float(screen.lighting, uLight.far, light->data->far);
                    r3d_shader_set_int(screen.lighting, uLight.shadow, true);
                }
                else {
                    r3d_shader_set_int(screen.lighting, uLight.shadow, false);
                }

                //if (light->data->type != R3D_LIGHT_DIR) {
                //    glEnable(GL_SCISSOR_TEST);
                //    glScissor(
                //        light->dstRect.x, light->dstRect.y,
                //        light->dstRect.width, light->dstRect.height
                //    );
                //}

                r3d_primitive_draw_screen();

                //if (light->data->type != R3D_LIGHT_DIR) {
                //    glDisable(GL_SCISSOR_TEST);
                //}
            }

            r3d_shader_unbind_sampler2D(screen.lighting, uTexAlbedo);
            r3d_shader_unbind_sampler2D(screen.lighting, uTexNormal);
            r3d_shader_unbind_sampler2D(screen.lighting, uTexDepth);
            r3d_shader_unbind_sampler2D(screen.lighting, uTexORM);
            r3d_shader_unbind_sampler2D(screen.lighting, uTexNoise);

            r3d_shader_unbind_samplerCube(screen.lighting, uLight.shadowCubemap);
            r3d_shader_unbind_sampler2D(screen.lighting, uLight.shadowMap);
        }
        r3d_shader_disable();
    }
}

void r3d_pass_scene_background(void)
{
    rlEnableFramebuffer(R3D.framebuffer.scene.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        if (R3D.env.useSky)
        {
            // Setup projection matrix
            rlMatrixMode(RL_PROJECTION);
            rlPushMatrix();
            rlSetMatrixProjection(R3D.state.transform.proj);

            // Setup view matrix
            rlMatrixMode(RL_MODELVIEW);
            rlLoadIdentity();
            rlMultMatrixf(MatrixToFloat(R3D.state.transform.view));

            // Disable backface culling to render the cube from the inside
            rlDisableBackfaceCulling();

            // Render skybox
            r3d_shader_enable(raster.skybox);
            {
                Matrix matView = rlGetMatrixModelview();
                Matrix matProj = rlGetMatrixProjection();

                // Bind cubemap texture
                r3d_shader_bind_samplerCube(raster.skybox, uCubeSky, R3D.env.sky.cubemap.id);

                // Set skybox parameters
                r3d_shader_set_vec4(raster.skybox, uRotation, R3D.env.quatSky);

                // Try binding vertex array objects (VAO) or use VBOs if not possible
                if (!rlEnableVertexArray(R3D.primitive.cube.vao)) {
                    rlEnableVertexBuffer(R3D.primitive.cube.vbo);
                    rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
                    rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
                    rlEnableVertexBufferElement(R3D.primitive.cube.ebo);
                }

                // Draw skybox (supporting stereo rendering)
                if (rlIsStereoRenderEnabled()) {
                    for (int eye = 0; eye < 2; eye++) {
                        rlViewport(eye * rlGetFramebufferWidth() / 2, 0, rlGetFramebufferWidth() / 2, rlGetFramebufferHeight());
                        r3d_shader_set_mat4(raster.skybox, uMatView, MatrixMultiply(matView, rlGetMatrixViewOffsetStereo(eye)));
                        r3d_shader_set_mat4(raster.skybox, uMatProj, rlGetMatrixProjectionStereo(eye));
                        rlDrawVertexArrayElements(0, 36, 0);
                    }
                }
                else {
                    r3d_shader_set_mat4(raster.skybox, uMatView, matView);
                    r3d_shader_set_mat4(raster.skybox, uMatProj, matProj);
                    rlDrawVertexArrayElements(0, 36, 0);
                }

                // Unbind cubemap texture
                r3d_shader_unbind_samplerCube(raster.skybox, uCubeSky);

                // Disable all possible vertex array objects (or VBOs)
                rlDisableVertexArray();
                rlDisableVertexBuffer();
                rlDisableVertexBufferElement();
            }
            r3d_shader_disable();

            // Reset back face culling
            rlEnableBackfaceCulling();

            // Reset projection matrix
            rlMatrixMode(RL_PROJECTION);
            rlPopMatrix();

            // Reset view matrix
            rlMatrixMode(RL_MODELVIEW);
            rlLoadIdentity();
        }
        else
        {
            glClearBufferfv(GL_COLOR, 0, (float[4]) {
                R3D.env.backgroundColor.x,
                R3D.env.backgroundColor.y,
                R3D.env.backgroundColor.z,
                0.0f
            });
            glClearBufferfv(GL_COLOR, 1, (float[4]) {
                0.0f, 0.0f, 0.0f, 0.0f
            });
        }
    }
}

void r3d_pass_scene_deferred(void)
{
    rlEnableFramebuffer(R3D.framebuffer.scene.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        // Enable gbuffer stencil test (render on geometry)
        r3d_gbuffer_enable_stencil_test(true);

        r3d_shader_enable(screen.scene);
        {
            r3d_shader_bind_sampler2D(screen.scene, uTexAlbedo, R3D.framebuffer.gBuffer.albedo);
            r3d_shader_bind_sampler2D(screen.scene, uTexEmission, R3D.framebuffer.gBuffer.emission);
            r3d_shader_bind_sampler2D(screen.scene, uTexDiffuse, R3D.framebuffer.deferred.diffuse);
            r3d_shader_bind_sampler2D(screen.scene, uTexSpecular, R3D.framebuffer.deferred.specular);

            r3d_primitive_draw_screen();

            r3d_shader_unbind_sampler2D(screen.scene, uTexAlbedo);
            r3d_shader_unbind_sampler2D(screen.scene, uTexEmission);
            r3d_shader_unbind_sampler2D(screen.scene, uTexDiffuse);
            r3d_shader_unbind_sampler2D(screen.scene, uTexSpecular);
        }
        r3d_shader_disable();
    }
}

static void r3d_pass_scene_forward_filter_and_send_lights(const r3d_drawcall_t* call)
{
    int lightCount = 0;

    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS && i < R3D.container.aLightBatch.count; i++, lightCount++)
    {
        r3d_light_batched_t* light = r3d_array_at(&R3D.container.aLightBatch, i);

        // TODO: Review this, it's not precise at all, but hard to determine without a bounding box...
        if (light->data->type != R3D_LIGHT_DIR) {
            if (!r3d_collision_check_point_in_sphere_sqr(
                (Vector3) {
                call->transform.m12, call->transform.m13, call->transform.m14
            },
                light->data->position, light->data->range)) {
                continue;
            }
        }

        // Send common data
        r3d_shader_set_int(raster.forward, uLights[i].enabled, true);
        r3d_shader_set_int(raster.forward, uLights[i].type, light->data->type);
        r3d_shader_set_vec3(raster.forward, uLights[i].color, light->data->color);
        r3d_shader_set_float(raster.forward, uLights[i].specular, light->data->specular);
        r3d_shader_set_float(raster.forward, uLights[i].energy, light->data->energy);

        // Send specific data
        if (light->data->type == R3D_LIGHT_DIR) {
            r3d_shader_set_vec3(raster.forward, uLights[i].direction, light->data->direction);
        }
        else if (light->data->type == R3D_LIGHT_SPOT) {
            r3d_shader_set_vec3(raster.forward, uLights[i].position, light->data->position);
            r3d_shader_set_vec3(raster.forward, uLights[i].direction, light->data->direction);
            r3d_shader_set_float(raster.forward, uLights[i].range, light->data->range);
            r3d_shader_set_float(raster.forward, uLights[i].attenuation, light->data->attenuation);
            r3d_shader_set_float(raster.forward, uLights[i].innerCutOff, light->data->innerCutOff);
            r3d_shader_set_float(raster.forward, uLights[i].outerCutOff, light->data->outerCutOff);
        }
        else if (light->data->type == R3D_LIGHT_OMNI) {
            r3d_shader_set_vec3(raster.forward, uLights[i].position, light->data->position);
            r3d_shader_set_float(raster.forward, uLights[i].range, light->data->range);
            r3d_shader_set_float(raster.forward, uLights[i].attenuation, light->data->attenuation);
        }

        // Send shadow map data
        if (light->data->shadow.enabled) {
            if (light->data->type == R3D_LIGHT_OMNI) {
                r3d_shader_bind_samplerCube(raster.forward, uLights[i].shadowCubemap, light->data->shadow.map.depth);
            }
            else {
                r3d_shader_set_float(raster.forward, uLights[i].shadowMapTxlSz, light->data->shadow.map.texelSize);
                r3d_shader_bind_sampler2D(raster.forward, uLights[i].shadowMap, light->data->shadow.map.depth);
                r3d_shader_set_mat4(raster.forward, uMatLightVP[i], light->data->shadow.matVP);
            }
            r3d_shader_set_float(raster.forward, uLights[i].shadowSoftness, light->data->shadow.softness);
            r3d_shader_set_float(raster.forward, uLights[i].shadowBias, light->data->shadow.bias);
            r3d_shader_set_float(raster.forward, uLights[i].near, light->data->near);
            r3d_shader_set_float(raster.forward, uLights[i].far, light->data->far);
            r3d_shader_set_int(raster.forward, uLights[i].shadow, true);
        }
        else {
            r3d_shader_set_int(raster.forward, uLights[i].shadow, false);
        }
    }

    for (int i = lightCount; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        r3d_shader_set_int(raster.forward, uLights[i].enabled, false);
    }
}

static void r3d_pass_scene_forward_inst_filter_and_send_lights(const r3d_drawcall_t* call)
{
    int lightCount = 0;

    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS && i < R3D.container.aLightBatch.count; i++, lightCount++)
    {
        r3d_light_batched_t* light = r3d_array_at(&R3D.container.aLightBatch, i);

        // TODO: Determine which light should illuminate all instances seems compromised in the current state...
        //if (light->data->type != R3D_LIGHT_DIR) {
        //    if (!r3d_collision_check_point_in_sphere_sqr(
        //        (Vector3) {
        //        call->transform.m12, call->transform.m13, call->transform.m14
        //    },
        //        light->data->position, light->data->range)) {
        //        continue;
        //    }
        //}

        // Send common data
        r3d_shader_set_int(raster.forwardInst, uLights[i].enabled, true);
        r3d_shader_set_int(raster.forwardInst, uLights[i].type, light->data->type);
        r3d_shader_set_vec3(raster.forwardInst, uLights[i].color, light->data->color);
        r3d_shader_set_float(raster.forwardInst, uLights[i].specular, light->data->specular);
        r3d_shader_set_float(raster.forwardInst, uLights[i].energy, light->data->energy);

        // Send specific data
        if (light->data->type == R3D_LIGHT_DIR) {
            r3d_shader_set_vec3(raster.forwardInst, uLights[i].direction, light->data->direction);
        }
        else if (light->data->type == R3D_LIGHT_SPOT) {
            r3d_shader_set_vec3(raster.forwardInst, uLights[i].position, light->data->position);
            r3d_shader_set_vec3(raster.forwardInst, uLights[i].direction, light->data->direction);
            r3d_shader_set_float(raster.forwardInst, uLights[i].range, light->data->range);
            r3d_shader_set_float(raster.forwardInst, uLights[i].attenuation, light->data->attenuation);
            r3d_shader_set_float(raster.forwardInst, uLights[i].innerCutOff, light->data->innerCutOff);
            r3d_shader_set_float(raster.forwardInst, uLights[i].outerCutOff, light->data->outerCutOff);
        }
        else if (light->data->type == R3D_LIGHT_OMNI) {
            r3d_shader_set_vec3(raster.forwardInst, uLights[i].position, light->data->position);
            r3d_shader_set_float(raster.forwardInst, uLights[i].range, light->data->range);
            r3d_shader_set_float(raster.forwardInst, uLights[i].attenuation, light->data->attenuation);
        }

        // Send shadow map data
        if (light->data->shadow.enabled) {
            if (light->data->type == R3D_LIGHT_OMNI) {
                r3d_shader_bind_samplerCube(raster.forwardInst, uLights[i].shadowCubemap, light->data->shadow.map.depth);
            }
            else {
                r3d_shader_set_float(raster.forwardInst, uLights[i].shadowMapTxlSz, light->data->shadow.map.texelSize);
                r3d_shader_bind_sampler2D(raster.forwardInst, uLights[i].shadowMap, light->data->shadow.map.depth);
                r3d_shader_set_mat4(raster.forwardInst, uMatLightVP[i], light->data->shadow.matVP);
            }
            r3d_shader_set_float(raster.forwardInst, uLights[i].shadowSoftness, light->data->shadow.softness);
            r3d_shader_set_float(raster.forwardInst, uLights[i].shadowBias, light->data->shadow.bias);
            r3d_shader_set_float(raster.forwardInst, uLights[i].near, light->data->near);
            r3d_shader_set_float(raster.forwardInst, uLights[i].far, light->data->far);
            r3d_shader_set_int(raster.forwardInst, uLights[i].shadow, true);
        }
        else {
            r3d_shader_set_int(raster.forwardInst, uLights[i].shadow, false);
        }
    }

    for (int i = lightCount; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        r3d_shader_set_int(raster.forwardInst, uLights[i].enabled, false);
    }
}

void r3d_pass_scene_forward_depth_prepass(void)
{
    rlEnableFramebuffer(R3D.framebuffer.scene.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlEnableBackfaceCulling();

        // Setup the depth pre-pass
        rlColorMask(false, false, false, false);
        rlEnableDepthTest();
        rlEnableDepthMask();

        // Reactivation of geometry drawing in the stencil buffer
        r3d_gbuffer_enable_stencil_write();

        // Setup projection matrix
        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlSetMatrixProjection(R3D.state.transform.proj);

        // Setup view matrix
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
        rlMultMatrixf(MatrixToFloat(R3D.state.transform.view));

        // Render instanced meshes
        if (R3D.container.aDrawForwardInst.count > 0) {
            r3d_shader_enable(raster.depthInst);
            {
                for (int i = 0; i < R3D.container.aDrawForwardInst.count; i++) {
                    r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawForwardInst, i);
                    r3d_drawcall_raster_depth_inst(call);
                }
            }
            r3d_shader_disable();
        }

        // Render non-instanced meshes
        if (R3D.container.aDrawForward.count > 0) {
            r3d_shader_enable(raster.depth);
            {
                // We render in reverse order to prioritize drawing the nearest
                // objects first, in order to optimize early depth testing.
                for (int i = R3D.container.aDrawForward.count - 1; i >= 0; i--) {
                    r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawForward, i);
                    r3d_drawcall_raster_depth(call);
                }
            }
            r3d_shader_disable();
        }

        // Reset projection matrix
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();

        // Reset view matrix
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
    }
}

void r3d_pass_scene_forward(void)
{
    rlEnableFramebuffer(R3D.framebuffer.scene.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlColorMask(true, true, true, true);
        rlEnableBackfaceCulling();
        rlEnableDepthTest();

        if (R3D.state.flags & R3D_FLAG_DEPTH_PREPASS) {
            glDepthFunc(GL_EQUAL);
            rlDisableDepthMask();
        }
        else {
            r3d_gbuffer_enable_stencil_write();
            rlEnableDepthMask();
        }

        // Setup projection matrix
        rlMatrixMode(RL_PROJECTION);
        rlPushMatrix();
        rlSetMatrixProjection(R3D.state.transform.proj);

        // Setup view matrix
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
        rlMultMatrixf(MatrixToFloat(R3D.state.transform.view));

        // Render instanced meshes
        if (R3D.container.aDrawForwardInst.count > 0) {
            r3d_shader_enable(raster.forwardInst);
            {
                r3d_shader_bind_sampler2D(raster.forwardInst, uTexNoise, R3D.texture.blueNoise);

                if (R3D.env.useSky) {
                    r3d_shader_bind_samplerCube(raster.forwardInst, uCubeIrradiance, R3D.env.sky.irradiance.id);
                    r3d_shader_bind_samplerCube(raster.forwardInst, uCubePrefilter, R3D.env.sky.prefilter.id);
                    r3d_shader_bind_sampler2D(raster.forwardInst, uTexBrdfLut, R3D.texture.iblBrdfLut);

                    r3d_shader_set_vec4(raster.forwardInst, uQuatSkybox, R3D.env.quatSky);
                    r3d_shader_set_int(raster.forwardInst, uHasSkybox, true);
                }
                else {
                    r3d_shader_set_vec3(raster.forwardInst, uColAmbient, R3D.env.ambientColor);
                    r3d_shader_set_int(raster.forwardInst, uHasSkybox, false);
                }

                r3d_shader_set_vec3(raster.forwardInst, uViewPosition, R3D.state.transform.position);

                for (int i = 0; i < R3D.container.aDrawForwardInst.count; i++) {
                    r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawForwardInst, i);
                    r3d_pass_scene_forward_inst_filter_and_send_lights(call);
                    r3d_render_apply_blend_mode(call->forward.blendMode);
                    r3d_drawcall_raster_forward_inst(call);
                }

                r3d_shader_unbind_sampler2D(raster.forwardInst, uTexNoise);

                if (R3D.env.useSky) {
                    r3d_shader_unbind_samplerCube(raster.forwardInst, uCubeIrradiance);
                    r3d_shader_unbind_samplerCube(raster.forwardInst, uCubePrefilter);
                    r3d_shader_unbind_sampler2D(raster.forwardInst, uTexBrdfLut);
                }

                for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
                    r3d_shader_unbind_samplerCube(raster.forwardInst, uLights[i].shadowCubemap);
                    r3d_shader_unbind_sampler2D(raster.forwardInst, uLights[i].shadowMap);
                }
            }
            r3d_shader_disable();
        }

        // Render non-instanced meshes
        if (R3D.container.aDrawForward.count > 0) {
            r3d_shader_enable(raster.forward);
            {
                r3d_shader_bind_sampler2D(raster.forward, uTexNoise, R3D.texture.blueNoise);

                if (R3D.env.useSky) {
                    r3d_shader_bind_samplerCube(raster.forward, uCubeIrradiance, R3D.env.sky.irradiance.id);
                    r3d_shader_bind_samplerCube(raster.forward, uCubePrefilter, R3D.env.sky.prefilter.id);
                    r3d_shader_bind_sampler2D(raster.forward, uTexBrdfLut, R3D.texture.iblBrdfLut);

                    r3d_shader_set_vec4(raster.forward, uQuatSkybox, R3D.env.quatSky);
                    r3d_shader_set_int(raster.forward, uHasSkybox, true);
                }
                else {
                    r3d_shader_set_vec3(raster.forward, uColAmbient, R3D.env.ambientColor);
                    r3d_shader_set_int(raster.forward, uHasSkybox, false);
                }

                r3d_shader_set_vec3(raster.forward, uViewPosition, R3D.state.transform.position);

                for (int i = 0; i < R3D.container.aDrawForward.count; i++) {
                    r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawForward, i);
                    r3d_pass_scene_forward_filter_and_send_lights(call);
                    r3d_render_apply_blend_mode(call->forward.blendMode);
                    r3d_drawcall_raster_forward(call);
                }

                r3d_shader_unbind_sampler2D(raster.forward, uTexNoise);

                if (R3D.env.useSky) {
                    r3d_shader_unbind_samplerCube(raster.forward, uCubeIrradiance);
                    r3d_shader_unbind_samplerCube(raster.forward, uCubePrefilter);
                    r3d_shader_unbind_sampler2D(raster.forward, uTexBrdfLut);
                }

                for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
                    r3d_shader_unbind_samplerCube(raster.forward, uLights[i].shadowCubemap);
                    r3d_shader_unbind_sampler2D(raster.forward, uLights[i].shadowMap);
                }
            }
            r3d_shader_disable();
        }

        // Reset projection matrix
        rlMatrixMode(RL_PROJECTION);
        rlPopMatrix();

        // Reset view matrix
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
    }
}

void r3d_pass_post_init(unsigned int fb, unsigned srcAttach)
{
    r3d_gbuffer_disable_stencil();

    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.post.id);

    r3d_framebuffer_swap_pingpong(R3D.framebuffer.post);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R3D.framebuffer.post.id);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);

    glReadBuffer(srcAttach);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glBlitFramebuffer(
        0, 0, R3D.state.resolution.width, R3D.state.resolution.height,
        0, 0, R3D.state.resolution.width, R3D.state.resolution.height,
        GL_COLOR_BUFFER_BIT, GL_NEAREST
    );
}

void r3d_pass_post_bloom(void)
{
    rlEnableFramebuffer(R3D.framebuffer.mipChainBloom.id);
    {
        rlDisableColorBlend();
        rlDisableDepthTest();

        /* --- Bloom: Down Sampling --- */

        r3d_shader_enable(generate.downsampling);
        {
            r3d_shader_set_vec2(generate.downsampling, uResolution, (
                (Vector2) { R3D.state.resolution.width, R3D.state.resolution.height }
            ))
            r3d_shader_set_int(generate.downsampling, uMipLevel, 0);

            // Set prefilter cutoff values
            float knee = R3D.env.bloomThreshold * R3D.env.bloomSoftThreshold;
            Vector4 bloomPrefilter = {
                .x = R3D.env.bloomThreshold,
                .y = bloomPrefilter.x - knee,
                .z = 2.0f * knee,
                .w = 0.25f / (knee + 0.00001f)
            };
            r3d_shader_set_vec4(generate.downsampling, uPrefilter, bloomPrefilter);

            // Bind scene color (HDR color buffer) as initial texture input
            r3d_shader_bind_sampler2D(generate.downsampling, uTexture, R3D.framebuffer.scene.color);
        
            // Progressively downsample through the mip chain
            for (int i = 0; i < R3D.framebuffer.mipChainBloom.mipCount; i++) {
                const struct r3d_mip_bloom_t* mip = &R3D.framebuffer.mipChainBloom.mipChain[i];

                glViewport(0, 0, mip->iW, mip->iH);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip->id, 0);

                // Render screen-filled quad of resolution of current mip
                r3d_primitive_draw_screen();

                // Set current mip resolution as srcResolution for next iteration
                r3d_shader_set_vec2(generate.downsampling, uResolution, (
                    (Vector2) { mip->fW, mip->fH }
                ))

                // Set current mip as texture input for next iteration
                glBindTexture(GL_TEXTURE_2D, mip->id);

                // Disable Karis average for consequent downsamples
                r3d_shader_set_int(generate.downsampling, uMipLevel, 1);
            }
        }

        /* --- Bloom: Up Sampling --- */

        r3d_shader_enable(generate.upsampling);
        {
            // Enable additive blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_ADD);

            Vector2 filterRadius = {
                R3D.state.resolution.texelX * R3D.env.bloomFilterRadius,
                R3D.state.resolution.texelY * R3D.env.bloomFilterRadius
            };
            r3d_shader_set_vec2(generate.upsampling, uFilterRadius, filterRadius);
        
            for (int i = R3D.framebuffer.mipChainBloom.mipCount - 1; i > 0; i--) {
                const struct r3d_mip_bloom_t* mip = &R3D.framebuffer.mipChainBloom.mipChain[i];
                const struct r3d_mip_bloom_t* nextMip = &R3D.framebuffer.mipChainBloom.mipChain[i-1];

                // Bind viewport and texture from where to read
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, mip->id);

                // Set framebuffer render target (we write to this texture)
                glViewport(0, 0, nextMip->fW, nextMip->fH);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nextMip->id, 0);

                // Render screen-filled quad of resolution of current mip
                r3d_primitive_draw_screen();
            }
        
            // Disable additive blending
            glDisable(GL_BLEND);
        }
    }

    rlEnableFramebuffer(R3D.framebuffer.post.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        r3d_framebuffer_swap_pingpong(R3D.framebuffer.post);

        r3d_shader_enable(screen.bloom);
        {
            r3d_shader_bind_sampler2D(screen.bloom, uTexColor, R3D.framebuffer.post.source);
            r3d_shader_bind_sampler2D(screen.bloom, uTexBloomBlur, R3D.framebuffer.mipChainBloom.mipChain[0].id);

            r3d_shader_set_int(screen.bloom, uBloomMode, R3D.env.bloomMode);
            r3d_shader_set_float(screen.bloom, uBloomIntensity, R3D.env.bloomIntensity);

            r3d_primitive_draw_screen();
        }
        r3d_shader_disable();
    }
}

void r3d_pass_post_fog(void)
{
    rlEnableFramebuffer(R3D.framebuffer.post.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        r3d_framebuffer_swap_pingpong(R3D.framebuffer.post);

        r3d_shader_enable(screen.fog);
        {
            r3d_shader_bind_sampler2D(screen.fog, uTexColor, R3D.framebuffer.post.source);
            r3d_shader_bind_sampler2D(screen.fog, uTexDepth, R3D.framebuffer.gBuffer.depth);

            r3d_shader_set_float(screen.fog, uNear, (float)rlGetCullDistanceNear());
            r3d_shader_set_float(screen.fog, uFar, (float)rlGetCullDistanceFar());
            r3d_shader_set_int(screen.fog, uFogMode, R3D.env.fogMode);
            r3d_shader_set_vec3(screen.fog, uFogColor, R3D.env.fogColor);
            r3d_shader_set_float(screen.fog, uFogStart, R3D.env.fogStart);
            r3d_shader_set_float(screen.fog, uFogEnd, R3D.env.fogEnd);
            r3d_shader_set_float(screen.fog, uFogDensity, R3D.env.fogDensity);

            r3d_primitive_draw_screen();
        }
        r3d_shader_disable();
    }
}

void r3d_pass_post_tonemap(void)
{
    rlEnableFramebuffer(R3D.framebuffer.post.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        r3d_framebuffer_swap_pingpong(R3D.framebuffer.post);

        r3d_shader_enable(screen.tonemap);
        {
            r3d_shader_bind_sampler2D(screen.tonemap, uTexColor, R3D.framebuffer.post.source);

            r3d_shader_set_int(screen.tonemap, uTonemapMode, R3D.env.tonemapMode);
            r3d_shader_set_float(screen.tonemap, uTonemapExposure, R3D.env.tonemapExposure);
            r3d_shader_set_float(screen.tonemap, uTonemapWhite, R3D.env.tonemapWhite);

            r3d_primitive_draw_screen();
        }
        r3d_shader_disable();
    }
}

void r3d_pass_post_adjustment(void)
{
    rlEnableFramebuffer(R3D.framebuffer.post.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        r3d_framebuffer_swap_pingpong(R3D.framebuffer.post);

        r3d_shader_enable(screen.adjustment);
        {
            r3d_shader_bind_sampler2D(screen.adjustment, uTexColor, R3D.framebuffer.post.source);

            r3d_shader_set_float(screen.adjustment, uBrightness, R3D.env.brightness);
            r3d_shader_set_float(screen.adjustment, uContrast, R3D.env.contrast);
            r3d_shader_set_float(screen.adjustment, uSaturation, R3D.env.saturation);

            r3d_primitive_draw_screen();
        }
        r3d_shader_disable();
    }
}

void r3d_pass_post_fxaa(void)
{
    rlEnableFramebuffer(R3D.framebuffer.post.id);
    {
        rlViewport(0, 0, R3D.state.resolution.width, R3D.state.resolution.height);
        rlDisableColorBlend();
        rlDisableDepthTest();

        r3d_framebuffer_swap_pingpong(R3D.framebuffer.post);

        r3d_shader_enable(screen.fxaa);
        {
            r3d_shader_bind_sampler2D(screen.fxaa, uTexture, R3D.framebuffer.post.source);

            r3d_shader_set_vec2(screen.fxaa, uTexelSize, ((Vector2) {
                R3D.state.resolution.texelX,
                R3D.state.resolution.texelY
            }));

            r3d_primitive_draw_screen();
        }
        r3d_shader_disable();
    }
}

void r3d_pass_final_blit(void)
{
    unsigned int dstId = 0;
    int dstX = 0, dstY = 0;
    int dstW = GetScreenWidth();
    int dstH = GetScreenHeight();

    // If a custom final framebuffer is set, use its ID and dimensions
    if (R3D.framebuffer.customTarget.id != 0) {
        dstId = R3D.framebuffer.customTarget.id;
        dstW = R3D.framebuffer.customTarget.texture.width;
        dstH = R3D.framebuffer.customTarget.texture.height;
    }

    // Maintain aspect ratio if the corresponding flag is set
    if (R3D.state.flags & R3D_FLAG_ASPECT_KEEP) {
        float srcRatio = (float)R3D.state.resolution.width / R3D.state.resolution.height;
        float dstRatio = (float)dstW / dstH;
        if (srcRatio > dstRatio) {
            int prevH = dstH;
            dstH = (int)(dstW * srcRatio + 0.5f);
            dstY = (prevH - dstH) / 2;
        }
        else {
            int prevW = dstW;
            dstW = (int)(dstH * srcRatio + 0.5f);
            dstX = (prevW - dstW) / 2;
        }
    }

    // Bind the destination framebuffer for drawing
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstId);

    // Blit only the color data from the post-processing framebuffer to the main framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, R3D.framebuffer.post.id);
    glBlitFramebuffer(
        0, 0, R3D.state.resolution.width, R3D.state.resolution.height,
        dstX, dstY, dstX + dstW, dstY + dstH, GL_COLOR_BUFFER_BIT,
        (R3D.state.flags & R3D_FLAG_BLIT_LINEAR) ? GL_LINEAR : GL_NEAREST
    );

    // Blit the depth data from the gbuffer framebuffer to the main framebuffer
    glBindFramebuffer(GL_READ_FRAMEBUFFER, R3D.framebuffer.gBuffer.id);
    glBlitFramebuffer(
        0, 0, R3D.state.resolution.width, R3D.state.resolution.height,
        dstX, dstY, dstX + dstW, dstY + dstH,
        GL_DEPTH_BUFFER_BIT, GL_NEAREST
    );
}

void r3d_reset_raylib_state(void)
{
    rlDisableFramebuffer();

    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableColorBlend();
    rlDisableDepthTest();
    rlEnableDepthMask();

    glDepthFunc(GL_LEQUAL);
}
