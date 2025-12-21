/* r3d_draw.h -- R3D Draw Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_draw.h>
#include <raymath.h>
#include <stddef.h>
#include <assert.h>
#include <float.h>
#include <rlgl.h>
#include <glad.h>

#include "./details/r3d_drawcall.h"
#include "./details/r3d_math.h"

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_light.h"
#include "./r3d_state.h"

// ========================================
// HELPER MACROS
// ========================================

#define R3D_SHADOW_CAST_ONLY_MASK ( \
    (1 << R3D_SHADOW_CAST_ONLY_AUTO) | \
    (1 << R3D_SHADOW_CAST_ONLY_DOUBLE_SIDED) | \
    (1 << R3D_SHADOW_CAST_ONLY_FRONT_SIDE) | \
    (1 << R3D_SHADOW_CAST_ONLY_BACK_SIDE) \
)

#define R3D_IS_SHADOW_CAST_ONLY(mode) \
    ((R3D_SHADOW_CAST_ONLY_MASK & (1 << (mode))) != 0)

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool r3d_has_deferred_calls(void);
static bool r3d_has_forward_calls(void);

static void r3d_prepare_cull_drawcalls(void);
static void r3d_prepare_sort_drawcalls(void);

static void r3d_pass_scene_shadows(void);
static void r3d_pass_scene_geometry(void);
static void r3d_pass_scene_decals(void);

static r3d_target_t r3d_pass_prepare_ssao(void);

static void r3d_pass_deferred_ambient(r3d_target_t ssaoSource);
static void r3d_pass_deferred_lights(r3d_target_t ssaoSource);
static void r3d_pass_deferred_compose(r3d_target_t sceneTarget);

static void r3d_pass_scene_forward(r3d_target_t sceneTarget);
static void r3d_pass_scene_background(r3d_target_t sceneTarget);

static r3d_target_t r3d_pass_post_setup(r3d_target_t sceneTarget);
static r3d_target_t r3d_pass_post_ssr(r3d_target_t sceneTarget);
static r3d_target_t r3d_pass_post_fog(r3d_target_t sceneTarget);
static r3d_target_t r3d_pass_post_dof(r3d_target_t sceneTarget);
static r3d_target_t r3d_pass_post_bloom(r3d_target_t sceneTarget);
static r3d_target_t r3d_pass_post_output(r3d_target_t sceneTarget);
static r3d_target_t r3d_pass_post_fxaa(r3d_target_t sceneTarget);

static void r3d_reset_raylib_state(void);

// ========================================
// PUBLIC API
// ========================================

void R3D_Begin(Camera3D camera)
{
    R3D_BeginEx(camera, NULL);
}

void R3D_BeginEx(Camera3D camera, const RenderTexture* target)
{
    r3d_mod_target_set_blit_screen(target);

    /* --- Render the rlgl batch before proceeding --- */

    rlDrawRenderBatchActive();

    /* --- Clear the previous draw call array state --- */

    r3d_array_clear(&R3D.container.aDrawForward);
    r3d_array_clear(&R3D.container.aDrawDeferred);
    r3d_array_clear(&R3D.container.aDrawForwardInst);
    r3d_array_clear(&R3D.container.aDrawDeferredInst);
    r3d_array_clear(&R3D.container.aDrawDecals);
    r3d_array_clear(&R3D.container.aDrawDecalsInst);

    /* --- Store camera position --- */

    R3D.state.transform.viewPos = camera.position;

    /* --- Compute projection matrix --- */

    float aspect = r3d_mod_target_get_render_aspect();

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

    /* --- Compute view/proj matrices --- */

    R3D.state.transform.view = MatrixLookAt(camera.position, camera.target, camera.up);
    R3D.state.transform.invProj = MatrixInvert(R3D.state.transform.proj);
    R3D.state.transform.invView = MatrixInvert(R3D.state.transform.view);
    R3D.state.transform.viewProj = r3d_matrix_multiply(&R3D.state.transform.view, &R3D.state.transform.proj);

    /* --- Compute frustum --- */

    R3D.state.frustum.aabb = r3d_frustum_get_bounding_box(R3D.state.transform.viewProj);
    R3D.state.frustum.shape = r3d_frustum_create(R3D.state.transform.viewProj);
}

void R3D_End(void)
{
    /* --- Rendering in shadow maps --- */

    r3d_light_update_and_cull(&R3D.state.frustum.shape, R3D.state.transform.viewPos);
    r3d_pass_scene_shadows();

    /* --- Process all draw calls before rendering --- */

    r3d_prepare_cull_drawcalls();
    r3d_prepare_sort_drawcalls();

    /* --- Opaque and decal rendering with deferred lighting and composition --- */

    r3d_target_t sceneTarget = R3D_TARGET_SCENE_0;

    if (r3d_has_deferred_calls())
    {
        R3D_TARGET_CLEAR(R3D_TARGET_GBUFFER);

        r3d_pass_scene_geometry();  // Sceneizing geometries in G-Buffer
        r3d_pass_scene_decals();    // Sceneizing decals in G-Buffer

        r3d_target_t ssaoSource = R3D_TARGET_INVALID;
        if (R3D.env.ssaoEnabled) {
            ssaoSource = r3d_pass_prepare_ssao();
        }

        r3d_pass_deferred_ambient(ssaoSource);
        r3d_pass_deferred_lights(ssaoSource);

        r3d_pass_deferred_compose(sceneTarget);
    }
    else
    {
        R3D_TARGET_CLEAR(R3D_TARGET_DEPTH);
    }

    /* --- Then transparent and background rendering --- */

    if (r3d_has_forward_calls()) {
        r3d_pass_scene_forward(sceneTarget);
    }

    r3d_pass_scene_background(sceneTarget);

    /* --- Applying effects over the scene and final blit --- */

    sceneTarget = r3d_pass_post_setup(sceneTarget);

    if (R3D.env.ssrEnabled) {
        sceneTarget = r3d_pass_post_ssr(sceneTarget);
    }

    if (R3D.env.fogMode != R3D_FOG_DISABLED) {
        sceneTarget = r3d_pass_post_fog(sceneTarget);
    }

    if (R3D.env.dofMode != R3D_DOF_DISABLED) {
        sceneTarget = r3d_pass_post_dof(sceneTarget);
    }

    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        sceneTarget = r3d_pass_post_bloom(sceneTarget);
    }

    sceneTarget = r3d_pass_post_output(sceneTarget);

    if (R3D.state.flags & R3D_FLAG_FXAA) {
        sceneTarget = r3d_pass_post_fxaa(sceneTarget);
    }

    r3d_target_blit(r3d_target_swap_scene(sceneTarget));

    /* --- Reset states changed by R3D --- */

    r3d_reset_raylib_state();
}

void R3D_DrawMesh(const R3D_Mesh* mesh, const R3D_Material* material, Matrix transform)
{
    if ((mesh->layerMask & R3D.state.layers) == 0) {
        return;
    }

    r3d_drawcall_t drawCall = {0};
    r3d_array_t* callArray = NULL;

    drawCall.mesh = *mesh;
    drawCall.transform = transform;
    drawCall.material = material ? *material : R3D_GetDefaultMaterial();

    if (drawCall.material.blendMode == R3D_BLEND_OPAQUE) {
        drawCall.renderMode = R3D_DRAWCALL_RENDER_DEFERRED;
        callArray = &R3D.container.aDrawDeferred;
    }
    else {
        drawCall.renderMode = R3D_DRAWCALL_RENDER_FORWARD;
        callArray = &R3D.container.aDrawForward;
    }

    r3d_array_push_back(callArray, &drawCall);
}

void R3D_DrawMeshInstanced(const R3D_Mesh* mesh, const R3D_Material* material, const Matrix* instanceTransforms, int instanceCount)
{
    R3D_DrawMeshInstancedPro(mesh, material, NULL, MatrixIdentity(), instanceTransforms, 0, NULL, 0, instanceCount);
}

void R3D_DrawMeshInstancedEx(const R3D_Mesh* mesh, const R3D_Material* material, const Matrix* instanceTransforms, const Color* instanceColors, int instanceCount)
{
    R3D_DrawMeshInstancedPro(mesh, material, NULL, MatrixIdentity(), instanceTransforms, 0, instanceColors, 0, instanceCount);
}

void R3D_DrawMeshInstancedPro(const R3D_Mesh* mesh, const R3D_Material* material,
                              const BoundingBox* globalAabb, Matrix globalTransform,
                              const Matrix* instanceTransforms, int transformsStride,
                              const Color* instanceColors, int colorsStride,
                              int instanceCount)
{
    if ((mesh->layerMask & R3D.state.layers) == 0) {
        return;
    }

    if (instanceCount == 0 || instanceTransforms == NULL) {
        return;
    }

    r3d_drawcall_t drawCall = {0};
    r3d_array_t* callArray = NULL;

    drawCall.mesh = *mesh;
    drawCall.transform = globalTransform;
    drawCall.material = material ? *material : R3D_GetDefaultMaterial();

    if (drawCall.material.blendMode == R3D_BLEND_OPAQUE) {
        drawCall.renderMode = R3D_DRAWCALL_RENDER_DEFERRED;
        callArray = &R3D.container.aDrawDeferredInst;
    }
    else {
        drawCall.renderMode = R3D_DRAWCALL_RENDER_FORWARD;
        callArray = &R3D.container.aDrawForwardInst;
    }

    drawCall.instanced.allAabb = globalAabb ? *globalAabb
        : (BoundingBox) {
            {-FLT_MAX, -FLT_MAX, -FLT_MAX},
            {+FLT_MAX, +FLT_MAX, +FLT_MAX}
        };

    drawCall.instanced.transforms = instanceTransforms;
    drawCall.instanced.transStride = transformsStride;
    drawCall.instanced.colStride = colorsStride;
    drawCall.instanced.colors = instanceColors;
    drawCall.instanced.count = instanceCount;

    r3d_array_push_back(callArray, &drawCall);
}

void R3D_DrawModel(const R3D_Model* model, Vector3 position, float scale)
{
    Vector3 vScale = { scale, scale, scale };
    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    R3D_DrawModelEx(model, position, rotationAxis, 0.0f, vScale);
}

void R3D_DrawModelEx(const R3D_Model* model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale)
{
    Matrix matTransform = r3d_matrix_scale_rotaxis_translate(
        scale,
        (Vector4) {
            rotationAxis.x,
            rotationAxis.y,
            rotationAxis.z,
            rotationAngle
        },
        position
    );

    R3D_DrawModelPro(model, matTransform);
}

void R3D_DrawModelPro(const R3D_Model* model, Matrix transform)
{
    for (int i = 0; i < model->meshCount; i++)
    {
        const R3D_Mesh* mesh = &model->meshes[i];
        if ((mesh->layerMask & R3D.state.layers) == 0) {
            continue;
        }

        r3d_drawcall_t drawCall = {0};
        r3d_array_t* callArray = NULL;

        const R3D_Material* material = &model->materials[model->meshMaterials[i]];

        drawCall.mesh = *mesh;
        drawCall.transform = transform;
        drawCall.player = model->player;
        drawCall.skeleton = model->skeleton;
        drawCall.material = material ? *material : R3D_GetDefaultMaterial();

        if (drawCall.material.blendMode == R3D_BLEND_OPAQUE) {
            drawCall.renderMode = R3D_DRAWCALL_RENDER_DEFERRED;
            callArray = &R3D.container.aDrawDeferred;
        }
        else {
            drawCall.renderMode = R3D_DRAWCALL_RENDER_FORWARD;
            callArray = &R3D.container.aDrawForward;
        }

        r3d_array_push_back(callArray, &drawCall);
    }
}

void R3D_DrawModelInstanced(const R3D_Model* model, const Matrix* instanceTransforms, int instanceCount)
{
    R3D_DrawModelInstancedPro(model, NULL, MatrixIdentity(), instanceTransforms, 0, NULL, 0, instanceCount);
}

void R3D_DrawModelInstancedEx(const R3D_Model* model, const Matrix* instanceTransforms, const Color* instanceColors, int instanceCount)
{
    R3D_DrawModelInstancedPro(model, NULL, MatrixIdentity(), instanceTransforms, 0, instanceColors, 0, instanceCount);
}

void R3D_DrawModelInstancedPro(const R3D_Model* model,
                               const BoundingBox* globalAabb, Matrix globalTransform,
                               const Matrix* instanceTransforms, int transformsStride,
                               const Color* instanceColors, int colorsStride,
                               int instanceCount)
{
    if (model == NULL || instanceCount == 0 || instanceTransforms == NULL || model->meshCount == 0) {
        return;
    }

    BoundingBox computedAabb;
    if (globalAabb == NULL) {
        computedAabb = (BoundingBox) {
            {-FLT_MAX, -FLT_MAX, -FLT_MAX},
            {+FLT_MAX, +FLT_MAX, +FLT_MAX}
        };
        globalAabb = &computedAabb;
    }

    r3d_array_t* deferredArr = &R3D.container.aDrawDeferredInst;
    r3d_array_t* forwardArr = &R3D.container.aDrawForwardInst;

    r3d_array_reserve(deferredArr, deferredArr->count + model->meshCount);
    r3d_array_reserve(forwardArr, forwardArr->count + model->meshCount);

    for (int i = 0; i < model->meshCount; i++)
    {
        const R3D_Mesh* mesh = &model->meshes[i];
        if ((mesh->layerMask & R3D.state.layers) == 0) {
            continue;
        }

        r3d_drawcall_t drawCall = {0};
        r3d_array_t* callArray = NULL;

        const R3D_Material* material = &model->materials[model->meshMaterials[i]];

        drawCall.mesh = *mesh;
        drawCall.material = *material;
        drawCall.player = model->player;
        drawCall.skeleton = model->skeleton;
        drawCall.transform = globalTransform;

        if (drawCall.material.blendMode != R3D_BLEND_OPAQUE) {
            drawCall.renderMode = R3D_DRAWCALL_RENDER_DEFERRED;
            callArray = &R3D.container.aDrawDeferredInst;
        }
        else {
            drawCall.renderMode = R3D_DRAWCALL_RENDER_FORWARD;
            callArray = &R3D.container.aDrawForwardInst;
        }
        
        drawCall.instanced.allAabb = *globalAabb;
        drawCall.instanced.transforms = instanceTransforms;
        drawCall.instanced.transStride = transformsStride;
        drawCall.instanced.colStride = colorsStride;
        drawCall.instanced.colors = instanceColors;
        drawCall.instanced.count = instanceCount;

        r3d_array_push_back(callArray, &drawCall);
    }
}

void R3D_DrawDecal(const R3D_Decal* decal, Matrix transform)
{
    r3d_drawcall_t drawCall = {0};

    drawCall.transform = transform;
    drawCall.material = decal->material;
    drawCall.material.depthMode = R3D_DEPTH_READ_ONLY;
    drawCall.mesh = R3D.misc.meshDecalBounds;
    drawCall.renderMode = R3D_DRAWCALL_RENDER_DEFERRED;

    r3d_array_push_back(&R3D.container.aDrawDecals, &drawCall);
}

void R3D_DrawDecalInstanced(const R3D_Decal* decal, const Matrix* instanceTransforms, int instanceCount)
{
    r3d_drawcall_t drawCall = {0};

    drawCall.transform = MatrixIdentity();
    drawCall.material = decal->material;
    drawCall.material.depthMode = R3D_DEPTH_READ_ONLY;
    drawCall.mesh = R3D.misc.meshDecalBounds;
    drawCall.renderMode = R3D_DRAWCALL_RENDER_DEFERRED;

    // TODO: Move aabb evaluation to potential Pro version of this function
    drawCall.instanced.allAabb = (BoundingBox) {
        { -FLT_MAX, -FLT_MAX, -FLT_MAX },
        { +FLT_MAX, +FLT_MAX, +FLT_MAX }
    };

    drawCall.instanced.transforms = instanceTransforms;
    drawCall.instanced.transStride = 0;
    drawCall.instanced.colStride = 0;
    drawCall.instanced.colors = NULL;
    drawCall.instanced.count = instanceCount;

    r3d_array_push_back(&R3D.container.aDrawDecalsInst, &drawCall);
}

void R3D_DrawParticleSystem(const R3D_ParticleSystem* system, const R3D_Mesh* mesh, const R3D_Material* material)
{
    R3D_DrawParticleSystemEx(system, mesh, material, MatrixIdentity());
}

void R3D_DrawParticleSystemEx(const R3D_ParticleSystem* system, const R3D_Mesh* mesh, const R3D_Material* material, Matrix transform)
{
    if (system == NULL || mesh == NULL) {
        return;
    }

    R3D_DrawMeshInstancedPro(
        mesh, material, &system->aabb, transform,
        &system->particles->transform, sizeof(R3D_Particle),
        &system->particles->color, sizeof(R3D_Particle),
        system->count
    );
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool r3d_has_deferred_calls(void)
{
    return (R3D.container.aDrawDeferred.count > 0 || R3D.container.aDrawDeferredInst.count > 0);
}

static bool r3d_has_forward_calls(void)
{
    return (R3D.container.aDrawForward.count > 0 || R3D.container.aDrawForwardInst.count > 0);
}

void r3d_prepare_cull_drawcalls(void)
{
    r3d_drawcall_t* calls = NULL;
    int count = 0;

    /* --- Frustum culling of deferred objects --- */

    calls = (r3d_drawcall_t*)R3D.container.aDrawDeferred.data;
    count = (int)R3D.container.aDrawDeferred.count;

    for (int i = count - 1; i >= 0; i--) {
        if (R3D_IS_SHADOW_CAST_ONLY(calls->mesh.shadowCastMode)) {
            calls[i] = calls[--count];
            continue;
        }
        if (!(R3D.state.flags & R3D_FLAG_NO_FRUSTUM_CULLING)) {
            if (!r3d_drawcall_geometry_is_visible(&calls[i])) {
                calls[i] = calls[--count];
                continue;
            }
        }
    }

    R3D.container.aDrawDeferred.count = count;

    /* --- Frustum culling of deferred decal objects --- */

    calls = (r3d_drawcall_t*)R3D.container.aDrawDecals.data;
    count = (int)R3D.container.aDrawDecals.count;

    for (int i = count - 1; i >= 0; i--) {
        if (!(R3D.state.flags & R3D_FLAG_NO_FRUSTUM_CULLING)) {
            if (!r3d_drawcall_geometry_is_visible(&calls[i])) {
                calls[i] = calls[--count];
                continue;
            }
        }
    }

    R3D.container.aDrawDecals.count = count;

    /* --- Frustum culling of forward objects --- */

    calls = (r3d_drawcall_t*)R3D.container.aDrawForward.data;
    count = (int)R3D.container.aDrawForward.count;

    for (int i = count - 1; i >= 0; i--) {
        if (R3D_IS_SHADOW_CAST_ONLY(calls->mesh.shadowCastMode)) {
            calls[i] = calls[--count];
            continue;
        }
        if (!(R3D.state.flags & R3D_FLAG_NO_FRUSTUM_CULLING)) {
            if (!r3d_drawcall_geometry_is_visible(&calls[i])) {
                calls[i] = calls[--count];
                continue;
            }
        }
    }

    R3D.container.aDrawForward.count = count;

    /* --- Frustum culling of deferred instanced objects --- */
    
    calls = (r3d_drawcall_t*)R3D.container.aDrawDeferredInst.data;
    count = (int)R3D.container.aDrawDeferredInst.count;

    for (int i = count - 1; i >= 0; i--) {
        if (R3D_IS_SHADOW_CAST_ONLY(calls->mesh.shadowCastMode)) {
            calls[i] = calls[--count];
            continue;
        }
        if (!(R3D.state.flags & R3D_FLAG_NO_FRUSTUM_CULLING)) {
            if (!r3d_drawcall_instanced_geometry_is_visible(&calls[i])) {
                calls[i] = calls[--count];
                continue;
            }
        }
    }
    
    R3D.container.aDrawDeferredInst.count = count;

    /* --- Frustum culling of forward instanced objects --- */

    calls = (r3d_drawcall_t*)R3D.container.aDrawForwardInst.data;
    count = (int)R3D.container.aDrawForwardInst.count;

    for (int i = count - 1; i >= 0; i--) {
        if (R3D_IS_SHADOW_CAST_ONLY(calls->mesh.shadowCastMode)) {
            calls[i] = calls[--count];
            continue;
        }
        if (!(R3D.state.flags & R3D_FLAG_NO_FRUSTUM_CULLING)) {
            if (!r3d_drawcall_instanced_geometry_is_visible(&calls[i])) {
                calls[i] = calls[--count];
                continue;
            }
        }
    }

    R3D.container.aDrawForwardInst.count = count;
}

void r3d_prepare_sort_drawcalls(void)
{
    // Sort front-to-back for deferred rendering
    if (R3D.state.flags & R3D_FLAG_OPAQUE_SORTING) {
        r3d_drawcall_sort_front_to_back(
            (r3d_drawcall_t*)R3D.container.aDrawDeferred.data,
            R3D.container.aDrawDeferred.count
        );
    }

    // Sort back-to-front for deferred decal rendering
    r3d_drawcall_sort_back_to_front(
        (r3d_drawcall_t*)R3D.container.aDrawDecals.data,
        R3D.container.aDrawDecals.count
    );

    // Sort back-to-front for forward rendering
    if (R3D.state.flags & R3D_FLAG_TRANSPARENT_SORTING) {
        r3d_drawcall_sort_back_to_front(
            (r3d_drawcall_t*)R3D.container.aDrawForward.data,
            R3D.container.aDrawForward.count
        );
    }
}

void r3d_pass_scene_shadows(void)
{
    /* --- Config context state --- */

    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);

    /* --- Iterate through all lights to render all geometries --- */

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        /* --- Skip if it's not time to update shadows --- */

        if (!r3d_light_shadow_should_be_upadted(light, true)) {
            continue;
        }

        /* --- Start rendering to shadow map --- */

        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowMap.fbo);
        {
            glViewport(0, 0, light->shadowMap.resolution, light->shadowMap.resolution);

            if (light->type == R3D_LIGHT_OMNI)
            {
                /* --- Setup shader --- */

                R3D_SHADER_USE(scene.depthCube);
                R3D_SHADER_SET_FLOAT(scene.depthCube, uFar, light->far);
                R3D_SHADER_SET_VEC3(scene.depthCube, uViewPosition, light->position);

                /* --- Render geometries for each face of the cubemap --- */

                for (int iFace = 0; iFace < 6; iFace++)
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + iFace, light->shadowMap.tex, 0);
                    glClear(GL_DEPTH_BUFFER_BIT);

                    /* --- First sceneize instanced geometries to the shadow map --- */

                    for (size_t iCall = 0; iCall < R3D.container.aDrawDeferredInst.count; iCall++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferredInst.data + iCall;
                        if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_drawcall_raster_depth_cube(call, true, &light->matVP[iFace]);
                        }
                    }

                    for (size_t iCall = 0; iCall < R3D.container.aDrawForwardInst.count; iCall++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForwardInst.data + iCall;
                        if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_drawcall_raster_depth_cube(call, true, &light->matVP[iFace]);
                        }
                    }

                    /* --- Then sceneize non-instanced geometries to the shadow map --- */

                    for (size_t iCall = 0; iCall < R3D.container.aDrawDeferred.count; iCall++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferred.data + iCall;
                        if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_drawcall_raster_depth_cube(call, true, &light->matVP[iFace]);
                        }
                    }

                    for (size_t iCall = 0; iCall < R3D.container.aDrawForward.count; iCall++) {
                        r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForward.data + iCall;
                        if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                            r3d_drawcall_raster_depth_cube(call, true, &light->matVP[iFace]);
                        }
                    }

                    /* --- The bone matrices texture may have been bind during drawcalls, so UNBIND! --- */

                    R3D_SHADER_UNBIND_SAMPLER_1D(scene.depthCube, uTexBoneMatrices);
                }
            }
            else
            {
                /* --- Setup dir/spot shadow map pass --- */

                glClear(GL_DEPTH_BUFFER_BIT);   // Clear depth buffer for these types of light
                R3D_SHADER_USE(scene.depth);

                /* --- First sceneize instanced geometries to the shadow map --- */

                for (size_t iCall = 0; iCall < R3D.container.aDrawDeferredInst.count; iCall++) {
                    r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferredInst.data + iCall;
                    if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                        r3d_drawcall_raster_depth(call, true, &light->matVP[0]);
                    }
                }
                for (size_t iCall = 0; iCall < R3D.container.aDrawForwardInst.count; iCall++) {
                    r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForwardInst.data + iCall;
                    if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                        r3d_drawcall_raster_depth(call, true, &light->matVP[0]);
                    }
                }

                /* --- Then sceneize non-instanced geometries to the shadow map --- */

                for (size_t iCall = 0; iCall < R3D.container.aDrawDeferred.count; iCall++) {
                    r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawDeferred.data + iCall;
                    if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                        r3d_drawcall_raster_depth(call, true, &light->matVP[0]);
                    }
                }
                for (size_t iCall = 0; iCall < R3D.container.aDrawForward.count; iCall++) {
                    r3d_drawcall_t* call = (r3d_drawcall_t*)R3D.container.aDrawForward.data + iCall;
                    if (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED) {
                        r3d_drawcall_raster_depth(call, true, &light->matVP[0]);
                    }
                }

                /* --- The bone matrices texture may have been bind during drawcalls, so UNBIND! --- */

                R3D_SHADER_UNBIND_SAMPLER_1D(scene.depth, uTexBoneMatrices);
            }
        }
    }
}

void r3d_pass_scene_geometry(void)
{
    R3D_TARGET_BIND(R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.geometry);

    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);

    for (size_t i = 0; i < R3D.container.aDrawDeferredInst.count; i++) {
        const r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawDeferredInst, i);
        r3d_drawcall_raster_geometry(call, &R3D.state.transform.viewProj);
    }

    for (size_t i = 0; i < R3D.container.aDrawDeferred.count; i++) {
        const r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawDeferred, i);
        r3d_drawcall_raster_geometry(call, &R3D.state.transform.viewProj);
    }

    // NOTE: The storage texture of the matrices may have been bind during drawcalls
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.geometry, uTexBoneMatrices);
}

void r3d_pass_scene_decals(void)
{
    R3D_TARGET_BIND(R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.decal);

    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    for (size_t i = 0; i < R3D.container.aDrawDecalsInst.count; i++) {
        const r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawDecalsInst, i);
        r3d_drawcall_raster_decal(call, &R3D.state.transform.viewProj);
    }

    for (size_t i = 0; i < R3D.container.aDrawDecals.count; i++) {
        const r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawDecals, i);
        r3d_drawcall_raster_decal(call, &R3D.state.transform.viewProj);
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexDepth);
}

r3d_target_t r3d_pass_prepare_ssao(void)
{
    glDisable(GL_DEPTH_TEST);   //< Can't depth test to touch only the geometry, since the target is half res...
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    /* --- Calculate SSAO --- */

    r3d_target_t ssaoTarget = R3D_TARGET_SSAO_0;
    R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);

    R3D_SHADER_USE(prepare.ssao);

    R3D_SHADER_SET_MAT4(prepare.ssao, uMatInvProj, R3D.state.transform.invProj);
    R3D_SHADER_SET_MAT4(prepare.ssao, uMatProj, R3D.state.transform.proj);
    R3D_SHADER_SET_MAT4(prepare.ssao, uMatView, R3D.state.transform.view);

    R3D_SHADER_SET_FLOAT(prepare.ssao, uRadius, R3D.env.ssaoRadius);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uBias, R3D.env.ssaoBias);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uIntensity, R3D.env.ssaoIntensity);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uPower, R3D.env.ssaoPower);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_1D(prepare.ssao, uTexKernel, r3d_texture_get(R3D_TEXTURE_SSAO_KERNEL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uTexNoise, r3d_texture_get(R3D_TEXTURE_SSAO_NOISE));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uTexDepth);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_1D(prepare.ssao, uTexKernel);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uTexNoise);

    /* --- Blur SSAO --- */

    R3D_SHADER_USE(prepare.ssaoBlur);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssaoBlur, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssaoBlur, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));
    R3D_SHADER_SET_MAT4(prepare.ssaoBlur, uMatInvProj, R3D.state.transform.invProj);

    for (int i = 0; i < R3D.env.ssaoIterations; i++)
    {
        // Horizontal pass
        R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);
        R3D_SHADER_BIND_SAMPLER_2D(prepare.ssaoBlur, uTexOcclusion, r3d_target_get(ssaoTarget));
        R3D_SHADER_SET_VEC2(prepare.ssaoBlur, uDirection, (Vector2) { 1.0, 0 });
        R3D_PRIMITIVE_DRAW_SCREEN();

        // Vertical pass
        R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);
        R3D_SHADER_BIND_SAMPLER_2D(prepare.ssaoBlur, uTexOcclusion, r3d_target_get(ssaoTarget));
        R3D_SHADER_SET_VEC2(prepare.ssaoBlur, uDirection, (Vector2) { 0, 1.0 });
        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssaoBlur, uTexOcclusion);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssaoBlur, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssaoBlur, uTexDepth);

    return r3d_target_swap_ssao(ssaoTarget);
}

void r3d_pass_deferred_ambient(r3d_target_t ssaoSource)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    /* --- Calculate skybox IBL contribution --- */

    if (R3D.env.useSky)
    {
        R3D_TARGET_BIND(R3D_TARGET_LIGHTING);
        R3D_SHADER_USE(deferred.ambientIbl);

        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexSSAO, R3D_TEXTURE_SELECT(r3d_target_get(ssaoSource), WHITE));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexORM, r3d_target_get(R3D_TARGET_ORM));
        R3D_SHADER_BIND_SAMPLER_CUBE(deferred.ambientIbl, uCubeIrradiance, R3D.env.sky.irradiance.id);
        R3D_SHADER_BIND_SAMPLER_CUBE(deferred.ambientIbl, uCubePrefilter, R3D.env.sky.prefilter.id);
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexBrdfLut, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

        R3D_SHADER_SET_VEC3(deferred.ambientIbl, uViewPosition, R3D.state.transform.viewPos);
        R3D_SHADER_SET_MAT4(deferred.ambientIbl, uMatInvProj, R3D.state.transform.invProj);
        R3D_SHADER_SET_MAT4(deferred.ambientIbl, uMatInvView, R3D.state.transform.invView);
        R3D_SHADER_SET_VEC4(deferred.ambientIbl, uQuatSkybox, R3D.env.quatSky);
        R3D_SHADER_SET_FLOAT(deferred.ambientIbl, uSkyboxAmbientIntensity, R3D.env.skyAmbientIntensity);
        R3D_SHADER_SET_FLOAT(deferred.ambientIbl, uSkyboxReflectIntensity, R3D.env.skyReflectIntensity);

        R3D_PRIMITIVE_DRAW_SCREEN();

        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexAlbedo);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexNormal);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexDepth);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexSSAO);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexORM);
        R3D_SHADER_UNBIND_SAMPLER_CUBE(deferred.ambientIbl, uCubeIrradiance);
        R3D_SHADER_UNBIND_SAMPLER_CUBE(deferred.ambientIbl, uCubePrefilter);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexBrdfLut);
    }

    /* --- If no skybox, calculate simple ambient contribution --- */

    else
    {
        R3D_TARGET_CLEAR(R3D_TARGET_SPECULAR);
        R3D_TARGET_BIND(R3D_TARGET_DIFFUSE, R3D_TARGET_DEPTH);

        R3D_SHADER_USE(deferred.ambient);

        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexSSAO, R3D_TEXTURE_SELECT(r3d_target_get(ssaoSource), WHITE));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexORM, r3d_target_get(R3D_TARGET_ORM));

        R3D_SHADER_SET_VEC3(deferred.ambient, uAmbientLight, R3D.env.ambientLight);

        R3D_PRIMITIVE_DRAW_SCREEN();

        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexAlbedo);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexSSAO);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexORM);
    }
}

void r3d_pass_deferred_lights(r3d_target_t ssaoSource)
{
    /* --- Setup OpenGL pipeline --- */

    R3D_TARGET_BIND(R3D_TARGET_LIGHTING);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);

    // Set additive blending to accumulate light contributions
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    /* --- Enable shader and setup constant stuff --- */

    R3D_SHADER_USE(deferred.lighting);

    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexSSAO, R3D_TEXTURE_SELECT(r3d_target_get(ssaoSource), WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexORM, r3d_target_get(R3D_TARGET_ORM));

    R3D_SHADER_SET_VEC3(deferred.lighting, uViewPosition, R3D.state.transform.viewPos);
    R3D_SHADER_SET_MAT4(deferred.lighting, uMatInvProj, R3D.state.transform.invProj);
    R3D_SHADER_SET_MAT4(deferred.lighting, uMatInvView, R3D.state.transform.invView);
    R3D_SHADER_SET_FLOAT(deferred.lighting, uSSAOLightAffect, R3D.env.ssaoLightAffect);

    /* --- Calculate lighting contributions --- */

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        int x = 0, y = 0;
        int w = R3D_TARGET_WIDTH;
        int h = R3D_TARGET_HEIGHT;

        if (light->type != R3D_LIGHT_DIR) {
            Vector3 min = light->aabb.min;
            Vector3 max = light->aabb.max;

            Vector2 minNDC = {FLT_MAX, FLT_MAX};
            Vector2 maxNDC = {-FLT_MAX, -FLT_MAX};

            bool allInside = true;
            for (int i = 0; i < 8; i++) {
                Vector4 corner = {(i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z, 1.0f};
                Vector4 clip = r3d_vector4_transform(corner, &R3D.state.transform.viewProj);
                if (clip.w <= 0.0f) { allInside = false; break; }

                Vector2 ndc = Vector2Scale((Vector2){clip.x, clip.y}, 1.0f / clip.w);
                minNDC = Vector2Min(minNDC, ndc);
                maxNDC = Vector2Max(maxNDC, ndc);
            }

            if (allInside) {
                x = (int)fmaxf((minNDC.x * 0.5f + 0.5f) * R3D_TARGET_WIDTH, 0.0f);
                y = (int)fmaxf((minNDC.y * 0.5f + 0.5f) * R3D_TARGET_HEIGHT, 0.0f);
                w = (int)fminf((maxNDC.x * 0.5f + 0.5f) * R3D_TARGET_WIDTH, (float)R3D_TARGET_WIDTH) - x;
                h = (int)fminf((maxNDC.y * 0.5f + 0.5f) * R3D_TARGET_HEIGHT, (float)R3D_TARGET_HEIGHT) - y;
                assert(w > 0 && h > 0); // This should never happen if the upstream frustum culling of the lights is correct.
            }
        }

        glScissor(x, y, w, h);

        // Sending data common to each type of light
        R3D_SHADER_SET_VEC3(deferred.lighting, uLight.color, light->color);
        R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.specular, light->specular);
        R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.energy, light->energy);
        R3D_SHADER_SET_INT(deferred.lighting, uLight.type, light->type);

        // Sending specific data according to the type of light
        if (light->type == R3D_LIGHT_DIR) {
            R3D_SHADER_SET_VEC3(deferred.lighting, uLight.direction, light->direction);
        }
        else if (light->type == R3D_LIGHT_SPOT) {
            R3D_SHADER_SET_VEC3(deferred.lighting, uLight.position, light->position);
            R3D_SHADER_SET_VEC3(deferred.lighting, uLight.direction, light->direction);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.range, light->range);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.attenuation, light->attenuation);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.innerCutOff, light->innerCutOff);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.outerCutOff, light->outerCutOff);
        }
        else if (light->type == R3D_LIGHT_OMNI) {
            R3D_SHADER_SET_VEC3(deferred.lighting, uLight.position, light->position);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.range, light->range);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.attenuation, light->attenuation);
        }

        // Sending shadow map data
        if (light->shadow) {
            if (light->type == R3D_LIGHT_OMNI) {
                R3D_SHADER_BIND_SAMPLER_CUBE(deferred.lighting, uLight.shadowCubemap, light->shadowMap.tex);
            }
            else {
                R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.shadowTexelSize, light->shadowTexelSize);
                R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uLight.shadowMap, light->shadowMap.tex);
                R3D_SHADER_SET_MAT4(deferred.lighting, uLight.matVP, light->matVP);
                if (light->type == R3D_LIGHT_DIR) {
                    R3D_SHADER_SET_VEC3(deferred.lighting, uLight.position, light->position);
                }
            }
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.shadowSoftness, light->shadowSoftness);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.shadowDepthBias, light->shadowDepthBias);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.shadowSlopeBias, light->shadowSlopeBias);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.near, light->near);
            R3D_SHADER_SET_FLOAT(deferred.lighting, uLight.far, light->far);
            R3D_SHADER_SET_INT(deferred.lighting, uLight.shadow, true);
        }
        else {
            R3D_SHADER_SET_INT(deferred.lighting, uLight.shadow, false);
        }

        // Accumulate this light!
        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    /* --- Unbind all textures --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uTexDepth);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uTexORM);

    R3D_SHADER_UNBIND_SAMPLER_CUBE(deferred.lighting, uLight.shadowCubemap);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uLight.shadowMap);

    /* --- Reset undesired state --- */

    glDisable(GL_SCISSOR_TEST);
}

void r3d_pass_deferred_compose(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    R3D_SHADER_USE(deferred.compose);

    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uTexEmission, r3d_target_get(R3D_TARGET_EMISSION));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uTexDiffuse, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uTexSpecular, r3d_target_get(R3D_TARGET_SPECULAR));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uTexDiffuse);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uTexSpecular);
}

static void r3d_pass_scene_forward_send_lights(const r3d_drawcall_t* call)
{
    int iLight = 0;

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        // Check if the geometry "touches" the light area
        // It's not the most accurate possible but hey
        if (light->type != R3D_LIGHT_DIR) {
            if (!CheckCollisionBoxes(light->aabb, call->mesh.aabb)) {
                continue;
            }
        }

        R3D_SHADER_SET_INT(scene.forward, uLights[iLight].enabled, true);
        R3D_SHADER_SET_INT(scene.forward, uLights[iLight].type, light->type);
        R3D_SHADER_SET_VEC3(scene.forward, uLights[iLight].color, light->color);
        R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].specular, light->specular);
        R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].energy, light->energy);

        if (light->type == R3D_LIGHT_DIR) {
            R3D_SHADER_SET_VEC3(scene.forward, uLights[iLight].direction, light->direction);
        }
        else if (light->type == R3D_LIGHT_SPOT) {
            R3D_SHADER_SET_VEC3(scene.forward, uLights[iLight].position, light->position);
            R3D_SHADER_SET_VEC3(scene.forward, uLights[iLight].direction, light->direction);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].range, light->range);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].attenuation, light->attenuation);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].innerCutOff, light->innerCutOff);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].outerCutOff, light->outerCutOff);
        }
        else if (light->type == R3D_LIGHT_OMNI) {
            R3D_SHADER_SET_VEC3(scene.forward, uLights[iLight].position, light->position);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].range, light->range);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].attenuation, light->attenuation);
        }

        if (light->shadow) {
            if (light->type == R3D_LIGHT_OMNI) {
                R3D_SHADER_BIND_SAMPLER_CUBE(scene.forward, uShadowMapCube[iLight], light->shadowMap.tex);
            }
            else {
                R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].shadowTexelSize, light->shadowTexelSize);
                R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uShadowMap2D[iLight], light->shadowMap.tex);
                R3D_SHADER_SET_MAT4(scene.forward, uMatLightVP[iLight], light->matVP);
            }
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].shadowSoftness, light->shadowSoftness);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].shadowDepthBias, light->shadowDepthBias);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].shadowSlopeBias, light->shadowSlopeBias);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].near, light->near);
            R3D_SHADER_SET_FLOAT(scene.forward, uLights[iLight].far, light->far);
            R3D_SHADER_SET_INT(scene.forward, uLights[iLight].shadow, true);
        }
        else {
            R3D_SHADER_SET_INT(scene.forward, uLights[iLight].shadow, false);
        }

        if (++iLight == R3D_SHADER_FORWARD_NUM_LIGHTS) {
            break;
        }
    }

    for (int i = iLight; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        R3D_SHADER_SET_INT(scene.forward, uLights[i].enabled, false);
    }
}

void r3d_pass_scene_forward(r3d_target_t sceneTarget)
{
    // NOTE: Material outputs are necessary for certain post-process passes such as SSR
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_ALBEDO, R3D_TARGET_NORMAL, R3D_TARGET_ORM, R3D_TARGET_DEPTH);
    R3D_SHADER_USE(scene.forward);

    glDepthFunc(GL_LEQUAL);

    if (R3D.env.useSky) {
        R3D_SHADER_BIND_SAMPLER_CUBE(scene.forward, uCubeIrradiance, R3D.env.sky.irradiance.id);
        R3D_SHADER_BIND_SAMPLER_CUBE(scene.forward, uCubePrefilter, R3D.env.sky.prefilter.id);
        R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uTexBrdfLut, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

        R3D_SHADER_SET_VEC4(scene.forward, uQuatSkybox, R3D.env.quatSky);
        R3D_SHADER_SET_INT(scene.forward, uHasSkybox, true);
        R3D_SHADER_SET_FLOAT(scene.forward, uSkyboxAmbientIntensity, R3D.env.skyAmbientIntensity);
        R3D_SHADER_SET_FLOAT(scene.forward, uSkyboxReflectIntensity, R3D.env.skyReflectIntensity);
    }
    else {
        R3D_SHADER_SET_VEC3(scene.forward, uAmbientLight, R3D.env.ambientLight);
        R3D_SHADER_SET_INT(scene.forward, uHasSkybox, false);
    }

    R3D_SHADER_SET_VEC3(scene.forward, uViewPosition, R3D.state.transform.viewPos);

    for (int i = 0; i < R3D.container.aDrawForwardInst.count; i++) {
        const r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawForwardInst, i);
        r3d_pass_scene_forward_send_lights(call);
        r3d_drawcall_raster_forward(call, &R3D.state.transform.viewProj);
    }

    for (int i = 0; i < R3D.container.aDrawForward.count; i++) {
        const r3d_drawcall_t* call = r3d_array_at(&R3D.container.aDrawForward, i);
        r3d_pass_scene_forward_send_lights(call);
        r3d_drawcall_raster_forward(call, &R3D.state.transform.viewProj);
    }

    if (R3D.env.useSky) {
        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.forward, uCubeIrradiance);
        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.forward, uCubePrefilter);
        R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexBrdfLut);
    }

    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.forward, uShadowMapCube[i]);
        R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uShadowMap2D[i]);
    }

    // NOTE: The storage texture of the matrices may have been bind during drawcalls
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.forward, uTexBoneMatrices);
}

void r3d_pass_scene_background(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    if (R3D.env.useSky) {
        R3D_SHADER_USE(scene.skybox);
        glDisable(GL_CULL_FACE);

        R3D_SHADER_BIND_SAMPLER_CUBE(scene.skybox, uCubeSky, R3D.env.sky.cubemap.id);
        R3D_SHADER_SET_FLOAT(scene.skybox, uSkyIntensity, R3D.env.skyBackgroundIntensity);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatView, R3D.state.transform.view);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatProj, R3D.state.transform.proj);
        R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.env.quatSky);

        R3D_PRIMITIVE_DRAW_CUBE();

        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.skybox, uCubeSky);
    }
    else {
        R3D_SHADER_USE(scene.background);
        R3D_SHADER_SET_VEC4(scene.background, uColor, R3D.env.backgroundColor);
        R3D_PRIMITIVE_DRAW_SCREEN();
    }
}

r3d_target_t r3d_pass_post_setup(r3d_target_t sceneTarget)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    return r3d_target_swap_scene(sceneTarget);
}

r3d_target_t r3d_pass_post_ssr(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);

    R3D_SHADER_USE(post.ssr);

    R3D_SHADER_BIND_SAMPLER_2D(post.ssr, uTexColor, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.ssr, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(post.ssr, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(post.ssr, uTexORM, r3d_target_get(R3D_TARGET_ORM));
    R3D_SHADER_BIND_SAMPLER_2D(post.ssr, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_INT(post.ssr, uMaxRaySteps, R3D.env.ssrMaxRaySteps);
    R3D_SHADER_SET_INT(post.ssr, uBinarySearchSteps, R3D.env.ssrBinarySearchSteps);
    R3D_SHADER_SET_FLOAT(post.ssr, uRayMarchLength, R3D.env.ssrRayMarchLength);
    R3D_SHADER_SET_FLOAT(post.ssr, uDepthThickness, R3D.env.ssrDepthThickness);
    R3D_SHADER_SET_FLOAT(post.ssr, uDepthTolerance, R3D.env.ssrDepthTolerance);
    R3D_SHADER_SET_FLOAT(post.ssr, uEdgeFadeStart, R3D.env.ssrEdgeFadeStart);
    R3D_SHADER_SET_FLOAT(post.ssr, uEdgeFadeEnd, R3D.env.ssrEdgeFadeEnd);

    R3D_SHADER_SET_MAT4(post.ssr, uMatView, R3D.state.transform.view);
    R3D_SHADER_SET_MAT4(post.ssr, uMatInvProj, R3D.state.transform.invProj);
    R3D_SHADER_SET_MAT4(post.ssr, uMatInvView, R3D.state.transform.invView);
    R3D_SHADER_SET_MAT4(post.ssr, uMatViewProj, R3D.state.transform.viewProj);
    R3D_SHADER_SET_VEC3(post.ssr, uViewPosition, R3D.state.transform.viewPos);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.ssr, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.ssr, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.ssr, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.ssr, uTexORM);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.ssr, uTexDepth);

    return sceneTarget;
}

r3d_target_t r3d_pass_post_fog(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fog);

    R3D_SHADER_BIND_SAMPLER_2D(post.fog, uTexColor, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.fog, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_FLOAT(post.fog, uNear, (float)rlGetCullDistanceNear());
    R3D_SHADER_SET_FLOAT(post.fog, uFar, (float)rlGetCullDistanceFar());
    R3D_SHADER_SET_INT(post.fog, uFogMode, R3D.env.fogMode);
    R3D_SHADER_SET_VEC3(post.fog, uFogColor, R3D.env.fogColor);
    R3D_SHADER_SET_FLOAT(post.fog, uFogStart, R3D.env.fogStart);
    R3D_SHADER_SET_FLOAT(post.fog, uFogEnd, R3D.env.fogEnd);
    R3D_SHADER_SET_FLOAT(post.fog, uFogDensity, R3D.env.fogDensity);
    R3D_SHADER_SET_FLOAT(post.fog, uSkyAffect, R3D.env.fogSkyAffect);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.fog, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.fog, uTexDepth);

    return sceneTarget;
}

r3d_target_t r3d_pass_post_dof(r3d_target_t sceneTarget)	
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.dof);

    R3D_SHADER_BIND_SAMPLER_2D(post.dof, uTexColor, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.dof, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_VEC2(post.dof, uTexelSize, (Vector2) {R3D_TARGET_TEXEL_SIZE});
    R3D_SHADER_SET_FLOAT(post.dof, uNear, (float)rlGetCullDistanceNear());
    R3D_SHADER_SET_FLOAT(post.dof, uFar, (float)rlGetCullDistanceFar());
    R3D_SHADER_SET_FLOAT(post.dof, uFocusPoint, R3D.env.dofFocusPoint);
    R3D_SHADER_SET_FLOAT(post.dof, uFocusScale, R3D.env.dofFocusScale);
    R3D_SHADER_SET_FLOAT(post.dof, uMaxBlurSize, R3D.env.dofMaxBlurSize);
    R3D_SHADER_SET_INT(post.dof, uDebugMode, R3D.env.dofDebugMode);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.dof, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.dof, uTexDepth);

    return sceneTarget;
}

r3d_target_t r3d_pass_post_bloom(r3d_target_t sceneTarget)
{
    r3d_target_t sceneSource = r3d_target_swap_scene(sceneTarget);
    GLuint sceneSourceID = r3d_target_get(sceneSource);
    int mipCount = r3d_mod_target_get_mip_count();

    float txSrcW = 0, txSrcH = 0;
    int srcW = 0, srcH = 0;
    int dstW = 0, dstH = 0;

    R3D_TARGET_BIND(R3D_TARGET_BLOOM);

    /* --- Bloom: Karis average before downsampling --- */

    R3D_SHADER_USE(prepare.bloomDown);

    r3d_mod_target_get_texel_size(&txSrcW, &txSrcH, 0);
    r3d_mod_target_get_resolution(&srcW, &srcH, 0);
    r3d_target_set_mip_level(0, 0);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomDown, uTexture, sceneSourceID);

    R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
    R3D_SHADER_SET_VEC4(prepare.bloomDown, uPrefilter, R3D.env.bloomPrefilter);
    R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, 0);

    R3D_PRIMITIVE_DRAW_SCREEN();

    /* --- Bloom: Downsampling --- */

    // It's okay to sample the target here
    // Given that we'll be sampling a different level from where we're writing
    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomDown, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = 1; dstLevel < mipCount; dstLevel++)
    {
        r3d_mod_target_get_texel_size(&txSrcW, &txSrcH, dstLevel - 1);
        r3d_mod_target_get_resolution(&srcW, &srcH, dstLevel - 1);
        r3d_mod_target_get_resolution(&dstW, &dstH, dstLevel);

        r3d_target_set_mip_level(0, dstLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
        R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, dstLevel);

        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.bloomDown, uTexture);

    /* --- Bloom: Upsampling --- */

    R3D_SHADER_USE(prepare.bloomUp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomUp, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = mipCount - 2; dstLevel >= 0; dstLevel--)
    {
        r3d_mod_target_get_texel_size(&txSrcW, &txSrcH, dstLevel + 1);
        r3d_mod_target_get_resolution(&srcW, &srcH, dstLevel + 1);
        r3d_mod_target_get_resolution(&dstW, &dstH, dstLevel);

        r3d_target_set_mip_level(0, dstLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_FLOAT(prepare.bloomUp, uSrcLevel, dstLevel + 1);
        R3D_SHADER_SET_VEC2(prepare.bloomUp, uFilterRadius, (Vector2) {
            (float)R3D.env.bloomFilterRadius * txSrcW,
            (float)R3D.env.bloomFilterRadius * txSrcH
        });

        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.bloomUp, uTexture);

    glDisable(GL_BLEND);

    /* --- Apply bloom to the scene --- */

    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.bloom);

    R3D_SHADER_BIND_SAMPLER_2D(post.bloom, uTexColor, sceneSourceID);
    R3D_SHADER_BIND_SAMPLER_2D(post.bloom, uTexBloomBlur, r3d_target_get(R3D_TARGET_BLOOM));

    R3D_SHADER_SET_INT(post.bloom, uBloomMode, R3D.env.bloomMode);
    R3D_SHADER_SET_FLOAT(post.bloom, uBloomIntensity, R3D.env.bloomIntensity);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.bloom, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.bloom, uTexBloomBlur);

    return sceneTarget;
}

r3d_target_t r3d_pass_post_output(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.output);

    R3D_SHADER_BIND_SAMPLER_2D(post.output, uTexColor, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_FLOAT(post.output, uTonemapExposure, R3D.env.tonemapExposure);
    R3D_SHADER_SET_FLOAT(post.output, uTonemapWhite, R3D.env.tonemapWhite);
    R3D_SHADER_SET_INT(post.output, uTonemapMode, R3D.env.tonemapMode);
    R3D_SHADER_SET_FLOAT(post.output, uBrightness, R3D.env.brightness);
    R3D_SHADER_SET_FLOAT(post.output, uContrast, R3D.env.contrast);
    R3D_SHADER_SET_FLOAT(post.output, uSaturation, R3D.env.saturation);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.output, uTexColor);

    return sceneTarget;
}

r3d_target_t r3d_pass_post_fxaa(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fxaa);

    R3D_SHADER_BIND_SAMPLER_2D(post.fxaa, uTexture, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_VEC2(post.fxaa, uTexelSize, (Vector2) {R3D_TARGET_TEXEL_SIZE});
    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.fxaa, uTexture);

    return sceneTarget;
}

void r3d_reset_raylib_state(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glViewport(0, 0, GetRenderWidth(), GetRenderHeight());

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Here we re-define the blend mode via rlgl to ensure its internal state
    // matches what we've just set manually with OpenGL.

    // It's not enough to change the blend mode only through rlgl, because if we
    // previously used a different blend mode (not "alpha") but rlgl still thinks it's "alpha",
    // then rlgl won't correctly apply the intended blend mode.

    // We do this at the end because calling rlSetBlendMode can trigger a draw call for
    // any content accumulated by rlgl, and we want that to be rendered into the main
    // framebuffer, not into one of R3D's internal framebuffers that will be discarded afterward.

    // TODO: Ideally, we would retrieve rlgl’s current blend mode state and restore it exactly.

    rlSetBlendMode(RL_BLEND_ALPHA);
}
