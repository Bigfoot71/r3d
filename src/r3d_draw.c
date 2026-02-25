/* r3d_draw.h -- R3D Draw Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_draw.h>
#include <r3d_config.h>
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

#include "./modules/r3d_texture.h"
#include "./modules/r3d_driver.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_render.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_env.h"

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

static void update_view_state(Camera3D camera, double near, double far);
static void upload_light_array_block_for_mesh(const r3d_render_call_t* call, bool shadow);
static void upload_frame_block(void);
static void upload_view_block(void);
static void upload_env_block(void);

static void raster_depth(const r3d_render_call_t* call, const Matrix* viewProj, r3d_light_t* light);
static void raster_depth_cube(const r3d_render_call_t* call, const Matrix* viewProj, r3d_light_t* light);
static void raster_geometry(const r3d_render_call_t* call, bool matchPrepass);
static void raster_decal(const r3d_render_call_t* call);
static void raster_forward(const r3d_render_call_t* call);
static void raster_unlit(const r3d_render_call_t* call, const Matrix* invView, const Matrix* viewProj);

static void pass_scene_shadow(void);
static void pass_scene_probes(void);
static void pass_scene_geometry(void);
static void pass_scene_prepass(void);
static void pass_scene_decals(void);

static void pass_prepare_depth_pyramid(void);
static r3d_target_t pass_prepare_ssao(void);
static r3d_target_t pass_prepare_ssil(void);
static r3d_target_t pass_prepare_ssgi(void);
static r3d_target_t pass_prepare_ssr(void);

static void pass_deferred_lights(void);
static void pass_deferred_ambient(r3d_target_t ssaoSource, r3d_target_t ssilSource, r3d_target_t ssgiSource);
static void pass_deferred_compose(r3d_target_t sceneTarget, r3d_target_t ssrSource);

static void pass_scene_forward(r3d_target_t sceneTarget);
static void pass_scene_background(r3d_target_t sceneTarget);

static r3d_target_t pass_post_setup(r3d_target_t sceneTarget);
static r3d_target_t pass_post_fog(r3d_target_t sceneTarget);
static r3d_target_t pass_post_dof(r3d_target_t sceneTarget);
static r3d_target_t pass_post_bloom(r3d_target_t sceneTarget);
static r3d_target_t pass_post_screen(r3d_target_t sceneTarget);
static r3d_target_t pass_post_output(r3d_target_t sceneTarget);
static r3d_target_t pass_post_fxaa(r3d_target_t sceneTarget);
static r3d_target_t pass_post_smaa(r3d_target_t sceneTarget);

static void blit_to_screen(r3d_target_t source);
static void visualize_to_screen(r3d_target_t source);

static void cleanup_after_render(void);

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
    update_view_state(camera, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    R3D.screen = target;
    r3d_render_clear();
}

void R3D_End(void)
{
    /* --- Invalidates OpenGL cache and save some infos --- */

    r3d_driver_invalidate_cache();
    r3d_driver_store_viewport();

    /* --- Upload and bind uniform buffers --- */

    upload_frame_block();
    upload_view_block();
    upload_env_block();

    /* --- Update all visible lights and render their shadow maps --- */

    bool hasVisibleShadows = false;
    r3d_light_update_and_cull(&R3D.viewState.frustum, R3D.viewState.position, &hasVisibleShadows);

    if (hasVisibleShadows) {
        pass_scene_shadow();
        r3d_shader_bind_sampler(R3D_SHADER_SAMPLER_SHADOW_DIR, r3d_light_shadow_get(R3D_LIGHT_DIR));
        r3d_shader_bind_sampler(R3D_SHADER_SAMPLER_SHADOW_SPOT, r3d_light_shadow_get(R3D_LIGHT_SPOT));
        r3d_shader_bind_sampler(R3D_SHADER_SAMPLER_SHADOW_OMNI, r3d_light_shadow_get(R3D_LIGHT_OMNI));
    }

    /* --- Update all visible environment probes and render their cubemaps --- */

    bool hasVisibleProbes = false;
    r3d_env_probe_update_and_cull(&R3D.viewState.frustum, &hasVisibleProbes);

    if (hasVisibleProbes || R3D.environment.ambient.map.flags != 0) {
        r3d_shader_bind_sampler(R3D_SHADER_SAMPLER_IBL_IRRADIANCE, r3d_env_irradiance_get());
        r3d_shader_bind_sampler(R3D_SHADER_SAMPLER_IBL_PREFILTER, r3d_env_prefilter_get());
        r3d_shader_bind_sampler(R3D_SHADER_SAMPLER_IBL_BRDF_LUT, r3d_texture_get(R3D_TEXTURE_BRDF_LUT));
        if (hasVisibleProbes) pass_scene_probes(); // Must have the IBL bind in case of ambient map
    }

    /* --- Cull groups and sort all draw calls before rendering --- */

    r3d_render_cull_groups(&R3D.viewState.frustum);

    r3d_render_sort_list(R3D_RENDER_LIST_OPAQUE, R3D.viewState.position, R3D_RENDER_SORT_FRONT_TO_BACK);
    r3d_render_sort_list(R3D_RENDER_LIST_TRANSPARENT, R3D.viewState.position, R3D_RENDER_SORT_BACK_TO_FRONT);
    r3d_render_sort_list(R3D_RENDER_LIST_DECAL, R3D.viewState.position, R3D_RENDER_SORT_MATERIAL_ONLY);

    r3d_render_sort_list(R3D_RENDER_LIST_OPAQUE_INST, R3D.viewState.position, R3D_RENDER_SORT_MATERIAL_ONLY);
    r3d_render_sort_list(R3D_RENDER_LIST_TRANSPARENT_INST, R3D.viewState.position, R3D_RENDER_SORT_MATERIAL_ONLY);
    r3d_render_sort_list(R3D_RENDER_LIST_DECAL_INST, R3D.viewState.position, R3D_RENDER_SORT_MATERIAL_ONLY);

    /* --- Deferred path for opaques and decals --- */

    r3d_target_t sceneTarget = R3D_TARGET_SCENE_0;
    r3d_target_t ssaoSource = R3D_TARGET_INVALID;
    r3d_target_t ssilSource = R3D_TARGET_INVALID;
    r3d_target_t ssgiSource = R3D_TARGET_INVALID;
    r3d_target_t ssrSource = R3D_TARGET_INVALID;

    r3d_driver_set_depth_mask(GL_TRUE);
    r3d_driver_set_stencil_mask(0xFF);

    if (r3d_render_has_deferred() || r3d_render_has_prepass()) {
        R3D_TARGET_CLEAR(true, R3D_TARGET_ALL_DEFERRED);

        if (r3d_render_has_deferred()) pass_scene_geometry();
        if (r3d_render_has_prepass()) pass_scene_prepass();
        if (r3d_render_has_decal()) pass_scene_decals();
        if (r3d_light_has_visible()) pass_deferred_lights();

        bool ssao = R3D.environment.ssao.enabled;
        bool ssil = R3D.environment.ssil.enabled;
        bool ssgi = R3D.environment.ssgi.enabled;
        bool ssr = R3D.environment.ssr.enabled;

        if (ssao || ssil || ssgi || ssr) {
            pass_prepare_depth_pyramid();
        }

        if (ssao) ssaoSource = pass_prepare_ssao();
        if (ssil) ssilSource = pass_prepare_ssil();
        if (ssgi) ssgiSource = pass_prepare_ssgi();
        pass_deferred_ambient(ssaoSource, ssilSource, ssgiSource);

        if (ssr) ssrSource = pass_prepare_ssr();
        pass_deferred_compose(sceneTarget, ssrSource);
    }
    else {
        r3d_target_clear(NULL, 0, 0, true);
    }

    /* --- Then background and transparent rendering --- */

    pass_scene_background(sceneTarget);

    if (r3d_render_has_forward() || r3d_render_has_prepass()) {
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

    sceneTarget = pass_post_screen(sceneTarget);
    sceneTarget = pass_post_output(sceneTarget);

    switch (R3D.aaMode) {
    case R3D_ANTI_ALIASING_MODE_FXAA:
        sceneTarget = pass_post_fxaa(sceneTarget);
        break;
    case R3D_ANTI_ALIASING_MODE_SMAA:
        sceneTarget = pass_post_smaa(sceneTarget);
        break;
    default:
        break;
    }

    switch (R3D.outputMode) {
    case R3D_OUTPUT_SCENE: blit_to_screen(r3d_target_swap_scene(sceneTarget)); break;
    case R3D_OUTPUT_ALBEDO: visualize_to_screen(R3D_TARGET_ALBEDO); break;
    case R3D_OUTPUT_NORMAL: visualize_to_screen(R3D_TARGET_NORMAL); break;
    case R3D_OUTPUT_ORM: visualize_to_screen(R3D_TARGET_ORM); break;
    case R3D_OUTPUT_DIFFUSE: visualize_to_screen(R3D_TARGET_DIFFUSE); break;
    case R3D_OUTPUT_SPECULAR: visualize_to_screen(R3D_TARGET_SPECULAR); break;
    case R3D_OUTPUT_SSAO: visualize_to_screen(ssaoSource); break;
    case R3D_OUTPUT_SSIL: visualize_to_screen(ssilSource); break;
    case R3D_OUTPUT_SSGI: visualize_to_screen(ssgiSource); break;
    case R3D_OUTPUT_SSR: visualize_to_screen(ssrSource); break;
    case R3D_OUTPUT_BLOOM: visualize_to_screen(R3D_TARGET_BLOOM); break;
    case R3D_OUTPUT_DOF: visualize_to_screen(R3D_TARGET_DOF_COC); break;
    }

    /* --- Reset states changed by R3D --- */

    cleanup_after_render();
}

void R3D_BeginCluster(BoundingBox aabb)
{
    if (!r3d_render_cluster_begin(aabb)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to begin cluster");
    }
}

void R3D_EndCluster(void)
{
    if (!r3d_render_cluster_end()) {
        R3D_TRACELOG(LOG_WARNING, "Failed to end cluster");
    }
}

void R3D_DrawMesh(R3D_Mesh mesh, R3D_Material material, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_st((Vector3) {scale, scale, scale}, position);
    R3D_DrawMeshPro(mesh, material, transform);
}

void R3D_DrawMeshEx(R3D_Mesh mesh, R3D_Material material, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix transform = r3d_matrix_srt_quat(scale, rotation, position);
    R3D_DrawMeshPro(mesh, material, transform);
}

void R3D_DrawMeshPro(R3D_Mesh mesh, R3D_Material material, Matrix transform)
{
    if (!BIT_TEST_ANY(R3D.layers, mesh.layerMask)) {
        return;
    }

    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.obb = r3d_compute_obb(mesh.aabb, transform);

    r3d_render_group_push(&drawGroup);

    r3d_render_call_t drawCall = {0};
    drawCall.type = R3D_RENDER_CALL_MESH;
    drawCall.mesh.material = material;
    drawCall.mesh.instance = mesh;

    r3d_render_call_push(&drawCall);
}

void R3D_DrawMeshInstanced(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawMeshInstancedPro(mesh, material, instances, 0, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawMeshInstancedEx(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int offset, int count)
{
    R3D_DrawMeshInstancedPro(mesh, material, instances, offset, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawMeshInstancedPro(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int offset, int count, Matrix transform)
{
    if (count <= 0) return;

    if (!BIT_TEST_ANY(R3D.layers, mesh.layerMask)) {
        return;
    }

    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.instances = instances;
    drawGroup.instanceOffset = CLAMP(offset, 0, instances.capacity);
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity - offset);

    r3d_render_group_push(&drawGroup);

    r3d_render_call_t drawCall = {0};
    drawCall.type = R3D_RENDER_CALL_MESH;
    drawCall.mesh.material = material;
    drawCall.mesh.instance = mesh;

    r3d_render_call_push(&drawCall);
}

void R3D_DrawModel(R3D_Model model, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_st((Vector3) {scale, scale, scale}, position);
    R3D_DrawModelPro(model, transform);
}

void R3D_DrawModelEx(R3D_Model model, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix transform = r3d_matrix_srt_quat(scale, rotation, position);
    R3D_DrawModelPro(model, transform);
}

void R3D_DrawModelPro(R3D_Model model, Matrix transform)
{
    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.obb = r3d_compute_obb(model.aabb, transform);
    drawGroup.skinTexture = model.skeleton.skinTexture;

    r3d_render_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!BIT_TEST_ANY(R3D.layers, mesh->layerMask)) {
            continue;
        }

        r3d_render_call_t drawCall = {0};
        drawCall.type = R3D_RENDER_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_render_call_push(&drawCall);
    }
}

void R3D_DrawModelInstanced(R3D_Model model, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawModelInstancedPro(model, instances, 0, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawModelInstancedEx(R3D_Model model, R3D_InstanceBuffer instances, int offset, int count)
{
    R3D_DrawModelInstancedPro(model, instances, offset, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawModelInstancedPro(R3D_Model model, R3D_InstanceBuffer instances, int offset, int count, Matrix transform)
{
    if (count <= 0) return;

    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.skinTexture = model.skeleton.skinTexture;
    drawGroup.instances = instances;
    drawGroup.instanceOffset = CLAMP(offset, 0, instances.capacity);
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity - offset);

    r3d_render_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!BIT_TEST_ANY(R3D.layers, mesh->layerMask)) {
            continue;
        }

        r3d_render_call_t drawCall = {0};
        drawCall.type = R3D_RENDER_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_render_call_push(&drawCall);
    }
}

void R3D_DrawAnimatedModel(R3D_Model model, R3D_AnimationPlayer player, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_st((Vector3) {scale, scale, scale}, position);
    R3D_DrawAnimatedModelPro(model, player, transform);
}

void R3D_DrawAnimatedModelEx(R3D_Model model, R3D_AnimationPlayer player, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix transform = r3d_matrix_srt_quat(scale, rotation, position);
    R3D_DrawAnimatedModelPro(model, player, transform);
}

void R3D_DrawAnimatedModelPro(R3D_Model model, R3D_AnimationPlayer player, Matrix transform)
{
    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.obb = r3d_compute_obb(model.aabb, transform);

    drawGroup.skinTexture = (player.skinTexture > 0)
        ? player.skinTexture : model.skeleton.skinTexture;

    r3d_render_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!BIT_TEST_ANY(R3D.layers, mesh->layerMask)) {
            continue;
        }

        r3d_render_call_t drawCall = {0};
        drawCall.type = R3D_RENDER_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_render_call_push(&drawCall);
    }
}

void R3D_DrawAnimatedModelInstanced(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawAnimatedModelInstancedPro(model, player, instances, 0, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawAnimatedModelInstancedEx(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int offset, int count)
{
    R3D_DrawAnimatedModelInstancedPro(model, player, instances, offset, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawAnimatedModelInstancedPro(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int offset, int count, Matrix transform)
{
    if (count <= 0) return;

    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.instances = instances;
    drawGroup.instanceOffset = CLAMP(offset, 0, instances.capacity);
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity - offset);

    drawGroup.skinTexture = (player.skinTexture > 0)
        ? player.skinTexture : model.skeleton.skinTexture;

    r3d_render_group_push(&drawGroup);

    for (int i = 0; i < model.meshCount; i++)
    {
        const R3D_Mesh* mesh = &model.meshes[i];

        if (!BIT_TEST_ANY(R3D.layers, mesh->layerMask)) {
            continue;
        }

        r3d_render_call_t drawCall = {0};
        drawCall.type = R3D_RENDER_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_render_call_push(&drawCall);
    }
}

void R3D_DrawDecal(R3D_Decal decal, Vector3 position, float scale)
{
    Matrix transform = r3d_matrix_st((Vector3) {scale, scale, scale}, position);
    R3D_DrawDecalPro(decal, transform);
}

void R3D_DrawDecalEx(R3D_Decal decal, Vector3 position, Quaternion rotation, Vector3 scale)
{
    Matrix transform = r3d_matrix_srt_quat(scale, rotation, position);
    R3D_DrawDecalPro(decal, transform);
}

void R3D_DrawDecalPro(R3D_Decal decal, Matrix transform)
{
    decal.normalThreshold = (decal.normalThreshold == 0.0) ? PI * 2 : decal.normalThreshold * DEG2RAD;
    decal.fadeWidth = decal.fadeWidth * DEG2RAD;

    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.obb = r3d_compute_obb(R3D_AABB_UNIT, transform);

    r3d_render_group_push(&drawGroup);

    r3d_render_call_t drawCall = {0};
    drawCall.type = R3D_RENDER_CALL_DECAL;
    drawCall.decal.instance = decal;

    r3d_render_call_push(&drawCall);
}

void R3D_DrawDecalInstanced(R3D_Decal decal, R3D_InstanceBuffer instances, int count)
{
    R3D_DrawDecalInstancedPro(decal, instances, 0, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawDecalInstancedEx(R3D_Decal decal, R3D_InstanceBuffer instances, int offset, int count)
{
    R3D_DrawDecalInstancedPro(decal, instances, offset, count, R3D_MATRIX_IDENTITY);
}

void R3D_DrawDecalInstancedPro(R3D_Decal decal, R3D_InstanceBuffer instances, int offset, int count, Matrix transform)
{
    if (count <= 0) return;

    decal.normalThreshold = (decal.normalThreshold == 0.0) ? PI * 2 : decal.normalThreshold * DEG2RAD;
    decal.fadeWidth = decal.fadeWidth * DEG2RAD;

    r3d_render_group_t drawGroup = {0};
    drawGroup.transform = transform;
    drawGroup.instances = instances;
    drawGroup.instanceOffset = CLAMP(offset, 0, instances.capacity);
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity - offset);

    r3d_render_group_push(&drawGroup);

    r3d_render_call_t drawCall = {0};
    drawCall.type = R3D_RENDER_CALL_DECAL;
    drawCall.decal.instance = decal;

    r3d_render_call_push(&drawCall);
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

void update_view_state(Camera3D camera, double near, double far)
{
    int resW = 1, resH = 1;
    switch (R3D.aspectMode) {
    case R3D_ASPECT_EXPAND:
        if (R3D.screen.id != 0) {
            resW = R3D.screen.texture.width;
            resH = R3D.screen.texture.height;
        }
        else {
            resW = GetRenderWidth();
            resH = GetRenderHeight();
        }
    case R3D_ASPECT_KEEP:
        r3d_target_get_resolution(&resW, &resH, R3D_TARGET_SCENE_0, 0);
        break;
    }

    double aspect = (double)resW / resH;

    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix proj = R3D_MATRIX_IDENTITY;

    if (camera.projection == CAMERA_PERSPECTIVE) {
        proj = MatrixPerspective(camera.fovy * DEG2RAD, aspect, near, far);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double top = camera.fovy / 2.0, right = top * aspect;
        proj = MatrixOrtho(-right, right, -top, top, near, far);
    }

    Matrix viewProj = MatrixMultiply(view, proj);

    R3D.viewState.frustum = r3d_frustum_create(viewProj);
    R3D.viewState.position = camera.position;

    R3D.viewState.view = view;
    R3D.viewState.proj = proj;
    R3D.viewState.invView = MatrixInvert(view);
    R3D.viewState.invProj = MatrixInvert(proj);
    R3D.viewState.viewProj = viewProj;

    R3D.viewState.projMode = camera.projection;
    R3D.viewState.aspect = (float)aspect;
    R3D.viewState.near = (float)near;
    R3D.viewState.far = (float)far;
}

void upload_light_array_block_for_mesh(const r3d_render_call_t* call, bool shadow)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    r3d_shader_block_light_array_t lights = {0};

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        // Check if the geometry "touches" the light area
        // It's not the most accurate possible but hey
        if (light->type != R3D_LIGHT_DIR) {
            if (!CheckCollisionBoxes(light->aabb, call->mesh.instance.aabb)) {
                continue;
            }
        }

        r3d_shader_block_light_t* data = &lights.uLights[lights.uNumLights];
        data->viewProj = MatrixTranspose(light->viewProj[0]);
        data->color = light->color;
        data->position = light->position;
        data->direction = light->direction;
        data->specular = light->specular;
        data->energy = light->energy;
        data->range = light->range;
        data->near = light->near;
        data->far = light->far;
        data->attenuation = light->attenuation;
        data->innerCutOff = light->innerCutOff;
        data->outerCutOff = light->outerCutOff;
        data->shadowSoftness = light->shadowSoftness;
        data->shadowDepthBias = light->shadowDepthBias;
        data->shadowSlopeBias = light->shadowSlopeBias;
        data->shadowLayer = shadow ? light->shadowLayer : -1;
        data->type = light->type;

        if (++lights.uNumLights == R3D_MAX_LIGHT_FORWARD_PER_MESH) {
            break;
        }
    }

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_LIGHT_ARRAY, &lights);
}

void upload_frame_block(void)
{
    static int frameIndex = 0;

    r3d_shader_block_frame_t frame = {
        .screenSize = (Vector2) {(float)R3D_TARGET_WIDTH, (float)R3D_TARGET_HEIGHT},
        .texelSize = (Vector2) {(float)R3D_TARGET_TEXEL_WIDTH, (float)R3D_TARGET_TEXEL_HEIGHT},
        .time = (float)GetTime(),
        .index = frameIndex++,
    };

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_FRAME, &frame);
}

void upload_view_block(void)
{
    r3d_shader_block_view_t view = {
        .position = R3D.viewState.position,
        .view = MatrixTranspose(R3D.viewState.view),
        .invView = MatrixTranspose(R3D.viewState.invView),
        .proj = MatrixTranspose(R3D.viewState.proj),
        .invProj = MatrixTranspose(R3D.viewState.invProj),
        .viewProj = MatrixTranspose(R3D.viewState.viewProj),
        .projMode = R3D.viewState.projMode,
        .aspect = R3D.viewState.aspect,
        .near = R3D.viewState.near,
        .far = R3D.viewState.far,
    };

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_VIEW, &view);
}

void upload_env_block(void)
{
    R3D_EnvBackground* background = &R3D.environment.background;
    R3D_EnvAmbient* ambient = &R3D.environment.ambient;

    r3d_shader_block_env_t env = {0};

    int iProbe = 0;
    R3D_ENV_PROBE_FOR_EACH_VISIBLE(probe) {
        env.uProbes[iProbe] = (struct r3d_shader_block_env_probe) {
            .position = probe->position,
            .falloff = probe->falloff,
            .range = probe->range,
            .irradiance = probe->irradiance,
            .prefilter = probe->prefilter
        };
        if (++iProbe >= R3D_MAX_PROBE_ON_SCREEN) {
            break;
        }
    }

    env.uAmbient.rotation = background->rotation;
    env.uAmbient.color = r3d_color_to_vec4(ambient->color);
    env.uAmbient.energy = ambient->energy;
    env.uAmbient.irradiance = (int)ambient->map.irradiance - 1;
    env.uAmbient.prefilter = (int)ambient->map.prefilter - 1;

    env.uNumPrefilterLevels = r3d_get_mip_levels_1d(R3D_CUBEMAP_PREFILTER_SIZE);
    env.uNumProbes = iProbe;

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_ENV, &env);
}

void raster_depth(const r3d_render_call_t* call, const Matrix* viewProj, r3d_light_t* light)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->mesh.material.shader;
    R3D_SHADER_USE_SELECT(scene.depth, shader);

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4_SELECT(scene.depth, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.depth, shader, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->skinTexture > 0) {
        R3D_SHADER_BIND_SAMPLER_SELECT(scene.depth, shader, uBoneMatricesTex, group->skinTexture);
        R3D_SHADER_SET_INT_SELECT(scene.depth, shader, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.depth, shader, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT_SELECT(scene.depth, shader, uBillboard, material->billboardMode);
    if (material->billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4_SELECT(scene.depth, shader, uMatInvView, R3D.viewState.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.depth, shader, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.depth, shader, uTexCoordScale, material->uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.depth, shader, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_SET_COL4_SELECT(scene.depth, shader, uAlbedoColor, R3D.colorSpace, material->albedo.color);

    if (material->transparencyMode == R3D_TRANSPARENCY_PREPASS) {
        R3D_SHADER_SET_FLOAT_SELECT(scene.depth, shader, uAlphaCutoff, (light != NULL) ? 0.1f : 0.99f);
    }
    else {
        R3D_SHADER_SET_FLOAT_SELECT(scene.depth, shader, uAlphaCutoff, material->alphaCutoff);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (light != NULL) {
        r3d_driver_set_shadow_cast_mode(mesh->shadowCastMode, material->cullMode);
    }
    else {
        r3d_driver_set_depth_state(material->depth);
        r3d_driver_set_stencil_state(material->stencil);
        r3d_driver_set_cull_mode(material->cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.depth, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.depth, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void raster_depth_cube(const r3d_render_call_t* call, const Matrix* viewProj, r3d_light_t* light)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->mesh.material.shader;
    R3D_SHADER_USE_SELECT(scene.depthCube, shader);

    /* --- Set shadow related data --- */

    if (light != NULL) {
        R3D_SHADER_SET_FLOAT_SELECT(scene.depthCube, shader, uFar, light->far);
        R3D_SHADER_SET_VEC3_SELECT(scene.depthCube, shader, uViewPosition, light->position);
    }

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4_SELECT(scene.depthCube, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.depthCube, shader, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->skinTexture > 0) {
        R3D_SHADER_BIND_SAMPLER_SELECT(scene.depthCube, shader, uBoneMatricesTex, group->skinTexture);
        R3D_SHADER_SET_INT_SELECT(scene.depthCube, shader, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.depthCube, shader, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT_SELECT(scene.depthCube, shader, uBillboard, material->billboardMode);
    if (material->billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4_SELECT(scene.depthCube, shader, uMatInvView, R3D.viewState.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.depthCube, shader, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.depthCube, shader, uTexCoordScale, material->uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.depthCube, shader, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_SET_COL4_SELECT(scene.depthCube, shader, uAlbedoColor, R3D.colorSpace, material->albedo.color);

    if (material->transparencyMode == R3D_TRANSPARENCY_PREPASS) {
        R3D_SHADER_SET_FLOAT_SELECT(scene.depthCube, shader, uAlphaCutoff, (light != NULL) ? 0.1f : 0.99f);
    }
    else {
        R3D_SHADER_SET_FLOAT_SELECT(scene.depthCube, shader, uAlphaCutoff, material->alphaCutoff);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (light != NULL) {
        r3d_driver_set_shadow_cast_mode(mesh->shadowCastMode, material->cullMode);
    }
    else {
        r3d_driver_set_depth_state(material->depth);
        r3d_driver_set_stencil_state(material->stencil);
        r3d_driver_set_cull_mode(material->cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.depthCube, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.depthCube, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void raster_probe(const r3d_render_call_t* call, const Matrix* invView, const Matrix* viewProj, r3d_env_probe_t* probe)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->mesh.material.shader;
    R3D_SHADER_USE_SELECT(scene.probe, shader);

    /* --- Set probe related data --- */

    R3D_SHADER_SET_VEC3_SELECT(scene.probe, shader, uViewPosition, probe->position);
    R3D_SHADER_SET_INT_SELECT(scene.probe, shader, uProbeInterior, probe->interior);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4_SELECT(scene.probe, shader, uMatInvView, *invView);
    R3D_SHADER_SET_MAT4_SELECT(scene.probe, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.probe, shader, uMatNormal, matNormal);
    R3D_SHADER_SET_MAT4_SELECT(scene.probe, shader, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->skinTexture > 0) {
        R3D_SHADER_BIND_SAMPLER_SELECT(scene.probe, shader, uBoneMatricesTex, group->skinTexture);
        R3D_SHADER_SET_INT_SELECT(scene.probe, shader, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.probe, shader, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT_SELECT(scene.probe, shader, uBillboard, material->billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.probe, shader, uEmissionEnergy, material->emission.energy);
    R3D_SHADER_SET_FLOAT_SELECT(scene.probe, shader, uNormalScale, material->normal.scale);
    R3D_SHADER_SET_FLOAT_SELECT(scene.probe, shader, uOcclusion, material->orm.occlusion);
    R3D_SHADER_SET_FLOAT_SELECT(scene.probe, shader, uRoughness, material->orm.roughness);
    R3D_SHADER_SET_FLOAT_SELECT(scene.probe, shader, uMetalness, material->orm.metalness);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.probe, shader, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.probe, shader, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4_SELECT(scene.probe, shader, uAlbedoColor, R3D.colorSpace, material->albedo.color);
    R3D_SHADER_SET_COL3_SELECT(scene.probe, shader, uEmissionColor, R3D.colorSpace, material->emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.probe, shader, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.probe, shader, uNormalMap, R3D_TEXTURE_SELECT(material->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.probe, shader, uEmissionMap, R3D_TEXTURE_SELECT(material->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.probe, shader, uOrmMap, R3D_TEXTURE_SELECT(material->orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_driver_set_depth_state(material->depth);
    r3d_driver_set_stencil_state(material->stencil);
    r3d_driver_set_blend_mode(material->blendMode, material->transparencyMode);
    r3d_driver_set_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.probe, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.probe, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void raster_geometry(const r3d_render_call_t* call, bool matchPrepass)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->mesh.material.shader;
    R3D_SHADER_USE_SELECT(scene.geometry, shader);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4_SELECT(scene.geometry, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.geometry, shader, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->skinTexture > 0) {
        R3D_SHADER_BIND_SAMPLER_SELECT(scene.geometry, shader, uBoneMatricesTex, group->skinTexture);
        R3D_SHADER_SET_INT_SELECT(scene.geometry, shader, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.geometry, shader, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT_SELECT(scene.geometry, shader, uBillboard, material->billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.geometry, shader, uEmissionEnergy, material->emission.energy);
    R3D_SHADER_SET_FLOAT_SELECT(scene.geometry, shader, uNormalScale, material->normal.scale);
    R3D_SHADER_SET_FLOAT_SELECT(scene.geometry, shader, uOcclusion, material->orm.occlusion);
    R3D_SHADER_SET_FLOAT_SELECT(scene.geometry, shader, uRoughness, material->orm.roughness);
    R3D_SHADER_SET_FLOAT_SELECT(scene.geometry, shader, uMetalness, material->orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.geometry, shader, uAlphaCutoff, material->alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.geometry, shader, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.geometry, shader, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4_SELECT(scene.geometry, shader, uAlbedoColor, R3D.colorSpace, material->albedo.color);
    R3D_SHADER_SET_COL3_SELECT(scene.geometry, shader, uEmissionColor, R3D.colorSpace, material->emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.geometry, shader, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.geometry, shader, uNormalMap, R3D_TEXTURE_SELECT(material->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.geometry, shader, uEmissionMap, R3D_TEXTURE_SELECT(material->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.geometry, shader, uOrmMap, R3D_TEXTURE_SELECT(material->orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    if (matchPrepass) {
        r3d_driver_set_depth_offset(material->depth.offsetUnits, material->depth.offsetFactor);
        r3d_driver_set_depth_range(material->depth.rangeNear, material->depth.rangeFar);
    }
    else {
        r3d_driver_set_depth_state(material->depth);
        r3d_driver_set_stencil_state(material->stencil);
    }

    r3d_driver_set_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.geometry, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.geometry, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void raster_decal(const r3d_render_call_t* call)
{
    assert(call->type == R3D_RENDER_CALL_DECAL); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Decal* decal = &call->decal.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->decal.instance.shader;
    R3D_SHADER_USE_SELECT(scene.decal, shader);

    /* --- Bind global textures --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.decal, shader, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 0));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.decal, shader, uGeomNormalTex, r3d_target_get(R3D_TARGET_GEOM_NORMAL));

    /* --- Set additional matrix uniforms --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4_SELECT(scene.decal, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.decal, shader, uMatNormal, matNormal);

    /* --- Skinning is never used for decals --- */

    R3D_SHADER_SET_INT_SELECT(scene.decal, shader, uSkinning, false);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uEmissionEnergy, decal->emission.energy);
    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uNormalScale, decal->normal.scale);
    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uOcclusion, decal->orm.occlusion);
    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uRoughness, decal->orm.roughness);
    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uMetalness, decal->orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uAlphaCutoff, decal->alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.decal, shader, uTexCoordOffset, decal->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.decal, shader, uTexCoordScale, decal->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4_SELECT(scene.decal, shader, uAlbedoColor, R3D.colorSpace, decal->albedo.color);
    R3D_SHADER_SET_COL3_SELECT(scene.decal, shader, uEmissionColor, R3D.colorSpace, decal->emission.color);

    /* --- Set decal specific values --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uNormalThreshold, decal->normalThreshold);
    R3D_SHADER_SET_FLOAT_SELECT(scene.decal, shader, uFadeWidth, decal->fadeWidth);
    R3D_SHADER_SET_INT_SELECT(scene.decal, shader, uApplyColor, decal->applyColor && (decal->albedo.texture.id != 0));

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.decal, shader, uAlbedoMap, R3D_TEXTURE_SELECT(decal->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.decal, shader, uNormalMap, R3D_TEXTURE_SELECT(decal->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.decal, shader, uEmissionMap, R3D_TEXTURE_SELECT(decal->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.decal, shader, uOrmMap, R3D_TEXTURE_SELECT(decal->orm.texture.id, WHITE));

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.decal, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.decal, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void raster_forward(const r3d_render_call_t* call)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->mesh.material.shader;
    R3D_SHADER_USE_SELECT(scene.forward, shader);

    /* --- Set view related data --- */

    // NOTE: We don't use the UBO view position because this shader is reused by probes with their own view position
    R3D_SHADER_SET_VEC3_SELECT(scene.forward, shader, uViewPosition, R3D.viewState.position);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4_SELECT(scene.forward, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.forward, shader, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->skinTexture > 0) {
        R3D_SHADER_BIND_SAMPLER_SELECT(scene.forward, shader, uBoneMatricesTex, group->skinTexture);
        R3D_SHADER_SET_INT_SELECT(scene.forward, shader, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.forward, shader, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT_SELECT(scene.forward, shader, uBillboard, material->billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.forward, shader, uEmissionEnergy, material->emission.energy);
    R3D_SHADER_SET_FLOAT_SELECT(scene.forward, shader, uNormalScale, material->normal.scale);
    R3D_SHADER_SET_FLOAT_SELECT(scene.forward, shader, uOcclusion, material->orm.occlusion);
    R3D_SHADER_SET_FLOAT_SELECT(scene.forward, shader, uRoughness, material->orm.roughness);
    R3D_SHADER_SET_FLOAT_SELECT(scene.forward, shader, uMetalness, material->orm.metalness);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.forward, shader, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.forward, shader, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4_SELECT(scene.forward, shader, uAlbedoColor, R3D.colorSpace, material->albedo.color);
    R3D_SHADER_SET_COL3_SELECT(scene.forward, shader, uEmissionColor, R3D.colorSpace, material->emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.forward, shader, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.forward, shader, uNormalMap, R3D_TEXTURE_SELECT(material->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.forward, shader, uEmissionMap, R3D_TEXTURE_SELECT(material->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER_SELECT(scene.forward, shader, uOrmMap, R3D_TEXTURE_SELECT(material->orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_driver_set_depth_state(material->depth);
    r3d_driver_set_stencil_state(material->stencil);
    r3d_driver_set_blend_mode(material->blendMode, material->transparencyMode);
    r3d_driver_set_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.forward, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.forward, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void raster_unlit(const r3d_render_call_t* call, const Matrix* invView, const Matrix* viewProj)
{
    assert(call->type == R3D_RENDER_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_render_group_t* group = r3d_render_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Use shader --- */

    R3D_SurfaceShader* shader = call->mesh.material.shader;
    R3D_SHADER_USE_SELECT(scene.unlit, shader);

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4_SELECT(scene.unlit, shader, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4_SELECT(scene.unlit, shader, uMatNormal, matNormal);

    R3D_SHADER_SET_MAT4_SELECT(scene.unlit, shader, uMatInvView, *invView);
    R3D_SHADER_SET_MAT4_SELECT(scene.unlit, shader, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->skinTexture > 0) {
        R3D_SHADER_BIND_SAMPLER_SELECT(scene.unlit, shader, uBoneMatricesTex, group->skinTexture);
        R3D_SHADER_SET_INT_SELECT(scene.unlit, shader, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.unlit, shader, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT_SELECT(scene.unlit, shader, uBillboard, material->billboardMode);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT_SELECT(scene.unlit, shader, uAlphaCutoff, material->alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2_SELECT(scene.unlit, shader, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2_SELECT(scene.unlit, shader, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4_SELECT(scene.unlit, shader, uAlbedoColor, R3D.colorSpace, material->albedo.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER_SELECT(scene.unlit, shader, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_driver_set_depth_state(material->depth);
    r3d_driver_set_stencil_state(material->stencil);
    r3d_driver_set_blend_mode(material->blendMode, material->transparencyMode);
    r3d_driver_set_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_render_has_instances(group)) {
        R3D_SHADER_SET_INT_SELECT(scene.unlit, shader, uInstancing, true);
        r3d_render_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT_SELECT(scene.unlit, shader, uInstancing, false);
        r3d_render_draw(call);
    }
}

void pass_scene_shadow(void)
{
    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_enable(GL_DEPTH_TEST);

    r3d_driver_set_depth_func(GL_LEQUAL);
    r3d_driver_set_depth_mask(GL_TRUE);

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        if (!r3d_light_shadow_should_be_updated(light, true)) {
            continue;
        }

        if (light->type == R3D_LIGHT_OMNI) {
            for (int iFace = 0; iFace < 6; iFace++) {
                r3d_light_shadow_bind_fbo(light->type, light->shadowLayer, iFace);
                glClear(GL_DEPTH_BUFFER_BIT);

                const r3d_frustum_t* frustum = &light->frustum[iFace];
                r3d_render_cull_groups(frustum);

                #define COND (call->mesh.instance.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
                R3D_RENDER_FOR_EACH(call, COND, frustum, R3D_RENDER_PACKLIST_SHADOW) {
                    if (r3d_render_should_cast_shadow(call)) {
                        raster_depth_cube(call, &light->viewProj[iFace], light);
                    }
                }
                #undef COND
            }
        }
        else {
            r3d_light_shadow_bind_fbo(light->type, light->shadowLayer, 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            const r3d_frustum_t* frustum = &light->frustum[0];
            r3d_render_cull_groups(frustum);

            #define COND (call->mesh.instance.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
            R3D_RENDER_FOR_EACH(call, COND, frustum, R3D_RENDER_PACKLIST_SHADOW) {
                if (r3d_render_should_cast_shadow(call)) {
                    raster_depth(call, &light->viewProj[0], light);
                }
            }
            #undef COND
        }
    }
}

void pass_scene_probes(void)
{
    R3D_ENV_PROBE_FOR_EACH_VISIBLE(probe)
    {
        if (!r3d_env_probe_should_be_updated(probe, true)) {
            continue;
        }

        for (int iFace = 0; iFace < 6; iFace++)
        {
            /* --- Generates the list of visible groups for the current face of the capture --- */

            const r3d_frustum_t* frustum = &probe->frustum[iFace];
            r3d_render_cull_groups(frustum);

            /* --- Render scene --- */

            r3d_driver_enable(GL_STENCIL_TEST);
            r3d_driver_enable(GL_DEPTH_TEST);
            r3d_driver_enable(GL_BLEND);

            r3d_driver_set_depth_mask(GL_TRUE);

            r3d_env_capture_bind_fbo(iFace, 0);
            glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_PACKLIST_PROBE) {
                if (call->mesh.material.unlit) {
                    raster_unlit(call, &probe->invView[iFace], &probe->viewProj[iFace]);
                }
                else {
                    upload_light_array_block_for_mesh(call, probe->shadows);
                    raster_probe(call, &probe->invView[iFace], &probe->viewProj[iFace], probe);
                }
            }

            r3d_driver_set_depth_offset(0.0f, 0.0f);
            r3d_driver_set_depth_range(0.0f, 1.0f);

            /* --- Render background --- */

            r3d_driver_disable(GL_STENCIL_TEST);
            r3d_driver_disable(GL_CULL_FACE);
            r3d_driver_disable(GL_BLEND);

            r3d_driver_set_depth_func(GL_LEQUAL);
            r3d_driver_set_depth_mask(GL_FALSE);

            if (!probe->interior && R3D.environment.background.sky.texture != 0) {
                R3D_SHADER_USE(scene.skybox);
                float lod = (float)r3d_get_mip_levels_1d(R3D.environment.background.sky.size);
                R3D_SHADER_BIND_SAMPLER(scene.skybox, uSkyMap, R3D.environment.background.sky.texture);
                R3D_SHADER_SET_FLOAT(scene.skybox, uEnergy, R3D.environment.background.energy);
                R3D_SHADER_SET_FLOAT(scene.skybox, uLod, R3D.environment.background.skyBlur * lod);
                R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.environment.background.rotation);
                R3D_SHADER_SET_MAT4(scene.skybox, uMatInvView, probe->invView[iFace]);
                R3D_SHADER_SET_MAT4(scene.skybox, uMatInvProj, probe->invProj);
            }
            else {
                Vector4 background = r3d_color_to_linear_scaled_vec4(
                    R3D.environment.background.color, R3D.colorSpace,
                    R3D.environment.background.energy
                );
                R3D_SHADER_USE(scene.background);
                R3D_SHADER_SET_VEC4(scene.background, uColor, background);
            }

            R3D_RENDER_SCREEN();
        }

        /* --- Generate irradiance and prefilter maps --- */

        r3d_env_capture_gen_mipmaps();

        if (probe->irradiance >= 0) {
            r3d_pass_prepare_irradiance(probe->irradiance, r3d_env_capture_get(), R3D_PROBE_CAPTURE_SIZE);
        }

        if (probe->prefilter >= 0) {
            r3d_pass_prepare_prefilter(probe->prefilter, r3d_env_capture_get(), R3D_PROBE_CAPTURE_SIZE);
        }

        r3d_target_invalidate_cache(); //< The IBL gen functions bind framebuffers; resetting them prevents any problems
    }
}

void pass_scene_geometry(void)
{
    R3D_TARGET_BIND(true, R3D_TARGET_GBUFFER);

    r3d_driver_enable(GL_STENCIL_TEST);
    r3d_driver_enable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_BLEND);

    r3d_driver_set_depth_mask(GL_TRUE);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_LIST_OPAQUE_INST, R3D_RENDER_LIST_OPAQUE) {
        if (!call->mesh.material.unlit) {
            raster_geometry(call, false);
        }
    }

    r3d_driver_set_depth_offset(0.0f, 0.0f);
    r3d_driver_set_depth_range(0.0f, 1.0f);
}

void pass_scene_prepass(void)
{
    /* --- First render only depth --- */

    r3d_target_bind(NULL, 0, 0, true);

    r3d_driver_enable(GL_STENCIL_TEST);
    r3d_driver_enable(GL_DEPTH_TEST);

    r3d_driver_set_depth_mask(GL_TRUE);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_LIST_TRANSPARENT_INST, R3D_RENDER_LIST_TRANSPARENT) {
        if (r3d_render_is_prepass(call)) {
            raster_depth(call, &R3D.viewState.viewProj, NULL);
        }
    }

    /* --- Render opaque only with GL_EQUAL --- */

    // NOTE: The transparent part will be rendered in forward
    R3D_TARGET_BIND(true, R3D_TARGET_GBUFFER);

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_BLEND);

    r3d_driver_set_depth_func(GL_EQUAL);
    r3d_driver_set_depth_mask(GL_FALSE);

    R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_LIST_TRANSPARENT_INST, R3D_RENDER_LIST_TRANSPARENT) {
        if (r3d_render_is_prepass(call)) {
            raster_geometry(call, true);
        }
    }

    /* --- Reset undesired states --- */

    r3d_driver_set_depth_offset(0.0f, 0.0f);
    r3d_driver_set_depth_range(0.0f, 1.0f);
}

void pass_scene_decals(void)
{
    R3D_TARGET_BIND(false, R3D_TARGET_DECAL);

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_enable(GL_CULL_FACE);
    r3d_driver_enable(GL_BLEND);

    r3d_driver_set_cull_face(GL_FRONT); // Only render back faces to avoid clipping issues

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_LIST_DECAL_INST, R3D_RENDER_LIST_DECAL) {
        raster_decal(call);
    }
}

void pass_prepare_depth_pyramid(void)
{
    int numLevels = r3d_target_get_num_levels(R3D_TARGET_DEPTH);

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    R3D_SHADER_USE(prepare.depthPyramid);

    for (int iDst = 1; iDst < numLevels; iDst++) {
        R3D_TARGET_BIND_LEVELS(R3D_TARGET_LEVEL_LIST(iDst, iDst-1), R3D_TARGET_DEPTH, R3D_TARGET_SELECTOR);
        R3D_SHADER_BIND_SAMPLER(prepare.depthPyramid, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, iDst - 1));
        R3D_RENDER_SCREEN();
    }
}

r3d_target_t pass_prepare_ssao(void)
{
    /* --- Setup OpenGL pipeline --- */

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    /* --- Downsample G-Buffer --- */

    R3D_TARGET_BIND_LEVEL(1, R3D_TARGET_NORMAL);
    R3D_SHADER_USE(prepare.ssaoInDown);

    R3D_SHADER_BIND_SAMPLER(prepare.ssaoInDown, uSelectorTex, r3d_target_get_level(R3D_TARGET_SELECTOR, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssaoInDown, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 0));

    R3D_RENDER_SCREEN();

    /* --- Calculate SSAO --- */

    R3D_TARGET_BIND(false, R3D_TARGET_SSAO_0);
    R3D_SHADER_USE(prepare.ssao);

    R3D_SHADER_SET_INT(prepare.ssao, uSampleCount, R3D.environment.ssao.sampleCount);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uRadius,  R3D.environment.ssao.radius);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uBias, R3D.environment.ssao.bias);
    R3D_SHADER_SET_FLOAT(prepare.ssao, uIntensity, R3D.environment.ssao.intensity);

    R3D_SHADER_BIND_SAMPLER(prepare.ssao, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssao, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));

    R3D_RENDER_SCREEN();

    /* --- Blur SSAO --- */

    R3D_SHADER_USE(prepare.ssaoBlur);
    R3D_SHADER_BIND_SAMPLER(prepare.ssaoBlur, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));

    R3D_TARGET_BIND(false, R3D_TARGET_SSAO_1);
    R3D_SHADER_SET_VEC2(prepare.ssaoBlur, uDirection, (Vector2){1,0});
    R3D_SHADER_BIND_SAMPLER(prepare.ssaoBlur, uSsaoTex, r3d_target_get(R3D_TARGET_SSAO_0));
    R3D_RENDER_SCREEN();

    R3D_TARGET_BIND(false, R3D_TARGET_SSAO_0);
    R3D_SHADER_SET_VEC2(prepare.ssaoBlur, uDirection, (Vector2){0,1});
    R3D_SHADER_BIND_SAMPLER(prepare.ssaoBlur, uSsaoTex, r3d_target_get(R3D_TARGET_SSAO_1));
    R3D_RENDER_SCREEN();

    return R3D_TARGET_SSAO_0;
}

r3d_target_t pass_prepare_ssil(void)
{
    /* --- Check if we need history --- */

    static r3d_target_t SSIL_HISTORY  = R3D_TARGET_SSIL_0;
    static r3d_target_t SSIL_RAW      = R3D_TARGET_SSIL_1;
    static r3d_target_t SSIL_FILTERED = R3D_TARGET_SSIL_2;

    if (r3d_target_get_or_null(SSIL_HISTORY) == 0) {
        R3D_TARGET_CLEAR(false, SSIL_HISTORY);
    }

    /* --- Setup OpenGL pipeline --- */

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    /* --- Downsample G-Buffer --- */

    R3D_TARGET_BIND_LEVEL(1, R3D_TARGET_DIFFUSE, R3D_TARGET_NORMAL, R3D_TARGET_DEPTH);
    R3D_SHADER_USE(prepare.ssilInDown);

    R3D_SHADER_BIND_SAMPLER(prepare.ssilInDown, uSelectorTex, r3d_target_get_level(R3D_TARGET_SELECTOR, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssilInDown, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssilInDown, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 0));

    R3D_RENDER_SCREEN();

    /* --- Calculate SSIL (RAW) --- */

    R3D_TARGET_BIND(false, SSIL_RAW);
    R3D_SHADER_USE(prepare.ssil);

    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uHistoryTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(SSIL_HISTORY), BLACK));
    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));

    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleCount, (float)R3D.environment.ssil.sampleCount);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSliceCount, (float)R3D.environment.ssil.sliceCount);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uRadius, R3D.environment.ssil.radius);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uThickness, R3D.environment.ssil.thickness);

    R3D_RENDER_SCREEN();

    /* --- Atrous denoise: RAW -> FILTERED --- */

    R3D_SHADER_USE(prepare.atrousWavelet);

    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));

    R3D_SHADER_SET_FLOAT(prepare.atrousWavelet, uInvNormalSharp, 12.5f);    // 1.0f / 0.08f
    R3D_SHADER_SET_FLOAT(prepare.atrousWavelet, uInvDepthSharp, 25.0f);     // 1.0f / 0.04f

    r3d_target_t* src = &SSIL_RAW;
    r3d_target_t* dst = &SSIL_FILTERED;

    int steps = R3D.environment.ssil.denoiseSteps;

    for (int i = 0; i < steps; i++)
    {
        int stepWidth = 1 << ((steps - 1) - i);
        float invStepWidth2 = 1.0f / (stepWidth*stepWidth);

        R3D_TARGET_BIND(false, *dst);
        R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uSourceTex, r3d_target_get(*src));
        R3D_SHADER_SET_FLOAT(prepare.atrousWavelet, uInvStepWidth2, invStepWidth2);
        R3D_SHADER_SET_INT(prepare.atrousWavelet, uStepWidth, stepWidth);
        R3D_RENDER_SCREEN();

        SWAP(r3d_target_t, *src, *dst);
    }

    SWAP(r3d_target_t, SSIL_HISTORY, *src);

    return SSIL_HISTORY;
}

r3d_target_t pass_prepare_ssgi(void)
{
    /* --- Check if we need history --- */

    static r3d_target_t SSGI_HISTORY  = R3D_TARGET_SSGI_0;
    static r3d_target_t SSGI_RAW      = R3D_TARGET_SSGI_1;
    static r3d_target_t SSGI_FILTERED = R3D_TARGET_SSGI_2;

    if (r3d_target_get_or_null(SSGI_HISTORY) == 0) {
        R3D_TARGET_CLEAR(false, SSGI_HISTORY);
    }

    /* --- Setup OpenGL pipeline --- */

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    /* --- Downsample G-Buffer --- */

    R3D_TARGET_BIND_LEVEL(1, R3D_TARGET_DIFFUSE, R3D_TARGET_NORMAL, R3D_TARGET_DEPTH);
    R3D_SHADER_USE(prepare.ssgiInDown);

    R3D_SHADER_BIND_SAMPLER(prepare.ssilInDown, uSelectorTex, r3d_target_get_level(R3D_TARGET_SELECTOR, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssgiInDown, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssgiInDown, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 0));

    R3D_RENDER_SCREEN();

    /* --- Calculate SSGI (RAW) --- */

    R3D_TARGET_BIND_LEVEL(0, SSGI_RAW);
    R3D_SHADER_USE(prepare.ssgi);

    R3D_SHADER_BIND_SAMPLER(prepare.ssgi, uHistoryTex, r3d_target_get(SSGI_HISTORY));
    R3D_SHADER_BIND_SAMPLER(prepare.ssgi, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssgi, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssgi, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssgi, uLutTex, r3d_texture_get(R3D_TEXTURE_SSGI_LUT));

    R3D_SHADER_SET_INT(prepare.ssgi, uSampleCount, R3D.environment.ssgi.sampleCount);
    R3D_SHADER_SET_INT(prepare.ssgi, uMaxRaySteps, R3D.environment.ssgi.maxRaySteps);
    R3D_SHADER_SET_FLOAT(prepare.ssgi, uStepSize, R3D.environment.ssgi.stepSize);
    R3D_SHADER_SET_FLOAT(prepare.ssgi, uThickness, R3D.environment.ssgi.thickness);
    R3D_SHADER_SET_FLOAT(prepare.ssgi, uMaxDistance, R3D.environment.ssgi.maxDistance);
    R3D_SHADER_SET_FLOAT(prepare.ssgi, uFadeStart, R3D.environment.ssgi.fadeStart);
    R3D_SHADER_SET_FLOAT(prepare.ssgi, uFadeEnd, R3D.environment.ssgi.fadeEnd);

    R3D_RENDER_SCREEN();

    /* --- Atrous denoise: RAW -> FILTERED --- */

    R3D_SHADER_USE(prepare.atrousWavelet);

    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));

    R3D_SHADER_SET_FLOAT(prepare.atrousWavelet, uInvNormalSharp, 1.3333333333333333f);  // 1.0f / 0.75f
    R3D_SHADER_SET_FLOAT(prepare.atrousWavelet, uInvDepthSharp, 0.5714285714285714f);   // 1.0f / 1.75f

    r3d_target_t* src = &SSGI_RAW;
    r3d_target_t* dst = &SSGI_FILTERED;

    int steps = R3D.environment.ssgi.denoiseSteps;

    for (int i = 0; i < steps; i++)
    {
        int stepWidth = 1 << ((steps - 1) - i);
        float invStepWidth2 = 1.0f / (stepWidth*stepWidth);

        R3D_TARGET_BIND(false, *dst);
        R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uSourceTex, r3d_target_get(*src));
        R3D_SHADER_SET_FLOAT(prepare.atrousWavelet, uInvStepWidth2, invStepWidth2);
        R3D_SHADER_SET_INT(prepare.atrousWavelet, uStepWidth, stepWidth);
        R3D_RENDER_SCREEN();

        SWAP(r3d_target_t, *src, *dst);
    }

    SWAP(r3d_target_t, SSGI_HISTORY, *src);

    return SSGI_HISTORY;
}

r3d_target_t pass_prepare_ssr(void)
{
    /* --- Setup OpenGL pipeline --- */

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    /* --- Downsample G-Buffer --- */

    R3D_TARGET_BIND_LEVEL(1, R3D_TARGET_DIFFUSE, R3D_TARGET_SPECULAR, R3D_TARGET_NORMAL, R3D_TARGET_DEPTH);
    R3D_SHADER_USE(prepare.ssrInDown);

    R3D_SHADER_BIND_SAMPLER(prepare.ssrInDown, uSelectorTex, r3d_target_get_level(R3D_TARGET_SELECTOR, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssrInDown, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssrInDown, uSpecularTex, r3d_target_get_level(R3D_TARGET_SPECULAR, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.ssrInDown, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 0));

    R3D_RENDER_SCREEN();

    /* --- Calculate SSR --- */

    R3D_TARGET_BIND_LEVEL(0, R3D_TARGET_SSR);
    R3D_SHADER_USE(prepare.ssr);

    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uSpecularTex, r3d_target_get_level(R3D_TARGET_SPECULAR, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));

    R3D_SHADER_SET_INT(prepare.ssr, uMaxRaySteps, R3D.environment.ssr.maxRaySteps);
    R3D_SHADER_SET_INT(prepare.ssr, uBinarySteps, R3D.environment.ssr.binarySteps);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uStepSize, R3D.environment.ssr.stepSize);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uThickness, R3D.environment.ssr.thickness);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uMaxDistance, R3D.environment.ssr.maxDistance);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFade, R3D.environment.ssr.edgeFade);

    R3D_RENDER_SCREEN();

    /* --- Downsample SSR --- */

    int numLevels = r3d_target_get_num_levels(R3D_TARGET_SSR);
    r3d_target_set_read_levels(R3D_TARGET_SSR, 0, numLevels - 1);

    R3D_SHADER_USE(prepare.blurDown);
    R3D_SHADER_BIND_SAMPLER(prepare.blurDown, uSourceTex, r3d_target_get(R3D_TARGET_SSR));

    for (int iDst = 1; iDst < numLevels; iDst++) {
        r3d_target_set_write_level(0, iDst);
        r3d_target_set_viewport(R3D_TARGET_SSR, iDst);
        R3D_SHADER_SET_INT(prepare.blurDown, uSourceLod, iDst - 1);
        R3D_RENDER_SCREEN();
    }

    /* --- Upsample only once for each level below zero --- */

    R3D_SHADER_USE(prepare.blurUp);
    R3D_SHADER_BIND_SAMPLER(prepare.blurUp, uSourceTex, r3d_target_get(R3D_TARGET_SSR));

    for (int iDst = 1; iDst < numLevels - 1; iDst++) {
        r3d_target_set_write_level(0, iDst);
        r3d_target_set_viewport(R3D_TARGET_SSR, iDst);
        R3D_SHADER_SET_INT(prepare.blurUp, uSourceLod, iDst + 1);
        R3D_RENDER_SCREEN();
    }

    return R3D_TARGET_SSR;
}

void pass_deferred_lights(void)
{
    /* --- Setup OpenGL pipeline --- */

    R3D_TARGET_BIND(true, R3D_TARGET_LIGHTING);

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_CULL_FACE);

    r3d_driver_enable(GL_SCISSOR_TEST);
    r3d_driver_enable(GL_DEPTH_TEST);
    r3d_driver_enable(GL_BLEND);

    // Set additive blending to accumulate light contributions
    r3d_driver_set_blend_func(GL_FUNC_ADD, GL_ONE, GL_ONE);
    r3d_driver_set_depth_func(GL_GREATER);
    r3d_driver_set_depth_mask(GL_FALSE);

    /* --- Enable shader and setup constant stuff --- */

    R3D_SHADER_USE(deferred.lighting);

    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uAlbedoTex, r3d_target_get_level(R3D_TARGET_ALBEDO, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uOrmTex, r3d_target_get_level(R3D_TARGET_ORM, 0));

    /* --- Calculate lighting contributions --- */

    R3D_LIGHT_FOR_EACH_VISIBLE(light)
    {
        // Set scissors rect
        r3d_rect_t dst = {0, 0, R3D_TARGET_WIDTH, R3D_TARGET_HEIGHT};
        if (light->type != R3D_LIGHT_DIR) {
            dst = r3d_light_get_screen_rect(light, &R3D.viewState.viewProj, dst.w, dst.h);
            if (memcmp(&dst, &(r3d_rect_t){0}, sizeof(r3d_rect_t)) == 0) continue;
        }
        glScissor(dst.x, dst.y, dst.w, dst.h);

        // Send light data to the GPU
        r3d_shader_block_light_t data = {
            .viewProj = MatrixTranspose(light->viewProj[0]),
            .color = light->color,
            .position = light->position,
            .direction = light->direction,
            .specular = light->specular,
            .energy = light->energy,
            .range = light->range,
            .near = light->near,
            .far = light->far,
            .attenuation = light->attenuation,
            .innerCutOff = light->innerCutOff,
            .outerCutOff = light->outerCutOff,
            .shadowSoftness = light->shadowSoftness,
            .shadowDepthBias = light->shadowDepthBias,
            .shadowSlopeBias = light->shadowSlopeBias,
            .shadowLayer = light->shadowLayer,
            .type = light->type,
        };
        r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_LIGHT, &data);

        // Accumulate this light!
        R3D_RENDER_SCREEN();
    }

    /* --- Reset undesired states --- */

    r3d_driver_disable(GL_SCISSOR_TEST);
}

void pass_deferred_ambient(r3d_target_t ssaoSource, r3d_target_t ssilSource, r3d_target_t ssgiSource)
{
    /* --- Setup OpenGL pipeline --- */

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_CULL_FACE);

    r3d_driver_enable(GL_DEPTH_TEST);
    r3d_driver_enable(GL_BLEND);

    // Set additive blending to accumulate light contributions
    r3d_driver_set_blend_func(GL_FUNC_ADD, GL_ONE, GL_ONE);
    r3d_driver_set_depth_func(GL_GREATER);
    r3d_driver_set_depth_mask(GL_FALSE);

    /* --- Calculation and composition of ambient/indirect lighting --- */

    R3D_TARGET_BIND(true, R3D_TARGET_LIGHTING);
    R3D_SHADER_USE(deferred.ambient);

    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uAlbedoTex, r3d_target_get_level(R3D_TARGET_ALBEDO, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uNormalTex, r3d_target_get_level(R3D_TARGET_NORMAL, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uSsaoTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uSsilTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssilSource), BLACK));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uSsgiTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssgiSource), BLACK));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uOrmTex, r3d_target_get_level(R3D_TARGET_ORM, 0));

    R3D_SHADER_SET_FLOAT(deferred.ambient, uSsaoPower, R3D.environment.ssao.power);
    R3D_SHADER_SET_FLOAT(deferred.ambient, uSsilIntensity, R3D.environment.ssil.intensity);
    R3D_SHADER_SET_FLOAT(deferred.ambient, uSsilAoPower, R3D.environment.ssil.aoPower);

    R3D_RENDER_SCREEN();
}

void pass_deferred_compose(r3d_target_t sceneTarget, r3d_target_t ssrSource)
{
    R3D_TARGET_BIND(true, sceneTarget);

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    r3d_driver_enable(GL_DEPTH_TEST);
    r3d_driver_set_depth_func(GL_GREATER);
    r3d_driver_set_depth_mask(GL_FALSE);

    R3D_SHADER_USE(deferred.compose);

    R3D_SHADER_BIND_SAMPLER(deferred.compose, uAlbedoTex, r3d_target_get_level(R3D_TARGET_ALBEDO, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.compose, uDiffuseTex, r3d_target_get_level(R3D_TARGET_DIFFUSE, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.compose, uSpecularTex, r3d_target_get_level(R3D_TARGET_SPECULAR, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.compose, uOrmTex, r3d_target_get_level(R3D_TARGET_ORM, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.compose, uSsrTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssrSource), BLANK));

    R3D_SHADER_SET_FLOAT(deferred.compose, uSsrNumLevels, (float)r3d_target_get_num_levels(R3D_TARGET_SSR));

    R3D_RENDER_SCREEN();
}

void pass_scene_forward(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(true, sceneTarget);

    r3d_driver_enable(GL_STENCIL_TEST);
    r3d_driver_enable(GL_DEPTH_TEST);
    r3d_driver_enable(GL_BLEND);

    /* --- Render unlit opaque --- */

    r3d_driver_set_depth_mask(GL_TRUE);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_LIST_OPAQUE_INST, R3D_RENDER_LIST_OPAQUE) {
        if (call->mesh.material.unlit) {
            raster_unlit(call, &R3D.viewState.invView, &R3D.viewState.viewProj);
        }
    }

    /* --- Render all transparent in order - (prepass/alpha treated as same) --- */

    r3d_driver_set_depth_mask(GL_FALSE);

    R3D_RENDER_FOR_EACH(call, true, frustum, R3D_RENDER_LIST_TRANSPARENT_INST, R3D_RENDER_LIST_TRANSPARENT) {
        if (call->mesh.material.unlit) {
            raster_unlit(call, &R3D.viewState.invView, &R3D.viewState.viewProj);
        }
        else {
            upload_light_array_block_for_mesh(call, true);
            raster_forward(call);
        }
    }

    /* --- Reset undesired states --- */

    r3d_driver_set_depth_offset(0.0f, 0.0f);
    r3d_driver_set_depth_range(0.0f, 1.0f);
}

void pass_scene_background(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(true, sceneTarget);

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    r3d_driver_enable(GL_DEPTH_TEST);
    r3d_driver_set_depth_func(GL_LEQUAL);
    r3d_driver_set_depth_mask(GL_FALSE);

    if (R3D.environment.background.sky.texture != 0) {
        R3D_SHADER_USE(scene.skybox);
        float lod = (float)r3d_get_mip_levels_1d(R3D.environment.background.sky.size);
        R3D_SHADER_BIND_SAMPLER(scene.skybox, uSkyMap, R3D.environment.background.sky.texture);
        R3D_SHADER_SET_FLOAT(scene.skybox, uEnergy, R3D.environment.background.energy);
        R3D_SHADER_SET_FLOAT(scene.skybox, uLod, R3D.environment.background.skyBlur * lod);
        R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.environment.background.rotation);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatInvView, R3D.viewState.invView);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatInvProj, R3D.viewState.invProj);
    }
    else {
        Vector4 background = r3d_color_to_linear_scaled_vec4(
            R3D.environment.background.color, R3D.colorSpace,
            R3D.environment.background.energy
        );
        R3D_SHADER_USE(scene.background);
        R3D_SHADER_SET_VEC4(scene.background, uColor, background);
    }

    R3D_RENDER_SCREEN();
}

r3d_target_t pass_post_setup(r3d_target_t sceneTarget)
{
    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);
    r3d_driver_disable(GL_BLEND);

    return r3d_target_swap_scene(sceneTarget);
}

r3d_target_t pass_post_fog(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fog);

    R3D_SHADER_BIND_SAMPLER(post.fog, uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER(post.fog, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 0));

    R3D_SHADER_SET_INT(post.fog, uFogMode, R3D.environment.fog.mode);
    R3D_SHADER_SET_COL3(post.fog, uFogColor, R3D.colorSpace, R3D.environment.fog.color);
    R3D_SHADER_SET_FLOAT(post.fog, uFogStart, R3D.environment.fog.start);
    R3D_SHADER_SET_FLOAT(post.fog, uFogEnd, R3D.environment.fog.end);
    R3D_SHADER_SET_FLOAT(post.fog, uFogDensity, R3D.environment.fog.density);
    R3D_SHADER_SET_FLOAT(post.fog, uSkyAffect, R3D.environment.fog.skyAffect);

    R3D_RENDER_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_dof(r3d_target_t sceneTarget)	
{
    /* --- Calculate CoC --- */

    R3D_TARGET_BIND_LEVEL(0, R3D_TARGET_DOF_COC);
    R3D_SHADER_USE(prepare.dofCoc);

    R3D_SHADER_BIND_SAMPLER(prepare.dofCoc, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 0));
    R3D_SHADER_SET_FLOAT(prepare.dofCoc, uFocusPoint, R3D.environment.dof.focusPoint);
    R3D_SHADER_SET_FLOAT(prepare.dofCoc, uFocusScale, R3D.environment.dof.focusScale);

    R3D_RENDER_SCREEN();

    /* --- Downsample CoC to half resolution --- */

    R3D_TARGET_BIND(false, R3D_TARGET_DOF_0, R3D_TARGET_DEPTH);
    r3d_target_set_write_level(1, 1);

    R3D_SHADER_USE(prepare.dofDown);
    R3D_SHADER_BIND_SAMPLER(prepare.dofDown, uSceneTex, r3d_target_get(r3d_target_swap_scene(sceneTarget)));
    R3D_SHADER_BIND_SAMPLER(prepare.dofDown, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.dofDown, uCoCTex, r3d_target_get(R3D_TARGET_DOF_COC));

    R3D_RENDER_SCREEN();

    /* --- Calculate DoF in half resolution --- */

    R3D_TARGET_BIND(false, R3D_TARGET_DOF_1);

    R3D_SHADER_USE(prepare.dofBlur);
    R3D_SHADER_BIND_SAMPLER(prepare.dofBlur, uSceneTex, r3d_target_get(R3D_TARGET_DOF_0));
    R3D_SHADER_BIND_SAMPLER(prepare.dofBlur, uDepthTex, r3d_target_get_level(R3D_TARGET_DEPTH, 1));
    R3D_SHADER_SET_FLOAT(prepare.dofBlur, uMaxBlurSize, R3D.environment.dof.maxBlurSize * 0.5f);

    R3D_RENDER_SCREEN();

    /* --- Compose DoF with the scene ---  */

    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.dof);

    R3D_SHADER_BIND_SAMPLER(post.dof, uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER(post.dof, uBlurTex, r3d_target_get(R3D_TARGET_DOF_1));

    R3D_RENDER_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_bloom(r3d_target_t sceneTarget)
{
    r3d_target_t sceneSource = r3d_target_swap_scene(sceneTarget);
    GLuint sceneSourceID = r3d_target_get(sceneSource);

    int numLevels = r3d_target_get_num_levels(R3D_TARGET_BLOOM);
    float txSrcW = 0, txSrcH = 0;

    R3D_TARGET_BIND(false, R3D_TARGET_BLOOM);

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

    int maxLevel = (int)((float)numLevels * R3D.environment.bloom.levels + 0.5f);
    if (maxLevel > numLevels) maxLevel = numLevels;
    else if (maxLevel < 1) maxLevel = 1;

    /* --- Karis average for the first downsampling to half res --- */

    R3D_SHADER_USE(prepare.bloomDown);
    R3D_SHADER_BIND_SAMPLER(prepare.bloomDown, uTexture, sceneSourceID);

    r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_SCENE_0, 0);
    R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
    R3D_SHADER_SET_VEC4(prepare.bloomDown, uPrefilter, prefilter);
    R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, 0);

    R3D_RENDER_SCREEN();

    /* --- Bloom Downsampling --- */

    // It's okay to sample the target here
    // Given that we'll be sampling a different level from where we're writing
    R3D_SHADER_BIND_SAMPLER(prepare.bloomDown, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = 1; dstLevel < maxLevel; dstLevel++)
    {
        r3d_target_set_viewport(R3D_TARGET_BLOOM, dstLevel);
        r3d_target_set_write_level(0, dstLevel);

        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, dstLevel - 1);
        R3D_SHADER_SET_VEC2(prepare.bloomDown, uTexelSize, (Vector2) {txSrcW, txSrcH});
        R3D_SHADER_SET_INT(prepare.bloomDown, uDstLevel, dstLevel);

        R3D_RENDER_SCREEN();
    }

    /* --- Bloom Upsampling --- */

    R3D_SHADER_USE(prepare.bloomUp);

    r3d_driver_enable(GL_BLEND);
    r3d_driver_set_blend_func(GL_FUNC_ADD, GL_ONE, GL_ONE);

    R3D_SHADER_BIND_SAMPLER(prepare.bloomUp, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = maxLevel - 2; dstLevel >= 0; dstLevel--)
    {
        r3d_target_set_viewport(R3D_TARGET_BLOOM, dstLevel);
        r3d_target_set_write_level(0, dstLevel);

        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, dstLevel + 1);
        R3D_SHADER_SET_FLOAT(prepare.bloomUp, uSrcLevel, (float)(dstLevel + 1));
        R3D_SHADER_SET_VEC2(prepare.bloomUp, uFilterRadius, (Vector2) {
            R3D.environment.bloom.filterRadius * txSrcW,
            R3D.environment.bloom.filterRadius * txSrcH
        });

        R3D_RENDER_SCREEN();
    }

    r3d_driver_disable(GL_BLEND);

    /* --- Apply bloom to the scene --- */

    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.bloom);

    R3D_SHADER_BIND_SAMPLER(post.bloom, uSceneTex, sceneSourceID);
    R3D_SHADER_BIND_SAMPLER(post.bloom, uBloomTex, r3d_target_get_all_levels(R3D_TARGET_BLOOM));

    R3D_SHADER_SET_INT(post.bloom, uBloomMode, R3D.environment.bloom.mode);
    R3D_SHADER_SET_FLOAT(post.bloom, uBloomIntensity, R3D.environment.bloom.intensity);

    R3D_RENDER_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_screen(r3d_target_t sceneTarget)
{
    for (int i = 0; i < ARRAY_SIZE(R3D.screenShaders); i++)
    {
        R3D_ScreenShader* shader = R3D.screenShaders[i];
        if (shader == NULL) continue;

        R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
        R3D_SHADER_USE_CUSTOM(R3D.screenShaders[i], post.screen);

        R3D_SHADER_BIND_SAMPLER_CUSTOM(shader, post.screen, uSceneTex, r3d_target_get(sceneTarget));
        R3D_SHADER_BIND_SAMPLER_CUSTOM(shader, post.screen, uNormalTex, r3d_target_get(R3D_TARGET_NORMAL));
        R3D_SHADER_BIND_SAMPLER_CUSTOM(shader, post.screen, uDepthTex, r3d_target_get(R3D_TARGET_DEPTH));

        R3D_RENDER_SCREEN();
    }

    return sceneTarget;
}

r3d_target_t pass_post_output(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.output);

    R3D_SHADER_BIND_SAMPLER(post.output, uSceneTex, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_FLOAT(post.output, uTonemapExposure, R3D.environment.tonemap.exposure);
    R3D_SHADER_SET_FLOAT(post.output, uTonemapWhite, R3D.environment.tonemap.white);
    R3D_SHADER_SET_INT(post.output, uTonemapMode, R3D.environment.tonemap.mode);
    R3D_SHADER_SET_FLOAT(post.output, uBrightness, R3D.environment.color.brightness);
    R3D_SHADER_SET_FLOAT(post.output, uContrast, R3D.environment.color.contrast);
    R3D_SHADER_SET_FLOAT(post.output, uSaturation, R3D.environment.color.saturation);

    R3D_RENDER_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_fxaa(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fxaa[R3D.aaPreset]);

    R3D_SHADER_BIND_SAMPLER(post.fxaa[R3D.aaPreset], uSceneTex, r3d_target_get(sceneTarget));
    R3D_RENDER_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_smaa(r3d_target_t sceneTarget)
{
    r3d_target_t sceneSource = r3d_target_swap_scene(sceneTarget);

    Vector4 metrics = {
        R3D_TARGET_TEXEL_WIDTH,
        R3D_TARGET_TEXEL_HEIGHT,
        R3D_TARGET_WIDTH,
        R3D_TARGET_HEIGHT
    };

    /* --- Clear previous content --- */

    // Bind and clear the stencil buffer. Since AA is the last post-processing
    // pass, clearing it here is safe. The stencil is used to avoid running the
    // blending weight calculation on pixels that have no edges, pass 1 writes 1
    // to the stencil for each edge pixel (non-edge pixels are discarded by the
    // shader), then pass 2 only executes where stencil == 1.

    r3d_driver_enable(GL_STENCIL_TEST);
    r3d_driver_set_stencil_mask(0xFF);

    R3D_TARGET_CLEAR(true, R3D_TARGET_SMAA_EDGES, R3D_TARGET_SMAA_BLEND);

    /* --- Edge detection ---  */

    r3d_driver_set_stencil_func(GL_ALWAYS, 1, 0xFF);
    r3d_driver_set_stencil_op(GL_KEEP, GL_KEEP, GL_REPLACE);

    R3D_TARGET_BIND(true, R3D_TARGET_SMAA_EDGES);
    R3D_SHADER_USE(prepare.smaaEdgeDetection[R3D.aaPreset]);

    R3D_SHADER_BIND_SAMPLER(prepare.smaaEdgeDetection[R3D.aaPreset], uSceneTex, r3d_target_get(sceneSource));

    R3D_RENDER_SCREEN();

    /* --- Compute blending weights --- */

    r3d_driver_set_stencil_func(GL_EQUAL, 1, 0xFF);
    r3d_driver_set_stencil_op(GL_KEEP, GL_KEEP, GL_KEEP);

    R3D_TARGET_BIND(true, R3D_TARGET_SMAA_BLEND);
    R3D_SHADER_USE(prepare.smaaBlendingWeights[R3D.aaPreset]);

    R3D_SHADER_BIND_SAMPLER(prepare.smaaBlendingWeights[R3D.aaPreset], uEdgesTex, r3d_target_get(R3D_TARGET_SMAA_EDGES));
    R3D_SHADER_BIND_SAMPLER(prepare.smaaBlendingWeights[R3D.aaPreset], uAreaTex, r3d_texture_get(R3D_TEXTURE_SMAA_AREA));
    R3D_SHADER_BIND_SAMPLER(prepare.smaaBlendingWeights[R3D.aaPreset], uSearchTex, r3d_texture_get(R3D_TEXTURE_SMAA_SEARCH));

    R3D_RENDER_SCREEN();

    /* --- Apply anti aliasing to the scene --- */

    r3d_driver_disable(GL_STENCIL_TEST);

    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.smaa[R3D.aaPreset]);

    R3D_SHADER_BIND_SAMPLER(post.smaa[R3D.aaPreset], uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER(post.smaa[R3D.aaPreset], uBlendTex, r3d_target_get(R3D_TARGET_SMAA_BLEND));

    R3D_RENDER_SCREEN();

    return sceneTarget;
}

void blit_to_screen(r3d_target_t source)
{
    if (r3d_target_get_or_null(source) == 0) {
        return;
    }

    GLuint dstId = R3D.screen.id;
    int dstW = dstId ? R3D.screen.texture.width  : GetRenderWidth();
    int dstH = dstId ? R3D.screen.texture.height : GetRenderHeight();

    int dstX = 0, dstY = 0;
    if (R3D.aspectMode == R3D_ASPECT_KEEP) {
        float srcRatio = (float)R3D_MOD_TARGET.resW / R3D_MOD_TARGET.resH;
        float dstRatio = (float)dstW / dstH;
        if (srcRatio > dstRatio) {
            int newH = (int)(dstW / srcRatio + 0.5f);
            dstY = (dstH - newH) / 2;
            dstH = newH;
        }
        else {
            int newW = (int)(dstH * srcRatio + 0.5f);
            dstX = (dstW - newW) / 2;
            dstW = newW;
        }
    }

    int srcW = 0, srcH = 0;
    r3d_target_get_resolution(&srcW, &srcH, source, 0);

    bool sameDim = (dstW == srcW) & (dstH == srcH);
    bool greater = (dstW >  srcW) | (dstH >  srcH);
    bool smaller = (dstW <  srcW) | (dstH <  srcH);

    if (sameDim || (greater && R3D.upscaleMode == R3D_UPSCALE_NEAREST) || (smaller && R3D.downscaleMode == R3D_DOWNSCALE_NEAREST)) {
        r3d_target_blit(source, true, dstId, dstX, dstY, dstW, dstH, false);
        return;
    }

    if ((greater && R3D.upscaleMode == R3D_UPSCALE_LINEAR) || (smaller && R3D.downscaleMode == R3D_DOWNSCALE_LINEAR)) {
        r3d_target_blit(source, true, dstId, dstX, dstY, dstW, dstH, true);
        return;
    }

    if (greater) {
        float txlW, txlH;
        r3d_target_get_texel_size(&txlW, &txlH, source, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, dstId);
        glViewport(dstX, dstY, dstW, dstH);

        switch (R3D.upscaleMode) {
        case R3D_UPSCALE_BICUBIC:
            R3D_SHADER_USE(prepare.bicubicUp);
            R3D_SHADER_SET_VEC2(prepare.bicubicUp, uSourceTexel, (Vector2) {txlW, txlH});
            R3D_SHADER_BIND_SAMPLER(prepare.bicubicUp, uSourceTex, r3d_target_get(source));
            break;
        case R3D_UPSCALE_LANCZOS:
            R3D_SHADER_USE(prepare.lanczosUp);
            R3D_SHADER_SET_VEC2(prepare.lanczosUp, uSourceTexel, (Vector2) {txlW, txlH});
            R3D_SHADER_BIND_SAMPLER(prepare.lanczosUp, uSourceTex, r3d_target_get(source));
            break;
        default:
            break;
        }

        R3D_RENDER_SCREEN();

        r3d_target_blit(0, true, dstId, dstX, dstY, dstW, dstH, false);
        return;
    }

    if (smaller && R3D.downscaleMode == R3D_DOWNSCALE_BOX) {
        glBindFramebuffer(GL_FRAMEBUFFER, dstId);
        glViewport(dstX, dstY, dstW, dstH);

        R3D_SHADER_USE(prepare.blurDown);
        R3D_SHADER_SET_INT(prepare.blurDown, uSourceLod, 0);
        R3D_SHADER_BIND_SAMPLER(prepare.blurDown, uSourceTex, r3d_target_get(source));

        R3D_RENDER_SCREEN();

        r3d_target_blit(0, true, dstId, dstX, dstY, dstW, dstH, false);
        return;
    }
}

void visualize_to_screen(r3d_target_t source)
{
    if (r3d_target_get_or_null(source) == 0) {
        return;
    }

    GLuint dstId = R3D.screen.id;
    int dstW = dstId ? R3D.screen.texture.width  : GetRenderWidth();
    int dstH = dstId ? R3D.screen.texture.height : GetRenderHeight();

    int dstX = 0, dstY = 0;
    if (R3D.aspectMode == R3D_ASPECT_KEEP) {
        float srcRatio = (float)R3D_MOD_TARGET.resW / R3D_MOD_TARGET.resH;
        float dstRatio = (float)dstW / dstH;
        if (srcRatio > dstRatio) {
            int newH = (int)(dstW / srcRatio + 0.5f);
            dstY = (dstH - newH) / 2;
            dstH = newH;
        }
        else {
            int newW = (int)(dstH * srcRatio + 0.5f);
            dstX = (dstW - newW) / 2;
            dstW = newW;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, dstId);
    glViewport(dstX, dstY, dstW, dstH);

    R3D_SHADER_USE(post.visualizer);
    R3D_SHADER_SET_INT(post.visualizer, uOutputMode, R3D.outputMode);
    R3D_SHADER_BIND_SAMPLER(post.visualizer, uSourceTex, r3d_target_get(source));

    R3D_RENDER_SCREEN();

    r3d_target_blit(0, true, dstId, dstX, dstY, dstW, dstH, false);
}

void cleanup_after_render(void)
{
    r3d_shader_invalidate_cache();
    r3d_target_invalidate_cache();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);

    r3d_driver_restore_viewport();

    r3d_driver_disable(GL_STENCIL_TEST);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_enable(GL_CULL_FACE);
    r3d_driver_enable(GL_BLEND);

    r3d_driver_set_blend_func(GL_FUNC_ADD, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    r3d_driver_set_depth_offset(0.0f, 0.0f);
    r3d_driver_set_depth_range(0.0f, 1.0f);
    r3d_driver_set_depth_func(GL_LEQUAL);
    r3d_driver_set_depth_mask(GL_TRUE);
    r3d_driver_set_cull_face(GL_BACK);

    // Here we re-define the blend mode via rlgl to ensure its internal state
    // matches what we've just set manually with OpenGL.

    // It's not enough to change the blend mode only through rlgl, because if we
    // previously used a different blend mode (not "alpha") but rlgl still thinks it's "alpha",
    // then rlgl won't correctly apply the intended blend mode.

    // We do this at the end because calling rlSetBlendMode can trigger a draw call for
    // any content accumulated by rlgl, and we want that to be rendered into the main
    // framebuffer, not into one of R3D's internal framebuffers that will be discarded afterward.

    rlSetBlendMode(RL_BLEND_ALPHA);

    // Here we reset the target sampling levels to facilitate debugging with RenderDoc
    // WARNING: Make sure that everything that affects levels works in release mode!

#ifndef NDEBUG
    for (int iTarget = 0; iTarget < R3D_TARGET_COUNT; iTarget++) {
        if (r3d_target_get_or_null(iTarget) != 0) {
            r3d_target_set_read_levels(iTarget, 0, r3d_target_get_num_levels(iTarget) - 1);
        }
    }
#endif
}
