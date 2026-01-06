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

#include "./r3d_core_state.h"

#include "./common/r3d_frustum.h"
#include "./common/r3d_helper.h"
#include "./common/r3d_pass.h"
#include "./common/r3d_math.h"

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_draw.h"
#include "./modules/r3d_env.h"
#include "r3d/r3d_core.h"

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

static void update_view_state(Camera3D camera, double aspect, double near, double far);
static void upload_view_state(void);
static void upload_env_state(void);

static void raster_depth(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP);
static void raster_depth_cube(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP);
static void raster_geometry(const r3d_draw_call_t* call);
static void raster_decal(const r3d_draw_call_t* call);
static void raster_forward(const r3d_draw_call_t* call);

static void pass_scene_shadow(void);
static void pass_scene_probes(void);
static void pass_scene_geometry(void);
static void pass_scene_prepass(void);
static void pass_scene_decals(void);

static r3d_target_t pass_prepare_ssao(void);
static r3d_target_t pass_prepare_ssil(void);
static r3d_target_t pass_prepare_ssr(void);

static void pass_deferred_lights(r3d_target_t ssaoSource);
static void pass_deferred_ambient(r3d_target_t ssaoSource, r3d_target_t ssilSource, r3d_target_t ssrSource);
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
    R3D_BeginEx((RenderTexture) {0}, camera);
}

void R3D_BeginEx(RenderTexture target, Camera3D camera)
{
    rlDrawRenderBatchActive();

    r3d_target_set_blit_screen(target);

    r3d_target_set_blit_mode(
        R3D_CORE_FLAGS_HAS(state, R3D_FLAG_ASPECT_KEEP),
        R3D_CORE_FLAGS_HAS(state, R3D_FLAG_BLIT_LINEAR)
    );

    update_view_state(
        camera,
        r3d_target_get_render_aspect(),
        rlGetCullDistanceNear(),
        rlGetCullDistanceFar()
    );

    r3d_draw_clear();
}

void R3D_End(void)
{
    /* --- Upload and bind uniform buffers --- */

    upload_view_state();
    upload_env_state();

    /* --- Update and collect all visible lights and probes and them (shadows and probes) --- */

    // TODO: Add `r3d_light_has()` along with a list of lights that cast shadows
    r3d_light_update_and_cull(&R3D.viewState.frustum, R3D.viewState.viewPosition);
    pass_scene_shadow();

    if (r3d_env_probe_has(R3D_ENV_PROBE_ARRAY_VALID)) {
        r3d_env_probe_update_and_cull(&R3D.viewState.frustum);
        pass_scene_probes();
    }

    /* --- Cull groups and sort all draw calls before rendering --- */

    r3d_draw_compute_visible_groups(&R3D.viewState.frustum);

    if (R3D_CORE_FLAGS_HAS(state, R3D_FLAG_OPAQUE_SORTING)) {
        r3d_draw_sort_list(R3D_DRAW_DEFERRED, R3D.viewState.viewPosition, R3D_DRAW_SORT_FRONT_TO_BACK);
    }

    if (R3D_CORE_FLAGS_HAS(state, R3D_FLAG_TRANSPARENT_SORTING)) {
        r3d_draw_sort_list(R3D_DRAW_PREPASS, R3D.viewState.viewPosition, R3D_DRAW_SORT_BACK_TO_FRONT);
        r3d_draw_sort_list(R3D_DRAW_FORWARD, R3D.viewState.viewPosition, R3D_DRAW_SORT_BACK_TO_FRONT);
    }

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
        if (R3D.environment.ssao.enabled) {
            ssaoSource = pass_prepare_ssao();
        }

        if (r3d_light_has_visible()) {
            pass_deferred_lights(ssaoSource);
        }

        r3d_target_t ssilSource = R3D_TARGET_INVALID;
        if (R3D.environment.ssil.enabled) {
            ssilSource = pass_prepare_ssil();
        }

        r3d_target_t ssrSource = R3D_TARGET_INVALID;
        if (R3D.environment.ssr.enabled) {
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

    if (R3D.environment.fog.mode != R3D_FOG_DISABLED) {
        sceneTarget = pass_post_fog(sceneTarget);
    }

    if (R3D.environment.dof.mode != R3D_DOF_DISABLED) {
        sceneTarget = pass_post_dof(sceneTarget);
    }

    if (R3D.environment.bloom.mode != R3D_BLOOM_DISABLED) {
        sceneTarget = pass_post_bloom(sceneTarget);
    }

    sceneTarget = pass_post_output(sceneTarget);

    if (R3D_CORE_FLAGS_HAS(state, R3D_FLAG_FXAA)) {
        sceneTarget = pass_post_fxaa(sceneTarget);
    }

    r3d_target_blit(r3d_target_swap_scene(sceneTarget));

    /* --- Reset states changed by R3D --- */

    reset_raylib_state();
}

void R3D_BeginCluster(BoundingBox aabb)
{
    if (!r3d_draw_cluster_begin(aabb)) {
        TraceLog(LOG_WARNING, "R3D: Failed to begin cluster");
    }
}

void R3D_EndCluster(void)
{
    if (!r3d_draw_cluster_end()) {
        TraceLog(LOG_WARNING, "R3D: Failed to end cluster");
    }
}

void R3D_DrawMesh(R3D_Mesh mesh, R3D_Material material, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_scale_translate((Vector3) {scale, scale, scale}, position);
    R3D_DrawMeshPro(mesh, material, transform);
}

void R3D_DrawMeshEx(R3D_Mesh mesh, R3D_Material material, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix tranform = r3d_matrix_scale_rotq_translate(scale, rotation, position);
    R3D_DrawMeshPro(mesh, material, tranform);
}

void R3D_DrawMeshPro(R3D_Mesh mesh, R3D_Material material, Matrix transform)
{
    if (!R3D_CORE_FLAGS_HAS(layers, mesh.layerMask)) {
        return;
    }

    r3d_draw_group_t drawGroup = {0};
    drawGroup.transform = transform;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};
    drawCall.material = material;
    drawCall.mesh = mesh;

    r3d_draw_call_push(&drawCall, false);
}

void R3D_DrawMeshInstanced(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawMeshInstancedEx(mesh, material, instances, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawMeshInstancedEx(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int count, Matrix transform)
{
    if (!R3D_CORE_FLAGS_HAS(layers, mesh.layerMask)) {
        return;
    }

    r3d_draw_group_t drawGroup = {0};

    drawGroup.transform = transform;
    drawGroup.instances = instances;
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity);

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};

    drawCall.material = material;
    drawCall.mesh = mesh;

    r3d_draw_call_push(&drawCall, false);
}

void R3D_DrawModel(R3D_Model model, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_scale_translate((Vector3) {scale, scale, scale}, position);
    R3D_DrawModelPro(model, transform);
}

void R3D_DrawModelEx(R3D_Model model, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix transform = r3d_matrix_scale_rotq_translate(scale, rotation, position);
    R3D_DrawModelPro(model, transform);
}

void R3D_DrawModelPro(R3D_Model model, Matrix transform)
{
    r3d_draw_group_t drawGroup = {0};

    drawGroup.aabb = model.aabb;
    drawGroup.transform = transform;
    drawGroup.texPose = model.skeleton.texBindPose;

    r3d_draw_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!R3D_CORE_FLAGS_HAS(layers, mesh->layerMask)) {
            return;
        }

        r3d_draw_call_t drawCall = {0};

        drawCall.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh = *mesh;

        r3d_draw_call_push(&drawCall, false);
    }
}

void R3D_DrawModelInstanced(R3D_Model model, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawModelInstancedEx(model, instances, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawModelInstancedEx(R3D_Model model, R3D_InstanceBuffer instances, int count, Matrix transform)
{
    r3d_draw_group_t drawGroup = {0};

    drawGroup.aabb = model.aabb;
    drawGroup.transform = transform;
    drawGroup.texPose = model.skeleton.texBindPose;

    drawGroup.transform = transform;
    drawGroup.instances = instances;
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity);

    r3d_draw_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!R3D_CORE_FLAGS_HAS(layers, mesh->layerMask)) {
            return;
        }

        r3d_draw_call_t drawCall = {0};

        drawCall.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh = *mesh;

        r3d_draw_call_push(&drawCall, false);
    }
}

void R3D_DrawAnimatedModel(R3D_Model model, R3D_AnimationPlayer player, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_scale_translate((Vector3) {scale, scale, scale}, position);
    R3D_DrawAnimatedModelPro(model, player, transform);
}

void R3D_DrawAnimatedModelEx(R3D_Model model, R3D_AnimationPlayer player, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix transform = r3d_matrix_scale_rotq_translate(scale, rotation, position);
    R3D_DrawAnimatedModelPro(model, player, transform);
}

void R3D_DrawAnimatedModelPro(R3D_Model model, R3D_AnimationPlayer player, Matrix transform)
{
    r3d_draw_group_t drawGroup = {0};

    drawGroup.aabb = model.aabb;
    drawGroup.transform = transform;
    drawGroup.texPose = (player.texGlobalPose > 0)
        ? player.texGlobalPose : model.skeleton.texBindPose;

    r3d_draw_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!R3D_CORE_FLAGS_HAS(layers, mesh->layerMask)) {
            return;
        }

        r3d_draw_call_t drawCall = {0};

        drawCall.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh = *mesh;

        r3d_draw_call_push(&drawCall, false);
    }
}

void R3D_DrawAnimatedModelInstanced(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawAnimatedModelInstancedEx(model, player, instances, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawAnimatedModelInstancedEx(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int count, Matrix transform)
{
    r3d_draw_group_t drawGroup = {0};

    drawGroup.aabb = model.aabb;
    drawGroup.transform = transform;
    drawGroup.texPose = (player.texGlobalPose > 0)
        ? player.texGlobalPose : model.skeleton.texBindPose;

    drawGroup.transform = transform;
    drawGroup.instances = instances;
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity);

    r3d_draw_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!R3D_CORE_FLAGS_HAS(layers, mesh->layerMask)) {
            return;
        }

        r3d_draw_call_t drawCall = {0};

        drawCall.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh = *mesh;

        r3d_draw_call_push(&drawCall, false);
    }
}

void R3D_DrawDecal(R3D_Decal decal, Matrix transform)
{
    r3d_draw_group_t drawGroup = {0};
    drawGroup.transform = transform;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};
    drawCall.material = decal.material;
    drawCall.mesh.shadowCastMode = R3D_SHADOW_CAST_DISABLED;
    drawCall.mesh.aabb.min = (Vector3) {-0.5f, -0.5f, -0.5f};
    drawCall.mesh.aabb.max = (Vector3) {+0.5f, +0.5f, +0.5f};

    r3d_draw_call_push(&drawCall, true);
}

void R3D_DrawDecalInstanced(R3D_Decal decal, R3D_InstanceBuffer instances, int count)
{
    r3d_draw_group_t drawGroup = {0};

    drawGroup.transform = R3D_MATRIX_IDENTITY;
    drawGroup.instances = instances;
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity);

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};

    drawCall.material = decal.material;
    drawCall.mesh.shadowCastMode = R3D_SHADOW_CAST_DISABLED;
    drawCall.mesh.aabb.min = (Vector3) {-0.5f, -0.5f, -0.5f};
    drawCall.mesh.aabb.max = (Vector3) {+0.5f, +0.5f, +0.5f};

    r3d_draw_call_push(&drawCall, true);
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

void update_view_state(Camera3D camera, double aspect, double near, double far)
{
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix proj = R3D_MATRIX_IDENTITY;

    if (camera.projection == CAMERA_PERSPECTIVE) {
        proj = MatrixPerspective(camera.fovy * DEG2RAD, aspect, near, far);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double top = camera.fovy / 2.0, right = top * aspect;
        proj = MatrixOrtho(-right, right, -top, top, near, far);
    }

    Matrix viewProj = r3d_matrix_multiply(&view, &proj);

    R3D.viewState.frustum = r3d_frustum_create(viewProj);
    R3D.viewState.viewPosition = camera.position;

    R3D.viewState.view = view;
    R3D.viewState.proj = proj;
    R3D.viewState.invView = MatrixInvert(view);
    R3D.viewState.invProj = MatrixInvert(proj);
    R3D.viewState.viewProj = viewProj;

    R3D.viewState.aspect = aspect;
    R3D.viewState.near = near;
    R3D.viewState.far = far;
}

void upload_view_state(void)
{
    r3d_shader_block_view_t view = {
        .viewPosition = R3D.viewState.viewPosition,
        .view = r3d_matrix_transpose(&R3D.viewState.view),
        .invView = r3d_matrix_transpose(&R3D.viewState.invView),
        .proj = r3d_matrix_transpose(&R3D.viewState.proj),
        .invProj = r3d_matrix_transpose(&R3D.viewState.invProj),
        .viewProj = r3d_matrix_transpose(&R3D.viewState.viewProj),
        .aspect = R3D.viewState.aspect,
        .near = R3D.viewState.near,
        .far = R3D.viewState.far,
    };

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_VIEW, &view);
}

void upload_env_state(void)
{
    R3D_EnvBackground* background = &R3D.environment.background;
    R3D_EnvAmbient* ambient = &R3D.environment.ambient;

    r3d_shader_block_env_t env = {0};

    int iProbe = 0;
    R3D_ENV_PROBE_FOR_EACH_VISIBLE(probe) {
        env.uProbes[iProbe++] = (struct r3d_shader_block_env_probe) {
            .position = probe->position,
            .falloff = probe->falloff,
            .range = probe->range,
            .irradiance = probe->irradiance,
            .prefilter = probe->prefilter,
            .enabled = true
        };
        if (iProbe >= R3D_SHADER_NUM_PROBES) {
            break;
        }
    }

    env.uAmbient.rotation = background->rotation;
    env.uAmbient.color = r3d_color_to_vec4(ambient->color);
    env.uAmbient.energy = ambient->energy;
    env.uAmbient.irradiance = (int)ambient->map.irradiance - 1;
    env.uAmbient.prefilter = (int)ambient->map.prefilter - 1;

    env.uNumPrefilterLevels = R3D_ENV_PREFILTER_MIPS;

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_ENV, &env);
}

void raster_depth(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depth, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.depth, uMatViewProj, *matVP);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.depth, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.depth, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depth, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depth, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depth, uMatInvView, R3D.viewState.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordScale, call->material.uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.depth, uAlbedoMap, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_SET_COL4(scene.depth, uAlbedoColor, R3D.colorSpace, call->material.albedo.color);

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
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.depth, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind samplers --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.depth, uAlbedoMap);
}

void raster_depth_cube(const r3d_draw_call_t* call, bool shadow, const Matrix* matVP)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depthCube, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.depthCube, uMatViewProj, *matVP);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.depthCube, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depthCube, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depthCube, uMatInvView, R3D.viewState.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordScale, call->material.uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.depthCube, uAlbedoMap, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_SET_COL4(scene.depthCube, uAlbedoColor, R3D.colorSpace, call->material.albedo.color);

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
        r3d_draw_instanced(call);
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

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.depthCube, uAlbedoMap);
}

void raster_probe(const r3d_draw_call_t* call, const Matrix* invView, const Matrix* viewProj)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.probe, uMatInvView, *invView);
    R3D_SHADER_SET_MAT4(scene.probe, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.probe, uMatNormal, matNormal);
    R3D_SHADER_SET_MAT4(scene.probe, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.probe, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.probe, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.probe, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.probe, uBillboard, call->material.billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.probe, uEmissionEnergy, call->material.emission.energy);
    R3D_SHADER_SET_FLOAT(scene.probe, uNormalScale, call->material.normal.scale);
    R3D_SHADER_SET_FLOAT(scene.probe, uOcclusion, call->material.orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.probe, uRoughness, call->material.orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.probe, uMetalness, call->material.orm.metalness);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.probe, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.probe, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.probe, uAlbedoColor, R3D.colorSpace, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.probe, uEmissionColor, R3D.colorSpace, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.probe, uAlbedoMap, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.probe, uNormalMap, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.probe, uEmissionMap, R3D_TEXTURE_SELECT(call->material.emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.probe, uOrmMap, R3D_TEXTURE_SELECT(call->material.orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(call->material.blendMode, call->material.transparencyMode);
    r3d_draw_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.probe, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.probe, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.probe, uAlbedoMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.probe, uNormalMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.probe, uEmissionMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.probe, uOrmMap);
}

void raster_geometry(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.geometry, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.geometry, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.geometry, uBoneMatricesTex, group->texPose);
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

    R3D_SHADER_SET_COL4(scene.geometry, uAlbedoColor, R3D.colorSpace, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.geometry, uEmissionColor, R3D.colorSpace, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uAlbedoMap, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uNormalMap, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uEmissionMap, R3D_TEXTURE_SELECT(call->material.emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.geometry, uOrmMap, R3D_TEXTURE_SELECT(call->material.orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.geometry, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.geometry, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uAlbedoMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uNormalMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uEmissionMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uOrmMap);
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

    R3D_SHADER_SET_COL4(scene.decal, uAlbedoColor, R3D.colorSpace, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.decal, uEmissionColor, R3D.colorSpace, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uAlbedoMap, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uNormalMap, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uEmissionMap, R3D_TEXTURE_SELECT(call->material.emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uOrmMap, R3D_TEXTURE_SELECT(call->material.orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(call->material.blendMode, call->material.transparencyMode);

    /* --- Disable face culling to avoid issues when camera is inside the decal bounding mesh --- */
    // TODO: Implement check for if camera is inside the mesh and apply the appropriate face culling / depth testing

    glDisable(GL_CULL_FACE);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.decal, uInstancing, true);
        r3d_primitive_draw_instanced(R3D_PRIMITIVE_CUBE, &group->instances, group->instanceCount);
    }
    else {
        R3D_SHADER_SET_INT(scene.decal, uInstancing, false);
        r3d_primitive_draw(R3D_PRIMITIVE_CUBE);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uAlbedoMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uNormalMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uEmissionMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uOrmMap);
}

void raster_forward(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.forward, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.forward, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER_1D(scene.forward, uBoneMatricesTex, group->texPose);
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

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.forward, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.forward, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.forward, uAlbedoColor, R3D.colorSpace, call->material.albedo.color);
    R3D_SHADER_SET_COL3(scene.forward, uEmissionColor, R3D.colorSpace, call->material.emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uAlbedoMap, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uNormalMap, R3D_TEXTURE_SELECT(call->material.normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uEmissionMap, R3D_TEXTURE_SELECT(call->material.emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uOrmMap, R3D_TEXTURE_SELECT(call->material.orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(call->material.blendMode, call->material.transparencyMode);
    r3d_draw_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.forward, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.forward, uInstancing, false);
        r3d_draw(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uAlbedoMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uNormalMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uEmissionMap);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uOrmMap);
}

void pass_scene_shadow(void)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        if (!r3d_light_shadow_should_be_updated(light, true)) {
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

                const r3d_frustum_t* frustum = &light->frustum[iFace];
                r3d_draw_compute_visible_groups(frustum);

                #define COND (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
                R3D_DRAW_FOR_EACH(call, COND, frustum, R3D_DRAW_DEFERRED_INST, R3D_DRAW_DEFERRED, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
                    raster_depth_cube(call, true, &light->matVP[iFace]);
                }
                #undef COND
            }

            // The bone matrices texture may have been bind during drawcalls, so UNBIND!
            R3D_SHADER_UNBIND_SAMPLER_1D(scene.depthCube, uBoneMatricesTex);
        }
        else {
            glClear(GL_DEPTH_BUFFER_BIT);
            R3D_SHADER_USE(scene.depth);

            const r3d_frustum_t* frustum = &light->frustum[0];
            r3d_draw_compute_visible_groups(frustum);

            #define COND (call->mesh.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
            R3D_DRAW_FOR_EACH(call, COND, frustum, R3D_DRAW_DEFERRED_INST, R3D_DRAW_DEFERRED, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
                raster_depth(call, true, &light->matVP[0]);
            }
            #undef COND

            // The bone matrices texture may have been bind during drawcalls, so UNBIND!
            R3D_SHADER_UNBIND_SAMPLER_1D(scene.depth, uBoneMatricesTex);
        }
    }
}

static void pass_scene_probe_send_lights(const r3d_draw_call_t* call, bool probeShadows)
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

        R3D_SHADER_SET_INT(scene.probe, uLights[iLight].enabled, true);
        R3D_SHADER_SET_INT(scene.probe, uLights[iLight].type, light->type);
        R3D_SHADER_SET_VEC3(scene.probe, uLights[iLight].color, light->color);
        R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].specular, light->specular);
        R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].energy, light->energy);

        if (light->type == R3D_LIGHT_DIR) {
            R3D_SHADER_SET_VEC3(scene.probe, uLights[iLight].direction, light->direction);
        }
        else if (light->type == R3D_LIGHT_SPOT) {
            R3D_SHADER_SET_VEC3(scene.probe, uLights[iLight].position, light->position);
            R3D_SHADER_SET_VEC3(scene.probe, uLights[iLight].direction, light->direction);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].range, light->range);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].attenuation, light->attenuation);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].innerCutOff, light->innerCutOff);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].outerCutOff, light->outerCutOff);
        }
        else if (light->type == R3D_LIGHT_OMNI) {
            R3D_SHADER_SET_VEC3(scene.probe, uLights[iLight].position, light->position);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].range, light->range);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].attenuation, light->attenuation);
        }

        if (light->shadow && probeShadows) {
            if (light->type == R3D_LIGHT_OMNI) {
                R3D_SHADER_BIND_SAMPLER_CUBE(scene.probe, uShadowMapCube[iLight], light->shadowMap.tex);
            }
            else {
                R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].shadowTexelSize, light->shadowTexelSize);
                R3D_SHADER_BIND_SAMPLER_2D(scene.probe, uShadowMap2D[iLight], light->shadowMap.tex);
                R3D_SHADER_SET_MAT4(scene.probe, uLightViewProj[iLight], light->matVP);
            }
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].shadowSoftness, light->shadowSoftness);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].shadowDepthBias, light->shadowDepthBias);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].shadowSlopeBias, light->shadowSlopeBias);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].near, light->near);
            R3D_SHADER_SET_FLOAT(scene.probe, uLights[iLight].far, light->far);
            R3D_SHADER_SET_INT(scene.probe, uLights[iLight].shadow, true);
        }
        else {
            R3D_SHADER_SET_INT(scene.probe, uLights[iLight].shadow, false);
        }

        if (++iLight == R3D_SHADER_NUM_FORWARD_LIGHTS) {
            break;
        }
    }

    for (int i = iLight; i < R3D_SHADER_NUM_FORWARD_LIGHTS; i++) {
        R3D_SHADER_SET_INT(scene.probe, uLights[i].enabled, false);
    }
}

void pass_scene_probes(void)
{
    #define PROBES_DRAW_LISTS   \
        R3D_DRAW_DEFERRED_INST, \
        R3D_DRAW_PREPASS_INST,  \
        R3D_DRAW_FORWARD_INST,  \
        R3D_DRAW_DEFERRED,      \
        R3D_DRAW_PREPASS,       \
        R3D_DRAW_FORWARD,

    R3D_ENV_PROBE_FOR_EACH_VALID(probe)
    {
        if (!r3d_env_probe_should_be_updated(probe, true)) {
            continue;
        }

        for (int iFace = 0; iFace < 6; iFace++)
        {
            Matrix viewProj = r3d_matrix_multiply(&probe->view[iFace], &probe->proj[iFace]);
            Matrix invView = MatrixInvert(probe->view[iFace]);

            /* --- Generates the list of visible groups for the current face of the capture --- */

            const r3d_frustum_t* frustum = &probe->frustum[iFace];
            r3d_draw_compute_visible_groups(frustum);

            /* --- Render scene --- */

            R3D_SHADER_USE(scene.probe);

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_TRUE);
            glEnable(GL_BLEND);

            R3D_SHADER_BIND_SAMPLER_CUBE_ARRAY(scene.probe, uIrradianceTex, r3d_env_irradiance_get());
            R3D_SHADER_BIND_SAMPLER_CUBE_ARRAY(scene.probe, uPrefilterTex, r3d_env_prefilter_get());
            R3D_SHADER_BIND_SAMPLER_2D(scene.probe, uBrdfLutTex, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));
            R3D_SHADER_SET_VEC3(scene.probe, uViewPosition, probe->position);
            R3D_SHADER_SET_INT(scene.probe, uProbeInterior, probe->interior);

            r3d_env_capture_bind_fbo(iFace, 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            R3D_DRAW_FOR_EACH(call, true, frustum, PROBES_DRAW_LISTS) {
                pass_scene_probe_send_lights(call, probe->shadows);
                raster_probe(call, &invView, &viewProj);
            }

            R3D_SHADER_UNBIND_SAMPLER_CUBE_ARRAY(scene.probe, uIrradianceTex);
            R3D_SHADER_UNBIND_SAMPLER_CUBE_ARRAY(scene.probe, uPrefilterTex);
            R3D_SHADER_UNBIND_SAMPLER_2D(scene.probe, uBrdfLutTex);

            for (int i = 0; i < R3D_SHADER_NUM_FORWARD_LIGHTS; i++) {
                R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.probe, uShadowMapCube[i]);
                R3D_SHADER_UNBIND_SAMPLER_2D(scene.probe, uShadowMap2D[i]);
            }

            // NOTE: The storage texture of the matrices may have been bind during drawcalls
            R3D_SHADER_UNBIND_SAMPLER_1D(scene.probe, uBoneMatricesTex);

            /* --- Render background --- */

            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);

            if (!probe->interior && R3D.environment.background.sky.texture != 0) {
                R3D_SHADER_USE(scene.skybox);
                glDisable(GL_CULL_FACE);

                R3D_SHADER_BIND_SAMPLER_CUBE(scene.skybox, uSkyMap, R3D.environment.background.sky.texture);
                R3D_SHADER_SET_FLOAT(scene.skybox, uSkyEnergy, R3D.environment.background.energy);
                R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.environment.background.rotation);
                R3D_SHADER_SET_MAT4(scene.skybox, uMatView, probe->view[iFace]);
                R3D_SHADER_SET_MAT4(scene.skybox, uMatProj, probe->proj[iFace]);

                R3D_PRIMITIVE_DRAW_CUBE();

                R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.skybox, uSkyMap);
            }
            else {
                Vector4 background = r3d_color_to_linear_scaled_vec4(
                    R3D.environment.background.color, R3D.colorSpace,
                    R3D.environment.background.energy
                );
                R3D_SHADER_USE(scene.background);
                R3D_SHADER_SET_VEC4(scene.background, uColor, background);
                R3D_PRIMITIVE_DRAW_SCREEN();
            }

            /* --- Generates mipmaps of the rendered face by downsampling it --- */

            R3D_SHADER_USE(prepare.cubefaceDown);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);

            R3D_SHADER_BIND_SAMPLER_CUBE(prepare.cubefaceDown, uSourceTex, r3d_env_capture_get());
            R3D_SHADER_SET_INT(prepare.cubefaceDown, uSourceFace, iFace);

            for (int mipLevel = 1; mipLevel < R3D_ENV_CAPTURE_MIPS; mipLevel++) {
                r3d_env_capture_bind_fbo(iFace, mipLevel);
                R3D_SHADER_SET_FLOAT(prepare.cubefaceDown, uSourceTexel, 1.0 / (R3D_ENV_CAPTURE_SIZE >> (mipLevel - 1)));
                R3D_SHADER_SET_FLOAT(prepare.cubefaceDown, uSourceLod, mipLevel - 1);
                R3D_PRIMITIVE_DRAW_SCREEN();
            }

            R3D_SHADER_UNBIND_SAMPLER_CUBE(prepare.cubefaceDown, uSourceTex);
        }

        /* --- Generate irradiance and prefilter maps --- */

        if (probe->irradiance >= 0) {
            r3d_pass_prepare_irradiance(probe->irradiance, r3d_env_capture_get(), R3D_ENV_CAPTURE_SIZE);
        }

        if (probe->prefilter >= 0) {
            r3d_pass_prepare_prefilter(probe->prefilter, r3d_env_capture_get(), R3D_ENV_CAPTURE_SIZE);
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

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_DEFERRED_INST, R3D_DRAW_DEFERRED) {
        raster_geometry(call);
    }

    // The bone matrices texture may have been bind during drawcalls, so UNBIND!
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.geometry, uBoneMatricesTex);
}

void pass_scene_prepass(void)
{
    /* --- First render only depth --- */

    R3D_TARGET_BIND(R3D_TARGET_DEPTH);
    R3D_SHADER_USE(scene.depth);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS) {
        raster_depth(call, false, &R3D.viewState.viewProj);
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
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.forward, uBoneMatricesTex);
}

void pass_scene_decals(void)
{
    R3D_TARGET_BIND(R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.decal);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    R3D_SHADER_BIND_SAMPLER_2D(scene.decal, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_DECAL_INST, R3D_DRAW_DECAL) {
        raster_decal(call);
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uDepthTex);
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

    R3D_SHADER_SET_INT(prepare.ssao, uSampleCount,  R3D.environment.ssao.sampleCount);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uRadius,  R3D.environment.ssao.radius);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uBias, R3D.environment.ssao.bias);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uIntensity, R3D.environment.ssao.intensity);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uPower, R3D.environment.ssao.power);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssao, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uNormalTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssao, uDepthTex);

    /* --- Denoise SSAO --- */

    R3D_SHADER_USE(prepare.atrousWavelet);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.atrousWavelet, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.atrousWavelet, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    for (int i = 0; i < 4; i++) {
        R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);
        R3D_SHADER_BIND_SAMPLER_2D(prepare.atrousWavelet, uSourceTex, r3d_target_get(ssaoTarget));
        R3D_SHADER_SET_INT(prepare.atrousWavelet, uStepSize, 1 << i);
        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.atrousWavelet, uSourceTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.atrousWavelet, uNormalTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.atrousWavelet, uDepthTex);

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

    bool needConvergence = (R3D.environment.ssil.convergence >= 0.1f);
    bool needBounce = (R3D.environment.ssil.bounce >= 0.01f);
    bool needHistory = (needConvergence || needBounce);

    if (needHistory && r3d_target_get_or_null(SSIL_HISTORY) == 0) {
        R3D_TARGET_CLEAR(SSIL_HISTORY);
    }

    /* --- Calculate SSIL --- */

    R3D_TARGET_BIND(SSIL_CURRENT);
    R3D_SHADER_USE(prepare.ssil);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uLightingTex, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uHistoryTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(SSIL_HISTORY), BLACK));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssil, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleCount, (float)R3D.environment.ssil.sampleCount);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleRadius, R3D.environment.ssil.sampleRadius);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSliceCount, (float)R3D.environment.ssil.sliceCount);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uHitThickness, R3D.environment.ssil.hitThickness);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uConvergence, R3D.environment.ssil.convergence);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uAoPower, R3D.environment.ssil.aoPower);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uBounce, R3D.environment.ssil.bounce);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uEnergy, R3D.environment.ssil.energy);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uLightingTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uHistoryTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uNormalTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssil, uDepthTex);

    /* --- Blur SSIL (Dual Filtering) --- */

    r3d_target_t SSIL_MIP = R3D_TARGET_SSIL_MIP;

    int mipCount = r3d_target_get_mip_count(SSIL_MIP);
    int mipMax = MIN(3, mipCount);

    R3D_TARGET_BIND(SSIL_MIP);

    // Downsample
    R3D_SHADER_USE(prepare.blurDown);
    R3D_SHADER_BIND_SAMPLER_2D(prepare.blurDown, uSourceTex, r3d_target_get(SSIL_CURRENT));
    for (int mipLevel = 1; mipLevel < mipMax; mipLevel++)
    {
        int dstW, dstH;
        r3d_target_get_resolution(&dstW, &dstH, SSIL_MIP, mipLevel);
        r3d_target_set_mip_level(0, mipLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_INT(prepare.blurDown, uSourceLod, mipLevel - 1);
        R3D_PRIMITIVE_DRAW_SCREEN();

        if (mipLevel == 1) {
            R3D_SHADER_BIND_SAMPLER_2D(prepare.blurDown, uSourceTex, r3d_target_get(SSIL_MIP));
        }
    }
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.blurDown, uSourceTex);

    // Upsample
    R3D_SHADER_USE(prepare.blurUp);
    R3D_SHADER_BIND_SAMPLER_2D(prepare.blurUp, uSourceTex, r3d_target_get(SSIL_MIP));
    for (int mipLevel = mipMax - 2; mipLevel >= 0; mipLevel--)
    {
        int dstW, dstH;
        r3d_target_get_resolution(&dstW, &dstH, SSIL_MIP, mipLevel);
        r3d_target_set_mip_level(0, mipLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_INT(prepare.blurUp, uSourceLod, mipLevel + 1);
        R3D_PRIMITIVE_DRAW_SCREEN();
    }
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.blurUp, uSourceTex);

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

    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uLightingTex, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uAlbedoTex, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uOrmTex, r3d_target_get(R3D_TARGET_ORM));
    R3D_SHADER_BIND_SAMPLER_2D(prepare.ssr, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_INT(prepare.ssr, uMaxRaySteps, R3D.environment.ssr.maxRaySteps);
    R3D_SHADER_SET_INT(prepare.ssr, uBinarySearchSteps, R3D.environment.ssr.binarySearchSteps);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uRayMarchLength, R3D.environment.ssr.rayMarchLength);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uDepthThickness, R3D.environment.ssr.depthThickness);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uDepthTolerance, R3D.environment.ssr.depthTolerance);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFadeStart, R3D.environment.ssr.edgeFadeStart);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFadeEnd, R3D.environment.ssr.edgeFadeEnd);

    R3D_SHADER_SET_VEC3(prepare.ssr, uAmbientColor, r3d_color_to_vec3(R3D.environment.ambient.color));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uAmbientEnergy, R3D.environment.ambient.energy);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uLightingTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uAlbedoTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uNormalTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uOrmTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.ssr, uDepthTex);

    r3d_target_gen_mipmap(R3D_TARGET_SSR);

    return R3D_TARGET_SSR;
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

    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uAlbedoTex, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uSsaoTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.lighting, uOrmTex, r3d_target_get(R3D_TARGET_ORM));

    R3D_SHADER_SET_FLOAT(deferred.lighting, uSSAOLightAffect, R3D.environment.ssao.lightAffect);

    /* --- Calculate lighting contributions --- */

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        r3d_rect_t dst = {0, 0, R3D_TARGET_WIDTH, R3D_TARGET_HEIGHT};
        if (light->type != R3D_LIGHT_DIR) {
            dst = r3d_light_get_screen_rect(light, &R3D.viewState.viewProj, dst.w, dst.h);
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

    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uAlbedoTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uNormalTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uDepthTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uOrmTex);

    R3D_SHADER_UNBIND_SAMPLER_CUBE(deferred.lighting, uLight.shadowCubemap);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.lighting, uLight.shadowMap);

    /* --- Reset undesired state --- */

    glDisable(GL_SCISSOR_TEST);
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

    R3D_TARGET_BIND(R3D_TARGET_LIGHTING);
    R3D_SHADER_USE(deferred.ambient);

    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uAlbedoTex, r3d_target_get(R3D_TARGET_ALBEDO));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uSsaoTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uSsilTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssilSource), BLACK));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uSsrTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssrSource), BLANK));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uOrmTex, r3d_target_get(R3D_TARGET_ORM));
    R3D_SHADER_BIND_SAMPLER_CUBE_ARRAY(deferred.ambient, uIrradianceTex, r3d_env_irradiance_get());
    R3D_SHADER_BIND_SAMPLER_CUBE_ARRAY(deferred.ambient, uPrefilterTex, r3d_env_prefilter_get());
    R3D_SHADER_BIND_SAMPLER_2D(deferred.ambient, uBrdfLutTex, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));
    R3D_SHADER_SET_FLOAT(deferred.ambient, uMipCountSSR, (float)r3d_target_get_mip_count(R3D_TARGET_SSR));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uAlbedoTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uNormalTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uDepthTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uSsaoTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uSsilTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uSsrTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uOrmTex);
    R3D_SHADER_UNBIND_SAMPLER_CUBE_ARRAY(deferred.ambient, uIrradianceTex);
    R3D_SHADER_UNBIND_SAMPLER_CUBE_ARRAY(deferred.ambient, uPrefilterTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.ambient, uBrdfLutTex);
}

void pass_deferred_compose(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    R3D_SHADER_USE(deferred.compose);

    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uDiffuseTex, r3d_target_get(R3D_TARGET_DIFFUSE));
    R3D_SHADER_BIND_SAMPLER_2D(deferred.compose, uSpecularTex, r3d_target_get(R3D_TARGET_SPECULAR));

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uDiffuseTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(deferred.compose, uSpecularTex);
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
                R3D_SHADER_SET_MAT4(scene.forward, uLightViewProj[iLight], light->matVP);
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

        if (++iLight == R3D_SHADER_NUM_FORWARD_LIGHTS) {
            break;
        }
    }

    for (int i = iLight; i < R3D_SHADER_NUM_FORWARD_LIGHTS; i++) {
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

    R3D_SHADER_BIND_SAMPLER_CUBE_ARRAY(scene.forward, uIrradianceTex, r3d_env_irradiance_get());
    R3D_SHADER_BIND_SAMPLER_CUBE_ARRAY(scene.forward, uPrefilterTex, r3d_env_prefilter_get());
    R3D_SHADER_BIND_SAMPLER_2D(scene.forward, uBrdfLutTex, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

    R3D_SHADER_SET_VEC3(scene.forward, uViewPosition, R3D.viewState.viewPosition);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_PREPASS_INST, R3D_DRAW_PREPASS, R3D_DRAW_FORWARD_INST, R3D_DRAW_FORWARD) {
        pass_scene_forward_send_lights(call);
        raster_forward(call);
    }

    if (R3D.environment.background.sky.texture != 0) {
        R3D_SHADER_UNBIND_SAMPLER_CUBE_ARRAY(scene.forward, uIrradianceTex);
        R3D_SHADER_UNBIND_SAMPLER_CUBE_ARRAY(scene.forward, uPrefilterTex);
        R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uBrdfLutTex);
    }

    for (int i = 0; i < R3D_SHADER_NUM_FORWARD_LIGHTS; i++) {
        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.forward, uShadowMapCube[i]);
        R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uShadowMap2D[i]);
    }

    // NOTE: The storage texture of the matrices may have been bind during drawcalls
    R3D_SHADER_UNBIND_SAMPLER_1D(scene.forward, uBoneMatricesTex);
}

void pass_scene_background(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(sceneTarget, R3D_TARGET_DEPTH);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    if (R3D.environment.background.sky.texture != 0) {
        R3D_SHADER_USE(scene.skybox);
        glDisable(GL_CULL_FACE);

        R3D_SHADER_BIND_SAMPLER_CUBE(scene.skybox, uSkyMap, R3D.environment.background.sky.texture);
        R3D_SHADER_SET_FLOAT(scene.skybox, uSkyEnergy, R3D.environment.background.energy);
        R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.environment.background.rotation);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatView, R3D.viewState.view);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatProj, R3D.viewState.proj);

        R3D_PRIMITIVE_DRAW_CUBE();

        R3D_SHADER_UNBIND_SAMPLER_CUBE(scene.skybox, uSkyMap);
    }
    else {
        Vector4 background = r3d_color_to_linear_scaled_vec4(
            R3D.environment.background.color, R3D.colorSpace,
            R3D.environment.background.energy
        );
        R3D_SHADER_USE(scene.background);
        R3D_SHADER_SET_VEC4(scene.background, uColor, background);
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

    R3D_SHADER_BIND_SAMPLER_2D(post.fog, uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.fog, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_INT(post.fog, uFogMode, R3D.environment.fog.mode);
    R3D_SHADER_SET_COL3(post.fog, uFogColor, R3D.colorSpace, R3D.environment.fog.color);
    R3D_SHADER_SET_FLOAT(post.fog, uFogStart, R3D.environment.fog.start);
    R3D_SHADER_SET_FLOAT(post.fog, uFogEnd, R3D.environment.fog.end);
    R3D_SHADER_SET_FLOAT(post.fog, uFogDensity, R3D.environment.fog.density);
    R3D_SHADER_SET_FLOAT(post.fog, uSkyAffect, R3D.environment.fog.skyAffect);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.fog, uSceneTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.fog, uDepthTex);

    return sceneTarget;
}

r3d_target_t pass_post_dof(r3d_target_t sceneTarget)	
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.dof);

    R3D_SHADER_BIND_SAMPLER_2D(post.dof, uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER_2D(post.dof, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

    R3D_SHADER_SET_FLOAT(post.dof, uFocusPoint, R3D.environment.dof.focusPoint);
    R3D_SHADER_SET_FLOAT(post.dof, uFocusScale, R3D.environment.dof.focusScale);
    R3D_SHADER_SET_FLOAT(post.dof, uMaxBlurSize, R3D.environment.dof.maxBlurSize);
    R3D_SHADER_SET_INT(post.dof, uDebugMode, R3D.environment.dof.debugMode);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.dof, uSceneTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.dof, uDepthTex);

    return sceneTarget;
}

r3d_target_t pass_post_bloom(r3d_target_t sceneTarget)
{
    r3d_target_t sceneSource = r3d_target_swap_scene(sceneTarget);
    GLuint sceneSourceID = r3d_target_get(sceneSource);

    int mipCount = r3d_target_get_mip_count(R3D_TARGET_BLOOM);
    float txSrcW = 0, txSrcH = 0;
    int dstW = 0, dstH = 0;

    R3D_TARGET_BIND(R3D_TARGET_BLOOM);

    /* --- Calculate bloom prefilter --- */

    float threshold = R3D.environment.bloom.threshold;
    float softThreshold = R3D.environment.bloom.threshold;

    float knee = threshold * softThreshold;

    Vector4 prefilter = {
        prefilter.x = threshold,
        prefilter.y = threshold - knee,
        prefilter.z = 2.0f * knee,
        prefilter.w = 0.25f / (knee + 0.00001f),
    };

    /* --- Adjust max mip count --- */

    int maxLevel = (int)((float)mipCount * R3D.environment.bloom.levels + 0.5f);
    if (maxLevel > mipCount) maxLevel = mipCount;
    else if (maxLevel < 1) maxLevel = 1;

    /* --- Karis average for the first downsampling to half res --- */

    R3D_SHADER_USE(prepare.bloomDown);

    r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, 0);
    r3d_target_set_mip_level(0, 0);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomDown, uTexture, sceneSourceID);

    R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
    R3D_SHADER_SET_VEC4(prepare.bloomDown, uPrefilter, prefilter);
    R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, 0);

    R3D_PRIMITIVE_DRAW_SCREEN();

    /* --- Bloom Downsampling --- */

    // It's okay to sample the target here
    // Given that we'll be sampling a different level from where we're writing
    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomDown, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = 1; dstLevel < maxLevel; dstLevel++)
    {
        int srcLevel = dstLevel - 1;

        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, srcLevel);
        r3d_target_get_resolution(&dstW, &dstH, R3D_TARGET_BLOOM, dstLevel);
        r3d_target_set_mip_level(0, dstLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
        R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, dstLevel);

        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.bloomDown, uTexture);

    /* --- Bloom Upsampling --- */

    R3D_SHADER_USE(prepare.bloomUp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    R3D_SHADER_BIND_SAMPLER_2D(prepare.bloomUp, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = maxLevel - 2; dstLevel >= 0; dstLevel--)
    {
        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, dstLevel + 1);
        r3d_target_get_resolution(&dstW, &dstH, R3D_TARGET_BLOOM, dstLevel);
        r3d_target_set_mip_level(0, dstLevel);
        glViewport(0, 0, dstW, dstH);

        R3D_SHADER_SET_FLOAT(prepare.bloomUp, uSrcLevel, dstLevel + 1);
        R3D_SHADER_SET_VEC2(prepare.bloomUp, uFilterRadius, (Vector2) {
            R3D.environment.bloom.filterRadius * txSrcW,
            R3D.environment.bloom.filterRadius * txSrcH
        });

        R3D_PRIMITIVE_DRAW_SCREEN();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.bloomUp, uTexture);

    glDisable(GL_BLEND);

    /* --- Apply bloom to the scene --- */

    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.bloom);

    R3D_SHADER_BIND_SAMPLER_2D(post.bloom, uSceneTex, sceneSourceID);
    R3D_SHADER_BIND_SAMPLER_2D(post.bloom, uBloomTex, r3d_target_get(R3D_TARGET_BLOOM));

    R3D_SHADER_SET_INT(post.bloom, uBloomMode, R3D.environment.bloom.mode);
    R3D_SHADER_SET_FLOAT(post.bloom, uBloomIntensity, R3D.environment.bloom.intensity);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.bloom, uSceneTex);
    R3D_SHADER_UNBIND_SAMPLER_2D(post.bloom, uBloomTex);

    return sceneTarget;
}

r3d_target_t pass_post_output(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.output);

    R3D_SHADER_BIND_SAMPLER_2D(post.output, uSceneTex, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_FLOAT(post.output, uTonemapExposure, R3D.environment.tonemap.exposure);
    R3D_SHADER_SET_FLOAT(post.output, uTonemapWhite, R3D.environment.tonemap.white);
    R3D_SHADER_SET_INT(post.output, uTonemapMode, R3D.environment.tonemap.mode);
    R3D_SHADER_SET_FLOAT(post.output, uBrightness, R3D.environment.color.brightness);
    R3D_SHADER_SET_FLOAT(post.output, uContrast, R3D.environment.color.contrast);
    R3D_SHADER_SET_FLOAT(post.output, uSaturation, R3D.environment.color.saturation);

    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.output, uSceneTex);

    return sceneTarget;
}

r3d_target_t pass_post_fxaa(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fxaa);

    R3D_SHADER_BIND_SAMPLER_2D(post.fxaa, uSourceTex, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_VEC2(post.fxaa, uSourceTexel, (Vector2) {R3D_TARGET_TEXEL_SIZE});
    R3D_PRIMITIVE_DRAW_SCREEN();

    R3D_SHADER_UNBIND_SAMPLER_2D(post.fxaa, uSourceTex);

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
