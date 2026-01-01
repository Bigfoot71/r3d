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

#include "./details/r3d_frustum.h"
#include "./details/r3d_macros.h"
#include "./details/r3d_math.h"

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_cache.h"
#include "./modules/r3d_draw.h"

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

static void raster_depth(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP);
static void raster_depth_cube(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP);
static void raster_geometry(const r3d_draw_call_t* call);
static void raster_decal(const r3d_draw_call_t* call);
static void raster_forward(const r3d_draw_call_t* call);

static void pass_scene_shadow(void);
static void pass_scene_geometry(void);
static void pass_scene_prepass(void);
static void pass_scene_decals(void);

static r3d_target_t pass_prepare_ssao(void);
static r3d_target_t pass_prepare_ssil(void);
static r3d_target_t pass_prepare_ssr(void);

static void pass_deferred_ambient(r3d_target_t ssaoSource, r3d_target_t ssilSource, r3d_target_t ssrSource);
static void pass_deferred_lights(r3d_target_t ssaoSource);
static void pass_deferred_compose(r3d_target_t sceneTarget);

static void pass_scene_forward(r3d_target_t sceneTarget);
static void pass_scene_background(r3d_target_t sceneTarget);

static r3d_target_t pass_post_setup(r3d_target_t sceneTarget);
static r3d_target_t pass_post_fog(r3d_target_t sceneTarget);
static r3d_target_t pass_post_dof(r3d_target_t sceneTarget);
static r3d_target_t pass_post_bloom(r3d_target_t sceneTarget);
static r3d_target_t pass_post_output(r3d_target_t sceneTarget);
static r3d_target_t pass_post_fxaa(r3d_target_t sceneTarget);

static void reset_raylib_state(void);

// ========================================
// PUBLIC API
// ========================================

void R3D_Begin(Camera3D camera)
{
    R3D_BeginEx(camera, NULL);
}

void R3D_BeginEx(Camera3D camera, const RenderTexture* target)
{
    rlDrawRenderBatchActive();

    r3d_target_set_blit_screen(target);

    r3d_target_set_blit_mode(
        R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_ASPECT_KEEP),
        R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_BLIT_LINEAR)
    );

    r3d_cache_update_view_state(
        camera,
        r3d_target_get_render_aspect(),
        rlGetCullDistanceNear(),
        rlGetCullDistanceFar()
    );

    r3d_draw_clear();
}

void R3D_End(void)
{
    /* --- Update and collect all visible lights then render shadow maps --- */

    r3d_light_update_and_cull(&R3D_CACHE_GET(viewState.frustum), R3D_CACHE_GET(viewState.viewPosition));

    pass_scene_shadow();

    /* --- Cull groups and sort all draw calls before rendering --- */

    if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
        r3d_draw_compute_visible_groups(&R3D_CACHE_GET(viewState.frustum));
    }

    if (R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_OPAQUE_SORTING)) {
        r3d_draw_sort_list(R3D_DRAW_DEFERRED, R3D_CACHE_GET(viewState.viewPosition), R3D_DRAW_SORT_FRONT_TO_BACK);
    }

    if (R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_TRANSPARENT_SORTING)) {
        r3d_draw_sort_list(R3D_DRAW_PREPASS, R3D_CACHE_GET(viewState.viewPosition), R3D_DRAW_SORT_BACK_TO_FRONT);
        r3d_draw_sort_list(R3D_DRAW_FORWARD, R3D_CACHE_GET(viewState.viewPosition), R3D_DRAW_SORT_BACK_TO_FRONT);
    }

    /* --- Upload and bind uniform buffers --- */

    r3d_cache_bind_view_state(R3D_SHADER_UBO_VIEW_SLOT);

    /* --- Opaque and decal rendering with deferred lighting and composition --- */

    r3d_target_t sceneTarget = R3D_TARGET_SCENE_0;

    if (r3d_draw_has_deferred() || r3d_draw_has_prepass()) {
        R3D_TARGET_CLEAR(R3D_TARGET_ALL_DEFERRED);

        pass_scene_geometry();

        if (r3d_draw_has_prepass()) {
            pass_scene_prepass();
        }

        if (r3d_draw_has_decal()) {
            pass_scene_decals();
        }

        r3d_target_t ssaoSource = R3D_TARGET_INVALID;
        if (R3D_CACHE_GET(environment.ssao.enabled)) {
            ssaoSource = pass_prepare_ssao();
        }

        if (r3d_light_has_visible()) {
            pass_deferred_lights(ssaoSource);
        }

        r3d_target_t ssilSource = R3D_TARGET_INVALID;
        if (R3D_CACHE_GET(environment.ssil.enabled)) {
            ssilSource = pass_prepare_ssil();
        }

        r3d_target_t ssrSource = R3D_TARGET_INVALID;
        if (R3D_CACHE_GET(environment.ssr.enabled)) {
            ssrSource = pass_prepare_ssr();
        }

        pass_deferred_ambient(ssaoSource, ssilSource, ssrSource);
        pass_deferred_compose(sceneTarget);
    }
    else {
        R3D_TARGET_CLEAR(R3D_TARGET_DEPTH);
    }

    /* --- Then background and transparent rendering --- */

    pass_scene_background(sceneTarget);

    if (r3d_draw_has_forward() || r3d_draw_has_prepass()) {
        pass_scene_forward(sceneTarget);
    }

    /* --- Applying effects over the scene and final blit --- */

    sceneTarget = pass_post_setup(sceneTarget);

    if (R3D_CACHE_GET(environment.fog.mode) != R3D_FOG_DISABLED) {
        sceneTarget = pass_post_fog(sceneTarget);
    }

    if (R3D_CACHE_GET(environment.dof.mode) != R3D_DOF_DISABLED) {
        sceneTarget = pass_post_dof(sceneTarget);
    }

    if (R3D_CACHE_GET(environment.bloom.mode) != R3D_BLOOM_DISABLED) {
        sceneTarget = pass_post_bloom(sceneTarget);
    }

    sceneTarget = pass_post_output(sceneTarget);

    if (R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_FXAA)) {
        sceneTarget = pass_post_fxaa(sceneTarget);
    }

    r3d_target_blit(r3d_target_swap_scene(sceneTarget));

    /* --- Reset states changed by R3D --- */

    reset_raylib_state();
}

void R3D_DrawMesh(const R3D_Mesh* mesh, const R3D_Material* material, Matrix transform)
{
    if (!R3D_CACHE_FLAGS_HAS(layers, mesh->layerMask)) {
        return;
    }

    r3d_draw_group_t drawGroup = {0};
    drawGroup.transform = transform;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};
    drawCall.material = material ? *material : R3D_GetDefaultMaterial();
    drawCall.mesh = *mesh;

    r3d_draw_call_push(&drawCall, false);
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
    if (!R3D_CACHE_FLAGS_HAS(layers, mesh->layerMask)) {
        return;
    }

    if (instanceCount == 0 || instanceTransforms == NULL) {
        return;
    }

    r3d_draw_group_t drawGroup = {0};

    drawGroup.transform = globalTransform;
    drawGroup.instanced.allAabb = globalAabb ? *globalAabb : (BoundingBox) {0};
    drawGroup.instanced.transforms = instanceTransforms;
    drawGroup.instanced.transStride = transformsStride;
    drawGroup.instanced.colStride = colorsStride;
    drawGroup.instanced.colors = instanceColors;
    drawGroup.instanced.count = instanceCount;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};

    drawCall.material = material ? *material : R3D_GetDefaultMaterial();
    drawCall.mesh = *mesh;

    r3d_draw_call_push(&drawCall, false);
}

void R3D_DrawModel(const R3D_Model* model, Vector3 position, float scale)
{
    Vector3 vScale = {scale, scale, scale};
    Vector3 rotationAxis = {0.0f, 1.0f, 0.0f};
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
    r3d_draw_group_t drawGroup = {0};

    drawGroup.aabb = model->aabb;
    drawGroup.transform = transform;
    drawGroup.skeleton = model->skeleton;
    drawGroup.player = model->player;

    r3d_draw_group_push(&drawGroup);

    for (int i = 0; i < model->meshCount; i++)
    {
        const R3D_Mesh* mesh = &model->meshes[i];

        if (!R3D_CACHE_FLAGS_HAS(layers, mesh->layerMask)) {
            return;
        }

        r3d_draw_call_t drawCall = {0};

        drawCall.material = model->materials[model->meshMaterials[i]];
        drawCall.mesh = *mesh;

        r3d_draw_call_push(&drawCall, false);
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

    r3d_draw_group_t drawGroup = {0};

    drawGroup.aabb = model->aabb;
    drawGroup.transform = globalTransform;
    drawGroup.skeleton = model->skeleton;
    drawGroup.player = model->player;

    drawGroup.instanced.allAabb = globalAabb ? *globalAabb : (BoundingBox) {0};
    drawGroup.instanced.transforms = instanceTransforms;
    drawGroup.instanced.transStride = transformsStride;
    drawGroup.instanced.colStride = colorsStride;
    drawGroup.instanced.colors = instanceColors;
    drawGroup.instanced.count = instanceCount;

    r3d_draw_group_push(&drawGroup);

    for (int i = 0; i < model->meshCount; i++)
    {
        const R3D_Mesh* mesh = &model->meshes[i];

        if (!R3D_CACHE_FLAGS_HAS(layers, mesh->layerMask)) {
            return;
        }

        r3d_draw_call_t drawCall = {0};

        drawCall.material = model->materials[model->meshMaterials[i]];
        drawCall.mesh = *mesh;

        r3d_draw_call_push(&drawCall, false);
    }
}

void R3D_DrawDecal(const R3D_Decal* decal, Matrix transform)
{
    r3d_draw_group_t drawGroup = {0};
    drawGroup.transform = transform;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};
    drawCall.material = decal->material;
    drawCall.mesh.shadowCastMode = R3D_SHADOW_CAST_DISABLED;
    drawCall.mesh.aabb.min = (Vector3) {-0.5f, -0.5f, -0.5f};
    drawCall.mesh.aabb.max = (Vector3) {+0.5f, +0.5f, +0.5f};

    r3d_draw_call_push(&drawCall, true);
}

void R3D_DrawDecalInstanced(const R3D_Decal* decal, const Matrix* instanceTransforms, int instanceCount)
{
    r3d_draw_group_t drawGroup = {0};

    drawGroup.transform = R3D_MATRIX_IDENTITY;
    // TODO: Move aabb evaluation to potential Pro version of this function
    drawGroup.instanced.allAabb = (BoundingBox) {0};
    drawGroup.instanced.transforms = instanceTransforms;
    drawGroup.instanced.transStride = 0;
    drawGroup.instanced.colStride = 0;
    drawGroup.instanced.colors = NULL;
    drawGroup.instanced.count = instanceCount;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};

    drawCall.material = decal->material;
    drawCall.mesh.shadowCastMode = R3D_SHADOW_CAST_DISABLED;
    drawCall.mesh.aabb.min = (Vector3) {-0.5f, -0.5f, -0.5f};
    drawCall.mesh.aabb.max = (Vector3) {+0.5f, +0.5f, +0.5f};

    r3d_draw_call_push(&drawCall, true);
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

void raster_depth(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depth, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.depth, uMatVP, *matVP);

    /* --- Send skinning related data --- */

    if (group->player != NULL || R3D_IsSkeletonValid(&group->skeleton)) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.depth, uTexBoneMatrices, group->player ? group->player->texGlobalPose : group->skeleton.texBindPose);
        R3D_SHADER_SET_INT(scene.depth, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depth, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depth, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depth, uMatInvView, R3D_CACHE_GET(viewState.invView));
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordScale, call->material.uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.depth, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_SET_FLOAT(scene.depth, uAlpha, ((float)call->material.albedo.color.a / 255));

    if (call->material.transparencyMode == R3D_TRANSPARENCY_PREPASS) {
        R3D_SHADER_SET_FLOAT(scene.depth, uAlphaCutoff, shadow ? 0.1f : 0.99f);
    }
    else {
        R3D_SHADER_SET_FLOAT(scene.depth, uAlphaCutoff, call->material.alphaCutoff);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_draw_apply_shadow_cast_mode(call->mesh.shadowCastMode, call->material.cullMode);
    }
    else {
        r3d_draw_apply_cull_mode(call->material.cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.depth, uInstancing, true);
        r3d_draw_instanced(call, 10, -1);
    }
    else {
        R3D_SHADER_SET_INT(scene.depth, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind samplers --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.depth, uTexAlbedo);
}

void raster_depth_cube(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depthCube, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.depthCube, uMatVP, *matVP);

    /* --- Send skinning related data --- */

    if (group->player != NULL || R3D_IsSkeletonValid(&group->skeleton)) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.depthCube, uTexBoneMatrices, group->player ? group->player->texGlobalPose : group->skeleton.texBindPose);
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depthCube, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depthCube, uMatInvView, R3D_CACHE_GET(viewState.invView));
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordScale, call->material.uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.depthCube, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_SET_FLOAT(scene.depthCube, uAlpha, ((float)call->material.albedo.color.a / 255));

    if (call->material.transparencyMode == R3D_TRANSPARENCY_PREPASS) {
        R3D_SHADER_SET_FLOAT(scene.depthCube, uAlphaCutoff, shadow ? 0.1f : 0.99f);
    }
    else {
        R3D_SHADER_SET_FLOAT(scene.depthCube, uAlphaCutoff, call->material.alphaCutoff);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_draw_apply_shadow_cast_mode(call->mesh.shadowCastMode, call->material.cullMode);
    }
    else {
        r3d_draw_apply_cull_mode(call->material.cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.depthCube, uInstancing, true);
        r3d_draw_instanced(call, 10, -1);
    }
    else {
        R3D_SHADER_SET_INT(scene.depthCube, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind vertex buffers --- */

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    /* --- Unbind samplers --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.depthCube, uTexAlbedo);
}

void raster_geometry(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.geometry, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.geometry, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->player != NULL || R3D_IsSkeletonValid(&group->skeleton)) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.geometry, uTexBoneMatrices, group->player ? group->player->texGlobalPose : group->skeleton.texBindPose);
        R3D_SHADER_SET_INT(scene.geometry, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.geometry, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.geometry, uBillboard, call->material.billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.geometry, uEmissionEnergy, call->material.emission.energy);
    R3D_SHADER_SET_FLOAT(scene.geometry, uNormalScale, call->material.normal.scale);
    R3D_SHADER_SET_FLOAT(scene.geometry, uOcclusion, call->material.orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.geometry, uRoughness, call->material.orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.geometry, uMetalness, call->material.orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT(scene.geometry, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.geometry, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.geometry, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.geometry, uAlbedoColor, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.geometry, uEmissionColor, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uTexNormal, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uTexEmission, R3D_TEXTURE_SELECT(call->material.emission.texture.id, BLACK));
    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uTexORM, R3D_TEXTURE_SELECT(call->material.orm.texture.id, BLACK));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.geometry, uInstancing, true);
        r3d_draw_instanced(call, 10, 14);
    }
    else {
        R3D_SHADER_SET_INT(scene.geometry, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexORM);
}

void raster_decal(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Set additional matrix uniforms --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.decal, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.decal, uMatNormal, matNormal);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.decal, uEmissionEnergy, call->material.emission.energy);
    R3D_SHADER_SET_FLOAT(scene.decal, uNormalScale, call->material.normal.scale);
    R3D_SHADER_SET_FLOAT(scene.decal, uOcclusion, call->material.orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.decal, uRoughness, call->material.orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.decal, uMetalness, call->material.orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT(scene.decal, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.decal, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.decal, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.decal, uAlbedoColor, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.decal, uEmissionColor, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uTexNormal, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uTexEmission, R3D_TEXTURE_SELECT(call->material.emission.texture.id, BLACK));
    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uTexORM, R3D_TEXTURE_SELECT(call->material.orm.texture.id, BLACK));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(call->material.blendMode, call->material.transparencyMode);

    /* --- Disable face culling to avoid issues when camera is inside the decal bounding mesh --- */
    // TODO: Implement check for if camera is inside the mesh and apply the appropriate face culling / depth testing

    glDisable(GL_CULL_FACE);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.decal, uInstancing, true);
        r3d_primitive_draw_instanced(
            R3D_PRIMITIVE_CUBE,
            group->instanced.transforms,
            group->instanced.transStride,
            group->instanced.colors,
            group->instanced.colStride,
            group->instanced.count,
            10, 14
        );
    }
    else {
        R3D_SHADER_SET_INT(scene.decal, uInstancing, false);
        r3d_primitive_draw(R3D_PRIMITIVE_CUBE);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexORM);
}

void raster_forward(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.forward, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.forward, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->player != NULL || R3D_IsSkeletonValid(&group->skeleton)) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.forward, uTexBoneMatrices, group->player ? group->player->texGlobalPose : group->skeleton.texBindPose);
        R3D_SHADER_SET_INT(scene.forward, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.forward, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.forward, uBillboard, call->material.billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.forward, uEmissionEnergy, call->material.emission.energy);
    R3D_SHADER_SET_FLOAT(scene.forward, uNormalScale, call->material.normal.scale);
    R3D_SHADER_SET_FLOAT(scene.forward, uOcclusion, call->material.orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.forward, uRoughness, call->material.orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.forward, uMetalness, call->material.orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT(scene.forward, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.forward, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.forward, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.forward, uAlbedoColor, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.forward, uEmissionColor, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uTexNormal, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uTexEmission, R3D_TEXTURE_SELECT(call->material.emission.texture.id, BLACK));
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uTexORM, R3D_TEXTURE_SELECT(call->material.orm.texture.id, BLACK));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(call->material.blendMode, call->material.transparencyMode);
    r3d_draw_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.forward, uInstancing, true);
        r3d_draw_instanced(call, 10, 14);
    }
    else {
        R3D_SHADER_SET_INT(scene.forward, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexORM);
}

void pass_scene_shadow(void)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        if (!r3d_light_shadow_should_be_upadted(light, true)) {
            continue;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, light->shadowMap.fbo);
        glViewport(0, 0, light->shadowMap.resolution, light->shadowMap.resolution);

        if (light->type == R3D_LIGHT_OMNI) {
            R3D_SHADER_USE(scene.depthCube);
            R3D_SHADER_SET_FLOAT(scene.depthCube, uFar, light->far);
            R3D_SHADER_SET_VEC3(scene.depthCube, uViewPosition, light->position);

            for (int iFace = 0; iFace < 6; iFace++) {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + iFace, light->shadowMap.tex, 0);
                glClear(GL_DEPTH_BUFFER_BIT);

                const r3d_frustum_t* frustum = NULL;
                if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
                    frustum = &light->frustum[iFace];
                    r3d_draw_compute_visible_groups(frustum);
                }

                #define COND (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
                R3D_DRAW_FOR_EACH(call, COND, frustum, R3D_DRAW_DEFERRED_INST, R3D_DRAW_DEFERRED, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
                    raster_depth_cube(call, true, &light->matVP[iFace]);
                }
                #undef COND
            }

            // The bone matrices texture may have been bind during drawcalls, so UNBIND!
            R3D_SHADER_UNBIND_SAMPLER_1D(scene.depthCube, uTexBoneMatrices);
        }
        else {
            glClear(GL_DEPTH_BUFFER_BIT);
            R3D_SHADER_USE(scene.depth);

            const r3d_frustum_t* frustum = NULL;
            if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
                frustum = &light->frustum[0];
                r3d_draw_compute_visible_groups(frustum);
            }

            #define COND (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
            R3D_DRAW_FOR_EACH(call, COND, frustum, R3D_DRAW_DEFERRED_INST, R3D_DRAW_DEFERRED, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
                raster_depth(call, true, &light->matVP[0]);
            }
            #undef COND

            // The bone matrices texture may have been bind during drawcalls, so UNBIND!
            R3D_SHADER_UNBIND_SAMPLER_1D(scene.depth, uTexBoneMatrices);
        }
    }
}

void pass_scene_geometry(void)
{
    R3D_TARGET_BIND(R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.geometry);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    const r3d_frustum_t* frustum = NULL;
    if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
        frustum = &R3D_CACHE_GET(viewState.frustum);
    }

    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_DEFERRED_INST, R3D_DRAW_DEFERRED) {
        raster_geometry(call);
    }

    // The bone matrices texture may have been bind during drawcalls, so UNBIND!
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.geometry, uTexBoneMatrices);
}

void pass_scene_prepass(void)
{
    /* --- First render only depth --- */

    R3D_TARGET_BIND(R3D_TARGET_DEPTH);
    R3D_SHADER_USE(scene.depth);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    const r3d_frustum_t* frustum = NULL;
    if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
        frustum = &R3D_CACHE_GET(viewState.frustum);
    }

    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
        raster_depth(call, false, &R3D_CACHE_GET(viewState.viewProj));
    }

    /* --- Render opaque only with GL_EQUAL --- */

    // NOTE: The transparent part will be rendered in forward
    R3D_TARGET_BIND(R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.geometry);

    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);

    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
        raster_geometry(call);
    }

    // NOTE: The storage texture of the matrices may have been bind during drawcalls
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.forward, uTexBoneMatrices);
}

void pass_scene_decals(void)
{
    R3D_TARGET_BIND(R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.decal);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    const r3d_frustum_t* frustum = NULL;
    if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
        frustum = &R3D_CACHE_GET(viewState.frustum);
    }

    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_DECAL_INST, R3D_DRAW_DECAL) {
        raster_decal(call);
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexDepth);
}

r3d_target_t pass_prepare_ssao(void)
{
    glDisable(GL_DEPTH_TEST);   //< Can't depth test to touch only the geometry, since the target is half res...
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    /* --- Calculate SSAO --- */

    r3d_target_t ssaoTarget = R3D_TARGET_SSAO_0;
    R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);

    R3D_SHADER_USE(prepare.ssao);

    R3D_SHADER_SET_INT(prepare.ssao, uSampleCount,  R3D_CACHE_GET(environment.ssao.sampleCount));
    R3D_SHADER_SET_FLOAT(prepare.ssao, uRadius,  R3D_CACHE_GET(environment.ssao.radius));
    R3D_SHADER_SET_FLOAT(prepare.ssao, uBias, R3D_CACHE_GET(environment.ssao.bias));
    R3D_SHADER_SET_FLOAT(prepare.ssao, uIntensity, R3D_CACHE_GET(environment.ssao.intensity));
    R3D_SHADER_SET_FLOAT(prepare.ssao, uPower, R3D_CACHE_GET(environment.ssao.power));

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uTexDepth);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uTexNormal);

    /* --- Denoise SSAO --- */

    R3D_SHADER_USE(prepare.atrousWavelet);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.atrousWavelet, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.atrousWavelet, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    for (int i = 0; i < 4; i++) {
        R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);
        R3D_SHADER_BIND_SAMPLER_2D(prepare.atrousWavelet, uTexSource, r3d_target_get(ssaoTarget));
        R3D_SHADER_SET_INT(prepare.atrousWavelet, uStepSize, 1 << i);
        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.atrousWavelet, uTexSource);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.atrousWavelet, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.atrousWavelet, uTexDepth);

    return r3d_target_swap_ssao(ssaoTarget);
}

r3d_target_t pass_prepare_ssil(void)
{
    glDisable(GL_DEPTH_TEST);   //< Can't depth test to touch only the geometry, since the target is half res...
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    // REVIEW: If we ever support multi viewports, managing the convergence history
    //         via static variables will need to be revisited and stored per viewport.

    static r3d_target_t SSIL_HISTORY = R3D_TARGET_SSIL_0;
    static r3d_target_t SSIL_CURRENT = R3D_TARGET_SSIL_1;

    /* --- Check if we need history --- */

    bool needConvergence = (R3D_CACHE_GET(environment.ssil.convergence) >= 0.1f);
    bool needBounce = (R3D_CACHE_GET(environment.ssil.bounce) >= 0.01f);
    bool needHistory = (needConvergence || needBounce);

    if (needHistory && r3d_target_get_or_null(SSIL_HISTORY) == 0) {
        R3D_TARGET_CLEAR(SSIL_HISTORY);
    }

    /* --- Calculate SSIL --- */

    R3D_TARGET_BIND(SSIL_CURRENT);
    R3D_SHADER_USE(prepare.ssil);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uTexLight, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uTexPrevSSIL, R3D_TEXTURE_SELECT(r3d_target_get_or_null(SSIL_HISTORY), BLACK));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleCount, (float)R3D_CACHE_GET(environment.ssil.sampleCount));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleRadius, R3D_CACHE_GET(environment.ssil.sampleRadius));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSliceCount, (float)R3D_CACHE_GET(environment.ssil.sliceCount));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uHitThickness, R3D_CACHE_GET(environment.ssil.hitThickness));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uConvergence, R3D_CACHE_GET(environment.ssil.convergence));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uAoPower, R3D_CACHE_GET(environment.ssil.aoPower));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uBounce, R3D_CACHE_GET(environment.ssil.bounce));
    R3D_SHADER_SET_FLOAT(prepare.ssil, uEnergy, R3D_CACHE_GET(environment.ssil.energy));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uTexLight);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uTexPrevSSIL);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uTexDepth);

    /* --- Blur SSIL (Dual Filtering) --- */

    r3d_target_t SSIL_MIP = R3D_TARGET_SSIL_MIP;

    int mipCount = r3d_target_get_mip_count(SSIL_MIP);
    int mipMax = MIN(3, mipCount);

    R3D_TARGET_BIND(SSIL_MIP);

    // Downsample
    R3D_SHADER_USE(prepare.blurDown);
    R3D_SHADER_BIND_SAMPLER_2D(prepare.blurDown, uTexSource, r3d_target_get(SSIL_CURRENT));
    for (int mipLevel = 1; mipLevel < mipMax; mipLevel++)
    {
        int dstW, dstH;
        r3d_target_get_resolution(&dstW, &dstH, SSIL_MIP, mipLevel);
        r3d_target_set_mip_level(0, mipLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_INT(prepare.blurDown, uMipSource, mipLevel - 1);
        R3D_PRIMITIVE_DRAW_SCREEN();

        if (mipLevel == 1) {
            R3D_SHADER_BIND_SAMPLER_2D(prepare.blurDown, uTexSource, r3d_target_get(SSIL_MIP));
        }
    }
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.blurDown, uTexSource);

    // Upsample
    R3D_SHADER_USE(prepare.blurUp);
    R3D_SHADER_BIND_SAMPLER_2D(prepare.blurUp, uTexSource, r3d_target_get(SSIL_MIP));
    for (int mipLevel = mipMax - 2; mipLevel >= 0; mipLevel--)
    {
        int dstW, dstH;
        r3d_target_get_resolution(&dstW, &dstH, SSIL_MIP, mipLevel);
        r3d_target_set_mip_level(0, mipLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_INT(prepare.blurUp, uMipSource, mipLevel + 1);
        R3D_PRIMITIVE_DRAW_SCREEN();
    }
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.blurUp, uTexSource);

    /* --- Swap history --- */

    if (needHistory) {
        r3d_target_t tmp = SSIL_HISTORY;
        SSIL_HISTORY = SSIL_CURRENT;
        SSIL_CURRENT = tmp;
    }

    return R3D_TARGET_SSIL_MIP;
}

r3d_target_t pass_prepare_ssr(void)
{
    glDisable(GL_DEPTH_TEST);   //< Can't depth test to touch only the geometry, since the target is half res...
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    R3D_TARGET_BIND(R3D_TARGET_SSR);
    R3D_SHADER_USE(prepare.ssr);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uTexColor, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uTexORM, r3d_target_get(R3D_TARGET_ORM));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_INT(prepare.ssr, uMaxRaySteps, R3D_CACHE_GET(environment.ssr.maxRaySteps));
    R3D_SHADER_SET_INT(prepare.ssr, uBinarySearchSteps, R3D_CACHE_GET(environment.ssr.binarySearchSteps));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uRayMarchLength, R3D_CACHE_GET(environment.ssr.rayMarchLength));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uDepthThickness, R3D_CACHE_GET(environment.ssr.depthThickness));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uDepthTolerance, R3D_CACHE_GET(environment.ssr.depthTolerance));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFadeStart, R3D_CACHE_GET(environment.ssr.edgeFadeStart));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFadeEnd, R3D_CACHE_GET(environment.ssr.edgeFadeEnd));

    R3D_SHADER_SET_COL3(prepare.ssr, uAmbientColor, R3D_CACHE_GET(environment.ambient.color));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uAmbientEnergy, R3D_CACHE_GET(environment.ambient.energy));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uTexORM);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uTexDepth);

    r3d_target_gen_mipmap(R3D_TARGET_SSR);

    return R3D_TARGET_SSR;
}

void pass_deferred_ambient(r3d_target_t ssaoSource, r3d_target_t ssilSource, r3d_target_t ssrSource)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);

    // Set additive blending to accumulate light contributions
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    /* --- Calculate skybox IBL contribution --- */

    if (R3D_CACHE_GET(environment.background.sky.cubemap.id) != 0)
    {
        R3D_TARGET_BIND(R3D_TARGET_LIGHTING);
        R3D_SHADER_USE(deferred.ambientIbl);

        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexNormal, r3d_target_get(R3D_TARGET_NORMAL));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexSSAO, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexSSIL, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssilSource), BLACK));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexSSR, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssrSource), BLANK));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexORM, r3d_target_get(R3D_TARGET_ORM));
        R3D_SHADER_BIND_SAMPLER_CUBE(deferred.ambientIbl, uCubeIrradiance, R3D_CACHE_GET(environment.background.sky.irradiance.id));
        R3D_SHADER_BIND_SAMPLER_CUBE(deferred.ambientIbl, uCubePrefilter, R3D_CACHE_GET(environment.background.sky.prefilter.id));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambientIbl, uTexBrdfLut, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

        R3D_SHADER_SET_VEC4(deferred.ambientIbl, uQuatSkybox, R3D_CACHE_GET(environment.background.rotation));
        R3D_SHADER_SET_FLOAT(deferred.ambientIbl, uAmbientEnergy, R3D_CACHE_GET(environment.ambient.energy));
        R3D_SHADER_SET_FLOAT(deferred.ambientIbl, uReflectEnergy, R3D_CACHE_GET(environment.ambient.reflect));
        R3D_SHADER_SET_FLOAT(deferred.ambientIbl, uMipCountSSR, (float)r3d_target_get_mip_count(R3D_TARGET_SSR));

        R3D_PRIMITIVE_DRAW_SCREEN();

        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexAlbedo);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexNormal);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexDepth);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexSSAO);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexSSIL);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexSSR);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexORM);
        R3D_SHADER_UNBIND_SAMPLER_CUBE(deferred.ambientIbl, uCubeIrradiance);
        R3D_SHADER_UNBIND_SAMPLER_CUBE(deferred.ambientIbl, uCubePrefilter);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambientIbl, uTexBrdfLut);
    }

    /* --- If no skybox, calculate simple ambient contribution --- */

    else
    {
        R3D_TARGET_BIND(R3D_TARGET_LIGHTING);
        R3D_SHADER_USE(deferred.ambient);

        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexAlbedo, r3d_target_get(R3D_TARGET_ALBEDO));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexSSAO, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexSSIL, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssilSource), BLACK));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexSSR, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssrSource), BLANK));
        R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uTexORM, r3d_target_get(R3D_TARGET_ORM));

        R3D_SHADER_SET_COL3(deferred.ambient, uAmbientColor, R3D_CACHE_GET(environment.ambient.color));
        R3D_SHADER_SET_FLOAT(deferred.ambient, uAmbientEnergy, R3D_CACHE_GET(environment.ambient.energy));
        R3D_SHADER_SET_FLOAT(deferred.ambient, uMipCountSSR, (float)r3d_target_get_mip_count(R3D_TARGET_SSR));

        R3D_PRIMITIVE_DRAW_SCREEN();

        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexAlbedo);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexSSAO);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexSSIL);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexSSR);
        R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uTexORM);
    }
}

void pass_deferred_lights(r3d_target_t ssaoSource)
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
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexSSAO, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uTexORM, r3d_target_get(R3D_TARGET_ORM));

    R3D_SHADER_SET_FLOAT(deferred.lighting, uSSAOLightAffect, R3D_CACHE_GET(environment.ssao.lightAffect));

    /* --- Calculate lighting contributions --- */

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        r3d_rect_t dst = {0, 0, R3D_TARGET_WIDTH, R3D_TARGET_HEIGHT};
        if (light->type != R3D_LIGHT_DIR) {
            dst = r3d_light_get_screen_rect(light, &R3D_CACHE_GET(viewState.viewProj), dst.w, dst.h);
        }

        glScissor(dst.x, dst.y, dst.w, dst.h);

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

void pass_deferred_compose(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    R3D_SHADER_USE(deferred.compose);

    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uTexDiffuse, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uTexSpecular, r3d_target_get(R3D_TARGET_SPECULAR));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uTexDiffuse);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uTexSpecular);
}

static void pass_scene_forward_send_lights(const r3d_draw_call_t* call)
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

void pass_scene_forward(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);
    R3D_SHADER_USE(scene.forward);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    if (R3D_CACHE_GET(environment.background.sky.cubemap.id) != 0) {
        R3D_SHADER_BIND_SAMPLER_CUBE(scene.forward, uCubeIrradiance, R3D_CACHE_GET(environment.background.sky.irradiance.id));
        R3D_SHADER_BIND_SAMPLER_CUBE(scene.forward, uCubePrefilter, R3D_CACHE_GET(environment.background.sky.prefilter.id));
        R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uTexBrdfLut, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

        R3D_SHADER_SET_FLOAT(scene.forward, uAmbientEnergy, R3D_CACHE_GET(environment.ambient.energy));
        R3D_SHADER_SET_FLOAT(scene.forward, uReflectEnergy, R3D_CACHE_GET(environment.ambient.reflect));
        R3D_SHADER_SET_VEC4(scene.forward, uQuatSkybox, R3D_CACHE_GET(environment.background.rotation));
        R3D_SHADER_SET_INT(scene.forward, uHasSkybox, true);
    }
    else {
        R3D_SHADER_SET_FLOAT(scene.forward, uAmbientEnergy, R3D_CACHE_GET(environment.ambient.energy));
        R3D_SHADER_SET_COL3(scene.forward, uAmbientColor, R3D_CACHE_GET(environment.ambient.color));
        R3D_SHADER_SET_INT(scene.forward, uHasSkybox, false);
    }

    R3D_SHADER_SET_VEC3(scene.forward, uViewPosition, R3D_CACHE_GET(viewState.viewPosition));

    const r3d_frustum_t* frustum = NULL;
    if (!R3D_CACHE_FLAGS_HAS(state, R3D_FLAG_NO_FRUSTUM_CULLING)) {
        frustum = &R3D_CACHE_GET(viewState.frustum);
    }

    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS, R3D_DRAW_FORWARD_INST, R3D_DRAW_FORWARD) {
        pass_scene_forward_send_lights(call);
        raster_forward(call);
    }

    if (R3D_CACHE_GET(environment.background.sky.cubemap.id) != 0) {
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

void pass_scene_background(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    if (R3D_CACHE_GET(environment.background.sky.cubemap.id) != 0) {
        R3D_SHADER_USE(scene.skybox);
        glDisable(GL_CULL_FACE);

        R3D_SHADER_BIND_SAMPLER_CUBE(scene.skybox, uCubeSky, R3D_CACHE_GET(environment.background.sky.cubemap.id));
        R3D_SHADER_SET_FLOAT(scene.skybox, uSkyEnergy, R3D_CACHE_GET(environment.background.energy));
        R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D_CACHE_GET(environment.background.rotation));

        R3D_PRIMITIVE_DRAW_CUBE();

        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.skybox, uCubeSky);
    }
    else {
        Color backgroundSDR = R3D_CACHE_GET(environment.background.color);
        float backgroundNRG = R3D_CACHE_GET(environment.background.energy);
        Vector4 backgroundHDR = {
            .x = (float)backgroundSDR.r / 255 * backgroundNRG,
            .y = (float)backgroundSDR.g / 255 * backgroundNRG,
            .z = (float)backgroundSDR.b / 255 * backgroundNRG,
            .w = 1.0f,
        };
        R3D_SHADER_USE(scene.background);
        R3D_SHADER_SET_VEC4(scene.background, uColor, backgroundHDR);
        R3D_PRIMITIVE_DRAW_SCREEN();
    }
}

r3d_target_t pass_post_setup(r3d_target_t sceneTarget)
{
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    return r3d_target_swap_scene(sceneTarget);
}

r3d_target_t pass_post_fog(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fog);

    R3D_SHADER_BIND_SAMPLER_2D(post.fog, uTexColor, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.fog, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_INT(post.fog, uFogMode, R3D_CACHE_GET(environment.fog.mode));
    R3D_SHADER_SET_COL3(post.fog, uFogColor, R3D_CACHE_GET(environment.fog.color));
    R3D_SHADER_SET_FLOAT(post.fog, uFogStart, R3D_CACHE_GET(environment.fog.start));
    R3D_SHADER_SET_FLOAT(post.fog, uFogEnd, R3D_CACHE_GET(environment.fog.end));
    R3D_SHADER_SET_FLOAT(post.fog, uFogDensity, R3D_CACHE_GET(environment.fog.density));
    R3D_SHADER_SET_FLOAT(post.fog, uSkyAffect, R3D_CACHE_GET(environment.fog.skyAffect));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.fog, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.fog, uTexDepth);

    return sceneTarget;
}

r3d_target_t pass_post_dof(r3d_target_t sceneTarget)	
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.dof);

    R3D_SHADER_BIND_SAMPLER_2D(post.dof, uTexColor, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.dof, uTexDepth, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_FLOAT(post.dof, uFocusPoint, R3D_CACHE_GET(environment.dof.focusPoint));
    R3D_SHADER_SET_FLOAT(post.dof, uFocusScale, R3D_CACHE_GET(environment.dof.focusScale));
    R3D_SHADER_SET_FLOAT(post.dof, uMaxBlurSize, R3D_CACHE_GET(environment.dof.maxBlurSize));
    R3D_SHADER_SET_INT(post.dof, uDebugMode, R3D_CACHE_GET(environment.dof.debugMode));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.dof, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.dof, uTexDepth);

    return sceneTarget;
}

r3d_target_t pass_post_bloom(r3d_target_t sceneTarget)
{
    r3d_target_t sceneSource = r3d_target_swap_scene(sceneTarget);
    GLuint sceneSourceID = r3d_target_get(sceneSource);

    int mipCount = r3d_target_get_mip_count(R3D_TARGET_BLOOM);
    float txSrcW = 0, txSrcH = 0;
    int srcW = 0, srcH = 0;
    int dstW = 0, dstH = 0;

    R3D_TARGET_BIND(R3D_TARGET_BLOOM);

    /* --- Calculate bloom prefilter --- */

    float threshold = R3D_CACHE_GET(environment.bloom.threshold);
    float softThreshold = R3D_CACHE_GET(environment.bloom.threshold);

    float knee = threshold * softThreshold;

    Vector4 prefilter = {
        prefilter.x = threshold,
        prefilter.y = threshold - knee,
        prefilter.z = 2.0f * knee,
        prefilter.w = 0.25f / (knee + 0.00001f),
    };

    /* --- Adjust max mip count --- */

    int maxLevel = (int)((float)mipCount * R3D_CACHE_GET(environment.bloom.levels) + 0.5f);
    if (maxLevel > mipCount) maxLevel = mipCount;
    else if (maxLevel < 1) maxLevel = 1;

    /* --- Bloom: Karis average before downsampling --- */

    R3D_SHADER_USE(prepare.bloomDown);

    r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, 0);
    r3d_target_get_resolution(&srcW, &srcH, R3D_TARGET_BLOOM, 0);
    r3d_target_set_mip_level(0, 0);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomDown, uTexture, sceneSourceID);

    R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
    R3D_SHADER_SET_VEC4(prepare.bloomDown, uPrefilter, prefilter);
    R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, 0);

    R3D_PRIMITIVE_DRAW_SCREEN();

    /* --- Bloom: Downsampling --- */

    // It's okay to sample the target here
    // Given that we'll be sampling a different level from where we're writing
    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomDown, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = 1; dstLevel < maxLevel; dstLevel++)
    {
        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, dstLevel - 1);
        r3d_target_get_resolution(&srcW, &srcH, R3D_TARGET_BLOOM, dstLevel - 1);
        r3d_target_get_resolution(&dstW, &dstH, R3D_TARGET_BLOOM, dstLevel);

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

    for (int dstLevel = maxLevel - 2; dstLevel >= 0; dstLevel--)
    {
        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, dstLevel + 1);
        r3d_target_get_resolution(&srcW, &srcH, R3D_TARGET_BLOOM, dstLevel + 1);
        r3d_target_get_resolution(&dstW, &dstH, R3D_TARGET_BLOOM, dstLevel);

        r3d_target_set_mip_level(0, dstLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_FLOAT(prepare.bloomUp, uSrcLevel, dstLevel + 1);
        R3D_SHADER_SET_VEC2(prepare.bloomUp, uFilterRadius, (Vector2) {
            R3D_CACHE_GET(environment.bloom.filterRadius) * txSrcW,
            R3D_CACHE_GET(environment.bloom.filterRadius) * txSrcH
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

    R3D_SHADER_SET_INT(post.bloom, uBloomMode, R3D_CACHE_GET(environment.bloom.mode));
    R3D_SHADER_SET_FLOAT(post.bloom, uBloomIntensity, R3D_CACHE_GET(environment.bloom.intensity));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.bloom, uTexColor);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.bloom, uTexBloomBlur);

    return sceneTarget;
}

r3d_target_t pass_post_output(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.output);

    R3D_SHADER_BIND_SAMPLER_2D(post.output, uTexColor, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_FLOAT(post.output, uTonemapExposure, R3D_CACHE_GET(environment.tonemap.exposure));
    R3D_SHADER_SET_FLOAT(post.output, uTonemapWhite, R3D_CACHE_GET(environment.tonemap.white));
    R3D_SHADER_SET_INT(post.output, uTonemapMode, R3D_CACHE_GET(environment.tonemap.mode));
    R3D_SHADER_SET_FLOAT(post.output, uBrightness, R3D_CACHE_GET(environment.color.brightness));
    R3D_SHADER_SET_FLOAT(post.output, uContrast, R3D_CACHE_GET(environment.color.contrast));
    R3D_SHADER_SET_FLOAT(post.output, uSaturation, R3D_CACHE_GET(environment.color.saturation));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.output, uTexColor);

    return sceneTarget;
}

r3d_target_t pass_post_fxaa(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fxaa);

    R3D_SHADER_BIND_SAMPLER_2D(post.fxaa, uTexture, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_VEC2(post.fxaa, uTexelSize, (Vector2) {R3D_TARGET_TEXEL_SIZE});
    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.fxaa, uTexture);

    return sceneTarget;
}

void reset_raylib_state(void)
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
