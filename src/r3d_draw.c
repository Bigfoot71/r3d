/* r3d_draw.h -- R3D Draw Module.
 *
 * Copyright (c) 2025 Le Juez Victor
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
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_draw.h"
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
static void upload_light_array_block_for_mesh(const r3d_draw_call_t* call, bool shadow);
static void upload_view_block(void);
static void upload_env_block(void);

static void raster_depth(const r3d_draw_call_t* call, bool shadow, const Matrix* viewProj);
static void raster_depth_cube(const r3d_draw_call_t* call, bool shadow, const Matrix* viewProj);
static void raster_geometry(const r3d_draw_call_t* call);
static void raster_decal(const r3d_draw_call_t* call);
static void raster_forward(const r3d_draw_call_t* call);

static void pass_scene_shadow(void);
static void pass_scene_probes(void);
static void pass_scene_geometry(void);
static void pass_scene_prepass(void);
static void pass_scene_decals(void);

static void pass_prepare_buffer_down(void);
static r3d_target_t pass_prepare_ssao(void);
static r3d_target_t pass_prepare_ssil(void);
static r3d_target_t pass_prepare_ssr(void);

static void pass_deferred_lights(void);
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

static void blit_to_screen(r3d_target_t source);
static void visualize_to_screen(r3d_target_t source);

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
    update_view_state(camera, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    R3D.screen = target;
    r3d_draw_clear();
}

void R3D_End(void)
{
    /* --- Upload and bind uniform buffers --- */

    upload_view_block();
    upload_env_block();

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
        r3d_draw_sort_list(R3D_DRAW_LIST_DEFERRED, R3D.viewState.viewPosition, R3D_DRAW_SORT_FRONT_TO_BACK);
    }

    if (R3D_CORE_FLAGS_HAS(state, R3D_FLAG_TRANSPARENT_SORTING)) {
        r3d_draw_sort_list(R3D_DRAW_LIST_PREPASS, R3D.viewState.viewPosition, R3D_DRAW_SORT_BACK_TO_FRONT);
        r3d_draw_sort_list(R3D_DRAW_LIST_FORWARD, R3D.viewState.viewPosition, R3D_DRAW_SORT_BACK_TO_FRONT);
    }

    /* --- Deferred path for opaques and decals --- */

    r3d_target_t sceneTarget = R3D_TARGET_SCENE_0;
    r3d_target_t ssaoSource = R3D_TARGET_INVALID;
    r3d_target_t ssilSource = R3D_TARGET_INVALID;
    r3d_target_t ssrSource = R3D_TARGET_INVALID;

    if (r3d_draw_has_deferred() || r3d_draw_has_prepass()) {
        R3D_TARGET_CLEAR(true, R3D_TARGET_ALL_DEFERRED);

        if (r3d_draw_has_deferred()) pass_scene_geometry();
        if (r3d_draw_has_prepass()) pass_scene_prepass();
        if (r3d_draw_has_decal()) pass_scene_decals();
        if (r3d_light_has_visible()) pass_deferred_lights();

        bool ssao = R3D.environment.ssao.enabled;
        bool ssil = R3D.environment.ssil.enabled;
        bool ssr = R3D.environment.ssr.enabled;

        if (ssao || ssil || ssr) pass_prepare_buffer_down();
        if (ssao) ssaoSource = pass_prepare_ssao();
        if (ssil) ssilSource = pass_prepare_ssil();
        if (ssr) ssrSource = pass_prepare_ssr();

        pass_deferred_ambient(ssaoSource, ssilSource, ssrSource);
        pass_deferred_compose(sceneTarget);
    }
    else {
        r3d_target_clear(NULL, 0, 0, true);
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

    if (R3D.antiAliasing == R3D_ANTI_ALIASING_FXAA) {
        sceneTarget = pass_post_fxaa(sceneTarget);
    }

    switch (R3D.outputMode) {
    case R3D_OUTPUT_SCENE: blit_to_screen(r3d_target_swap_scene(sceneTarget)); break;
    case R3D_OUTPUT_ALBEDO: visualize_to_screen(R3D_TARGET_ALBEDO); break;
    case R3D_OUTPUT_NORMAL: visualize_to_screen(R3D_TARGET_NORMAL); break;
    case R3D_OUTPUT_TANGENT: visualize_to_screen(R3D_TARGET_GEOM_NORM_TAN); break;
    case R3D_OUTPUT_ORM: visualize_to_screen(R3D_TARGET_ORM); break;
    case R3D_OUTPUT_DIFFUSE: visualize_to_screen(R3D_TARGET_DIFFUSE); break;
    case R3D_OUTPUT_SPECULAR: visualize_to_screen(R3D_TARGET_SPECULAR); break;
    case R3D_OUTPUT_SSAO: visualize_to_screen(ssaoSource); break;
    case R3D_OUTPUT_SSIL: visualize_to_screen(ssilSource); break;
    case R3D_OUTPUT_SSR: visualize_to_screen(ssrSource); break;
    case R3D_OUTPUT_BLOOM: visualize_to_screen(R3D_TARGET_BLOOM); break;
    }

    /* --- Reset states changed by R3D --- */

    reset_raylib_state();
}

void R3D_BeginCluster(BoundingBox aabb)
{
    if (!r3d_draw_cluster_begin(aabb)) {
        R3D_TRACELOG(LOG_WARNING, "R3D: Failed to begin cluster");
    }
}

void R3D_EndCluster(void)
{
    if (!r3d_draw_cluster_end()) {
        R3D_TRACELOG(LOG_WARNING, "R3D: Failed to end cluster");
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
    drawCall.type = R3D_DRAW_CALL_MESH;
    drawCall.mesh.material = material;
    drawCall.mesh.instance = mesh;

    r3d_draw_call_push(&drawCall);
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
    drawCall.type = R3D_DRAW_CALL_MESH;
    drawCall.mesh.material = material;
    drawCall.mesh.instance = mesh;

    r3d_draw_call_push(&drawCall);
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
        drawCall.type = R3D_DRAW_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_draw_call_push(&drawCall);
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
        drawCall.type = R3D_DRAW_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_draw_call_push(&drawCall);
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
        drawCall.type = R3D_DRAW_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_draw_call_push(&drawCall);
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
        drawCall.type = R3D_DRAW_CALL_MESH;
        drawCall.mesh.material = model.materials[model.meshMaterials[i]];
        drawCall.mesh.instance = *mesh;

        r3d_draw_call_push(&drawCall);
    }
}

void R3D_DrawDecal(R3D_Decal decal, Matrix transform)
{
    decal.normalThreshold = (decal.normalThreshold == 0.0) ? PI * 2 : decal.normalThreshold * DEG2RAD;
    decal.fadeWidth = decal.fadeWidth * DEG2RAD;

    r3d_draw_group_t drawGroup = {0};
    drawGroup.transform = transform;

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};
    drawCall.type = R3D_DRAW_CALL_DECAL;
    drawCall.decal.instance = decal;

    r3d_draw_call_push(&drawCall);
}

void R3D_DrawDecalInstanced(R3D_Decal decal, R3D_InstanceBuffer instances, int count)
{
    decal.normalThreshold = (decal.normalThreshold == 0.0) ? PI * 2 : decal.normalThreshold * DEG2RAD;
    decal.fadeWidth = decal.fadeWidth * DEG2RAD;

    r3d_draw_group_t drawGroup = {0};
    drawGroup.transform = R3D_MATRIX_IDENTITY;
    drawGroup.instances = instances;
    drawGroup.instanceCount = CLAMP(count, 0, instances.capacity);

    r3d_draw_group_push(&drawGroup);

    r3d_draw_call_t drawCall = {0};
    drawCall.type = R3D_DRAW_CALL_DECAL;
    drawCall.decal.instance = decal;

    r3d_draw_call_push(&drawCall);
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

    Matrix viewProj = r3d_matrix_multiply(&view, &proj);

    R3D.viewState.frustum = r3d_frustum_create(viewProj);
    R3D.viewState.viewPosition = camera.position;

    R3D.viewState.view = view;
    R3D.viewState.proj = proj;
    R3D.viewState.invView = MatrixInvert(view);
    R3D.viewState.invProj = MatrixInvert(proj);
    R3D.viewState.viewProj = viewProj;

    R3D.viewState.aspect = (float)aspect;
    R3D.viewState.near = (float)near;
    R3D.viewState.far = (float)far;
}

void upload_light_array_block_for_mesh(const r3d_draw_call_t* call, bool shadow)
{
    assert(call->type == R3D_DRAW_CALL_MESH); //< Paranoid assert, should be fine

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
        data->viewProj = r3d_matrix_transpose(&light->viewProj[0]);
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
        data->shadowTexelSize = light->shadowTexelSize;
        data->shadowDepthBias = light->shadowDepthBias;
        data->shadowSlopeBias = light->shadowSlopeBias;
        data->shadowLayer = light->shadowLayer;
        data->shadow = shadow && light->shadow;
        data->type = light->type;

        if (++lights.uNumLights == R3D_MAX_LIGHT_FORWARD_PER_MESH) {
            break;
        }
    }

    r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_LIGHT_ARRAY, &lights);
}

void upload_view_block(void)
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

void raster_depth(const r3d_draw_call_t* call, bool shadow, const Matrix* viewProj)
{
    assert(call->type == R3D_DRAW_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depth, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.depth, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER(scene.depth, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.depth, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depth, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depth, uBillboard, material->billboardMode);
    if (material->billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depth, uMatInvView, R3D.viewState.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordScale, material->uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER(scene.depth, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_SET_COL4(scene.depth, uAlbedoColor, R3D.colorSpace, material->albedo.color);

    if (material->transparencyMode == R3D_TRANSPARENCY_PREPASS) {
        R3D_SHADER_SET_FLOAT(scene.depth, uAlphaCutoff, shadow ? 0.1f : 0.99f);
    }
    else {
        R3D_SHADER_SET_FLOAT(scene.depth, uAlphaCutoff, material->alphaCutoff);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_draw_apply_shadow_cast_mode(mesh->shadowCastMode, material->cullMode);
    }
    else {
        r3d_draw_apply_cull_mode(material->cullMode);
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
}

void raster_depth_cube(const r3d_draw_call_t* call, bool shadow, const Matrix* viewProj)
{
    assert(call->type == R3D_DRAW_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depthCube, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.depthCube, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER(scene.depthCube, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depthCube, uBillboard, material->billboardMode);
    if (material->billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depthCube, uMatInvView, R3D.viewState.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordScale, material->uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER(scene.depthCube, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_SET_COL4(scene.depthCube, uAlbedoColor, R3D.colorSpace, material->albedo.color);

    if (material->transparencyMode == R3D_TRANSPARENCY_PREPASS) {
        R3D_SHADER_SET_FLOAT(scene.depthCube, uAlphaCutoff, shadow ? 0.1f : 0.99f);
    }
    else {
        R3D_SHADER_SET_FLOAT(scene.depthCube, uAlphaCutoff, material->alphaCutoff);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_draw_apply_shadow_cast_mode(mesh->shadowCastMode, material->cullMode);
    }
    else {
        r3d_draw_apply_cull_mode(material->cullMode);
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
}

void raster_probe(const r3d_draw_call_t* call, const Matrix* invView, const Matrix* viewProj)
{
    assert(call->type == R3D_DRAW_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.probe, uMatInvView, *invView);
    R3D_SHADER_SET_MAT4(scene.probe, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.probe, uMatNormal, matNormal);
    R3D_SHADER_SET_MAT4(scene.probe, uMatViewProj, *viewProj);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER(scene.probe, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.probe, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.probe, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.probe, uBillboard, material->billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.probe, uEmissionEnergy, material->emission.energy);
    R3D_SHADER_SET_FLOAT(scene.probe, uNormalScale, material->normal.scale);
    R3D_SHADER_SET_FLOAT(scene.probe, uOcclusion, material->orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.probe, uRoughness, material->orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.probe, uMetalness, material->orm.metalness);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.probe, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2(scene.probe, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.probe, uAlbedoColor, R3D.colorSpace, material->albedo.color);
    R3D_SHADER_SET_COL3(scene.probe, uEmissionColor, R3D.colorSpace, material->emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER(scene.probe, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.probe, uNormalMap, R3D_TEXTURE_SELECT(material->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER(scene.probe, uEmissionMap, R3D_TEXTURE_SELECT(material->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.probe, uOrmMap, R3D_TEXTURE_SELECT(material->orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(material->blendMode, material->transparencyMode);
    r3d_draw_apply_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.probe, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.probe, uInstancing, false);
        r3d_draw(call);
    }
}

void raster_geometry(const r3d_draw_call_t* call)
{
    assert(call->type == R3D_DRAW_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.geometry, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.geometry, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER(scene.geometry, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.geometry, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.geometry, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.geometry, uBillboard, material->billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.geometry, uEmissionEnergy, material->emission.energy);
    R3D_SHADER_SET_FLOAT(scene.geometry, uNormalScale, material->normal.scale);
    R3D_SHADER_SET_FLOAT(scene.geometry, uOcclusion, material->orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.geometry, uRoughness, material->orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.geometry, uMetalness, material->orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT(scene.geometry, uAlphaCutoff, material->alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.geometry, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2(scene.geometry, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.geometry, uAlbedoColor, R3D.colorSpace, material->albedo.color);
    R3D_SHADER_SET_COL3(scene.geometry, uEmissionColor, R3D.colorSpace, material->emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER(scene.geometry, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.geometry, uNormalMap, R3D_TEXTURE_SELECT(material->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER(scene.geometry, uEmissionMap, R3D_TEXTURE_SELECT(material->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.geometry, uOrmMap, R3D_TEXTURE_SELECT(material->orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.geometry, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.geometry, uInstancing, false);
        r3d_draw(call);
    }
}

void raster_decal(const r3d_draw_call_t* call)
{
    assert(call->type == R3D_DRAW_CALL_DECAL); //< Paranoid assert, should be fine

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_Decal* decal = &call->decal.instance;

    /* --- Set additional matrix uniforms --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.decal, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.decal, uMatNormal, matNormal);

    /* --- Skinning is never used for decals --- */

    R3D_SHADER_SET_INT(scene.decal, uSkinning, false);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.decal, uEmissionEnergy, decal->emission.energy);
    R3D_SHADER_SET_FLOAT(scene.decal, uNormalScale, decal->normal.scale);
    R3D_SHADER_SET_FLOAT(scene.decal, uOcclusion, decal->orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.decal, uRoughness, decal->orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.decal, uMetalness, decal->orm.metalness);

    /* --- Set misc material values --- */

    R3D_SHADER_SET_FLOAT(scene.decal, uAlphaCutoff, decal->alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.decal, uTexCoordOffset, decal->uvOffset);
    R3D_SHADER_SET_VEC2(scene.decal, uTexCoordScale, decal->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.decal, uAlbedoColor, R3D.colorSpace, decal->albedo.color);
    R3D_SHADER_SET_COL3(scene.decal, uEmissionColor, R3D.colorSpace, decal->emission.color);

    /* --- Set decal specific values --- */

    R3D_SHADER_SET_FLOAT(scene.decal, uNormalThreshold, decal->normalThreshold);
    R3D_SHADER_SET_FLOAT(scene.decal, uFadeWidth, decal->fadeWidth);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER(scene.decal, uAlbedoMap, R3D_TEXTURE_SELECT(decal->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.decal, uNormalMap, R3D_TEXTURE_SELECT(decal->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER(scene.decal, uEmissionMap, R3D_TEXTURE_SELECT(decal->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.decal, uOrmMap, R3D_TEXTURE_SELECT(decal->orm.texture.id, WHITE));

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.decal, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.decal, uInstancing, false);
        r3d_draw(call);
    }
}

void raster_forward(const r3d_draw_call_t* call)
{
    assert(call->type == R3D_DRAW_CALL_MESH); //< Paranoid assert, should be fine

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_Material* material = &call->mesh.material;
    const R3D_Mesh* mesh = &call->mesh.instance;

    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&group->transform);

    R3D_SHADER_SET_MAT4(scene.forward, uMatModel, group->transform);
    R3D_SHADER_SET_MAT4(scene.forward, uMatNormal, matNormal);

    /* --- Send skinning related data --- */

    if (group->texPose > 0) {
        R3D_SHADER_BIND_SAMPLER(scene.forward, uBoneMatricesTex, group->texPose);
        R3D_SHADER_SET_INT(scene.forward, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.forward, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.forward, uBillboard, material->billboardMode);

    /* --- Set factor material maps --- */

    R3D_SHADER_SET_FLOAT(scene.forward, uEmissionEnergy, material->emission.energy);
    R3D_SHADER_SET_FLOAT(scene.forward, uNormalScale, material->normal.scale);
    R3D_SHADER_SET_FLOAT(scene.forward, uOcclusion, material->orm.occlusion);
    R3D_SHADER_SET_FLOAT(scene.forward, uRoughness, material->orm.roughness);
    R3D_SHADER_SET_FLOAT(scene.forward, uMetalness, material->orm.metalness);

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.forward, uTexCoordOffset, material->uvOffset);
    R3D_SHADER_SET_VEC2(scene.forward, uTexCoordScale, material->uvScale);

    /* --- Set color material maps --- */

    R3D_SHADER_SET_COL4(scene.forward, uAlbedoColor, R3D.colorSpace, material->albedo.color);
    R3D_SHADER_SET_COL3(scene.forward, uEmissionColor, R3D.colorSpace, material->emission.color);

    /* --- Bind active texture maps --- */

    R3D_SHADER_BIND_SAMPLER(scene.forward, uAlbedoMap, R3D_TEXTURE_SELECT(material->albedo.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.forward, uNormalMap, R3D_TEXTURE_SELECT(material->normal.texture.id, NORMAL));
    R3D_SHADER_BIND_SAMPLER(scene.forward, uEmissionMap, R3D_TEXTURE_SELECT(material->emission.texture.id, WHITE));
    R3D_SHADER_BIND_SAMPLER(scene.forward, uOrmMap, R3D_TEXTURE_SELECT(material->orm.texture.id, WHITE));

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_draw_apply_blend_mode(material->blendMode, material->transparencyMode);
    r3d_draw_apply_cull_mode(material->cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    if (r3d_draw_has_instances(group)) {
        R3D_SHADER_SET_INT(scene.forward, uInstancing, true);
        r3d_draw_instanced(call);
    }
    else {
        R3D_SHADER_SET_INT(scene.forward, uInstancing, false);
        r3d_draw(call);
    }
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

        if (light->type == R3D_LIGHT_OMNI) {
            R3D_SHADER_USE(scene.depthCube);
            R3D_SHADER_SET_FLOAT(scene.depthCube, uFar, light->far);
            R3D_SHADER_SET_VEC3(scene.depthCube, uViewPosition, light->position);

            for (int iFace = 0; iFace < 6; iFace++) {
                r3d_light_shadow_bind_fbo(light->type, light->shadowLayer, iFace);
                glClear(GL_DEPTH_BUFFER_BIT);

                const r3d_frustum_t* frustum = &light->frustum[iFace];
                r3d_draw_compute_visible_groups(frustum);

                #define COND (call->mesh.instance.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
                R3D_DRAW_FOR_EACH(call, COND, frustum, R3D_DRAW_LIST_DEFERRED_INST, R3D_DRAW_LIST_DEFERRED, R3D_DRAW_LIST_PREPASS_INST, R3D_DRAW_LIST_PREPASS) {
                    raster_depth_cube(call, true, &light->viewProj[iFace]);
                }
                #undef COND
            }
        }
        else {
            r3d_light_shadow_bind_fbo(light->type, light->shadowLayer, 0);
            glClear(GL_DEPTH_BUFFER_BIT);
            R3D_SHADER_USE(scene.depth);

            const r3d_frustum_t* frustum = &light->frustum[0];
            r3d_draw_compute_visible_groups(frustum);

                #define COND (call->mesh.instance.shadowCastMode != R3D_SHADOW_CAST_DISABLED)
            R3D_DRAW_FOR_EACH(call, COND, frustum, R3D_DRAW_LIST_DEFERRED_INST, R3D_DRAW_LIST_DEFERRED, R3D_DRAW_LIST_PREPASS_INST, R3D_DRAW_LIST_PREPASS) {
                raster_depth(call, true, &light->viewProj[0]);
            }
            #undef COND
        }
    }
}

void pass_scene_probes(void)
{
    #define PROBES_DRAW_LISTS        \
        R3D_DRAW_LIST_DEFERRED_INST, \
        R3D_DRAW_LIST_PREPASS_INST,  \
        R3D_DRAW_LIST_FORWARD_INST,  \
        R3D_DRAW_LIST_DEFERRED,      \
        R3D_DRAW_LIST_PREPASS,       \
        R3D_DRAW_LIST_FORWARD,

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

            R3D_SHADER_BIND_SAMPLER(scene.probe, uShadowDirTex, r3d_light_shadow_get(R3D_LIGHT_DIR));
            R3D_SHADER_BIND_SAMPLER(scene.probe, uShadowSpotTex, r3d_light_shadow_get(R3D_LIGHT_SPOT));
            R3D_SHADER_BIND_SAMPLER(scene.probe, uShadowOmniTex, r3d_light_shadow_get(R3D_LIGHT_OMNI));

            R3D_SHADER_BIND_SAMPLER(scene.probe, uIrradianceTex, r3d_env_irradiance_get());
            R3D_SHADER_BIND_SAMPLER(scene.probe, uPrefilterTex, r3d_env_prefilter_get());
            R3D_SHADER_BIND_SAMPLER(scene.probe, uBrdfLutTex, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

            R3D_SHADER_SET_VEC3(scene.probe, uViewPosition, probe->position);
            R3D_SHADER_SET_INT(scene.probe, uProbeInterior, probe->interior);

            r3d_env_capture_bind_fbo(iFace, 0);
            glClear(GL_DEPTH_BUFFER_BIT);

            R3D_DRAW_FOR_EACH(call, true, frustum, PROBES_DRAW_LISTS) {
                upload_light_array_block_for_mesh(call, probe->shadows);
                raster_probe(call, &invView, &viewProj);
            }

            /* --- Render background --- */

            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);

            if (!probe->interior && R3D.environment.background.sky.texture != 0) {
                R3D_SHADER_USE(scene.skybox);
                glDisable(GL_CULL_FACE);

                float lod = (float)r3d_get_mip_levels_1d(R3D.environment.background.sky.size);

                R3D_SHADER_BIND_SAMPLER(scene.skybox, uSkyMap, R3D.environment.background.sky.texture);
                R3D_SHADER_SET_FLOAT(scene.skybox, uSkyEnergy, R3D.environment.background.energy);
                R3D_SHADER_SET_FLOAT(scene.skybox, uSkyLod, R3D.environment.background.skyBlur * lod);
                R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.environment.background.rotation);
                R3D_SHADER_SET_MAT4(scene.skybox, uMatView, probe->view[iFace]);
                R3D_SHADER_SET_MAT4(scene.skybox, uMatProj, probe->proj[iFace]);

                R3D_DRAW_CUBE();
            }
            else {
                Vector4 background = r3d_color_to_linear_scaled_vec4(
                    R3D.environment.background.color, R3D.colorSpace,
                    R3D.environment.background.energy
                );
                R3D_SHADER_USE(scene.background);
                R3D_SHADER_SET_VEC4(scene.background, uColor, background);
                R3D_DRAW_SCREEN();
            }
        }

        /* --- Generate irradiance and prefilter maps --- */

        r3d_env_capture_gen_mipmaps();

        if (probe->irradiance >= 0) {
            r3d_pass_prepare_irradiance(probe->irradiance, r3d_env_capture_get(), R3D_PROBE_CAPTURE_SIZE);
        }

        if (probe->prefilter >= 0) {
            r3d_pass_prepare_prefilter(probe->prefilter, r3d_env_capture_get(), R3D_PROBE_CAPTURE_SIZE);
        }

        r3d_target_reset(); //< The IBL gen functions bind framebuffers; resetting them prevents any problems
    }
}

void pass_scene_geometry(void)
{
    R3D_TARGET_BIND(true, R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.geometry);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_LIST_DEFERRED_INST, R3D_DRAW_LIST_DEFERRED) {
        raster_geometry(call);
    }
}

void pass_scene_prepass(void)
{
    /* --- First render only depth --- */

    r3d_target_bind(NULL, 0, 0, true);
    R3D_SHADER_USE(scene.depth);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_LIST_PREPASS_INST, R3D_DRAW_LIST_PREPASS) {
        raster_depth(call, false, &R3D.viewState.viewProj);
    }

    /* --- Render opaque only with GL_EQUAL --- */

    // NOTE: The transparent part will be rendered in forward
    R3D_TARGET_BIND(true, R3D_TARGET_GBUFFER);
    R3D_SHADER_USE(scene.geometry);

    glDepthFunc(GL_EQUAL);
    glDepthMask(GL_FALSE);

    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_LIST_PREPASS_INST, R3D_DRAW_LIST_PREPASS) {
        raster_geometry(call);
    }
}

void pass_scene_decals(void)
{
    R3D_TARGET_BIND(false, R3D_TARGET_DECAL);
    R3D_SHADER_USE(scene.decal);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // Only render back faces to avoid clipping issues

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    R3D_SHADER_BIND_SAMPLER(scene.decal, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 0, 0));
    R3D_SHADER_BIND_SAMPLER(scene.decal, uNormTanTex, r3d_target_get(R3D_TARGET_GEOM_NORM_TAN));

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_LIST_DECAL_INST, R3D_DRAW_LIST_DECAL) {
        raster_decal(call);
    }

    glCullFace(GL_BACK);
}

void pass_prepare_buffer_down(void)
{
    R3D_TARGET_BIND_LEVEL(1, R3D_TARGET_ALBEDO, R3D_TARGET_NORMAL, R3D_TARGET_ORM, R3D_TARGET_DEPTH, R3D_TARGET_DIFFUSE);
    R3D_SHADER_USE(prepare.bufferDown);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    R3D_SHADER_BIND_SAMPLER(prepare.bufferDown, uAlbedoTex, r3d_target_get_levels(R3D_TARGET_ALBEDO, 0, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.bufferDown, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 0, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.bufferDown, uOrmTex, r3d_target_get_levels(R3D_TARGET_ORM, 0, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.bufferDown, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 0, 0));
    R3D_SHADER_BIND_SAMPLER(prepare.bufferDown, uDiffuseTex, r3d_target_get_levels(R3D_TARGET_DIFFUSE, 0, 0));

    R3D_DRAW_SCREEN();
}

r3d_target_t pass_prepare_ssao(void)
{
    /* --- Setup OpenGL pipeline --- */

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

    R3D_SHADER_BIND_SAMPLER(prepare.ssao, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssao, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 1, 1));

    R3D_DRAW_SCREEN();

    /* --- Denoise SSAO --- */

    R3D_SHADER_USE(prepare.atrousWavelet);

    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 1, 1));

    for (int i = 0; i < 3; i++) {
        R3D_TARGET_BIND_AND_SWAP_SSAO(ssaoTarget);
        R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uSourceTex, r3d_target_get(ssaoTarget));
        R3D_SHADER_SET_INT(prepare.atrousWavelet, uStepSize, 1 << i);
        R3D_DRAW_SCREEN();
    }

    return r3d_target_swap_ssao(ssaoTarget);
}

r3d_target_t pass_prepare_ssil(void)
{
    /* --- Check if we need history --- */

    static r3d_target_t SSIL_HISTORY  = R3D_TARGET_SSIL_0;
    static r3d_target_t SSIL_RAW      = R3D_TARGET_SSIL_1;
    static r3d_target_t SSIL_FILTERED = R3D_TARGET_SSIL_2;

    bool needConvergence = (R3D.environment.ssil.convergence >= 0.1f);
    bool needBounce      = (R3D.environment.ssil.bounce >= 0.01f);
    bool needHistory     = (needConvergence || needBounce);

    if (needHistory && r3d_target_get_or_null(SSIL_HISTORY) == 0) {
        R3D_TARGET_CLEAR(SSIL_HISTORY);
    }

    /* --- Setup OpenGL pipeline --- */

    glDisable(GL_DEPTH_TEST);   //< Can't depth test to touch only the geometry, since the target is half res...
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    /* --- Calculate SSIL (RAW) --- */

    R3D_TARGET_BIND(false, SSIL_RAW);
    R3D_SHADER_USE(prepare.ssil);

    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uLightingTex, r3d_target_get_levels(R3D_TARGET_DIFFUSE, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uHistoryTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(SSIL_HISTORY), BLACK));
    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssil, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 1, 1));

    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleCount, (float)R3D.environment.ssil.sampleCount);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSampleRadius, R3D.environment.ssil.sampleRadius);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uSliceCount, (float)R3D.environment.ssil.sliceCount);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uHitThickness, R3D.environment.ssil.hitThickness);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uConvergence, R3D.environment.ssil.convergence);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uAoPower, R3D.environment.ssil.aoPower);
    R3D_SHADER_SET_FLOAT(prepare.ssil, uBounce, R3D.environment.ssil.bounce);

    R3D_DRAW_SCREEN();

    /* --- Atrous denoise: RAW -> FILTERED --- */

    R3D_SHADER_USE(prepare.atrousWavelet);

    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 1, 1));

    r3d_target_t src = SSIL_RAW;
    r3d_target_t dst = SSIL_FILTERED;

    for (int i = 0; i < 3; i++) {
        R3D_TARGET_BIND(false, dst);
        R3D_SHADER_BIND_SAMPLER(prepare.atrousWavelet, uSourceTex, r3d_target_get(src));
        R3D_SHADER_SET_INT(prepare.atrousWavelet, uStepSize, 1 << i);
        R3D_DRAW_SCREEN();
        SWAP(r3d_target_t, src, dst);
    }

    r3d_target_t SSIL_RESULT = src;

    /* --- Store history --- */

    if (needHistory) {
        SWAP(r3d_target_t, SSIL_HISTORY, SSIL_RESULT);
    }

    return SSIL_RESULT;
}

r3d_target_t pass_prepare_ssr(void)
{
    /* --- Setup OpenGL pipeline --- */

    glDisable(GL_DEPTH_TEST);   //< Can't depth test to touch only the geometry, since the target is half res...
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    /* --- Calculate SSR and downsample it --- */

    R3D_TARGET_BIND(false, R3D_TARGET_SSR);
    R3D_SHADER_USE(prepare.ssr);

    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uLightingTex, r3d_target_get_levels(R3D_TARGET_DIFFUSE, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uAlbedoTex, r3d_target_get_levels(R3D_TARGET_ALBEDO, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uOrmTex, r3d_target_get_levels(R3D_TARGET_ORM, 1, 1));
    R3D_SHADER_BIND_SAMPLER(prepare.ssr, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 1, 1));

    R3D_SHADER_SET_INT(prepare.ssr, uMaxRaySteps, R3D.environment.ssr.maxRaySteps);
    R3D_SHADER_SET_INT(prepare.ssr, uBinarySearchSteps, R3D.environment.ssr.binarySearchSteps);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uRayMarchLength, R3D.environment.ssr.rayMarchLength);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uDepthThickness, R3D.environment.ssr.depthThickness);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uDepthTolerance, R3D.environment.ssr.depthTolerance);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFadeStart, R3D.environment.ssr.edgeFadeStart);
    R3D_SHADER_SET_FLOAT(prepare.ssr, uEdgeFadeEnd, R3D.environment.ssr.edgeFadeEnd);

    R3D_SHADER_SET_VEC3(prepare.ssr, uAmbientColor, r3d_color_to_vec3(R3D.environment.ambient.color));
    R3D_SHADER_SET_FLOAT(prepare.ssr, uAmbientEnergy, R3D.environment.ambient.energy);

    R3D_DRAW_SCREEN();

    r3d_target_gen_mipmap(R3D_TARGET_SSR);

    return R3D_TARGET_SSR;
}

void pass_deferred_lights(void)
{
    /* --- Setup OpenGL pipeline --- */

    R3D_TARGET_BIND(true, R3D_TARGET_LIGHTING);

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

    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uAlbedoTex, r3d_target_get_levels(R3D_TARGET_ALBEDO, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uOrmTex, r3d_target_get_levels(R3D_TARGET_ORM, 0, 0));

    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uShadowDirTex, r3d_light_shadow_get(R3D_LIGHT_DIR));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uShadowSpotTex, r3d_light_shadow_get(R3D_LIGHT_SPOT));
    R3D_SHADER_BIND_SAMPLER(deferred.lighting, uShadowOmniTex, r3d_light_shadow_get(R3D_LIGHT_OMNI));

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
            .viewProj = r3d_matrix_transpose(&light->viewProj[0]),
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
            .shadowTexelSize = light->shadowTexelSize,
            .shadowDepthBias = light->shadowDepthBias,
            .shadowSlopeBias = light->shadowSlopeBias,
            .shadowLayer = light->shadowLayer,
            .shadow = light->shadow,
            .type = light->type,
        };
        r3d_shader_set_uniform_block(R3D_SHADER_BLOCK_LIGHT, &data);

        // Accumulate this light!
        R3D_DRAW_SCREEN();
    }

    /* --- Reset undesired state --- */

    glDisable(GL_SCISSOR_TEST);
}

void pass_deferred_ambient(r3d_target_t ssaoSource, r3d_target_t ssilSource, r3d_target_t ssrSource)
{
    /* --- Setup OpenGL pipeline --- */

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);

    // Set additive blending to accumulate light contributions
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    /* --- Calculation and composition of ambient/indirect lighting --- */

    R3D_TARGET_BIND(true, R3D_TARGET_LIGHTING);
    R3D_SHADER_USE(deferred.ambient);

    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uAlbedoTex, r3d_target_get_levels(R3D_TARGET_ALBEDO, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uNormalTex, r3d_target_get_levels(R3D_TARGET_NORMAL, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uSsaoTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssaoSource), WHITE));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uSsilTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssilSource), BLACK));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uSsrTex, R3D_TEXTURE_SELECT(r3d_target_get_or_null(ssrSource), BLANK));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uOrmTex, r3d_target_get_levels(R3D_TARGET_ORM, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uIrradianceTex, r3d_env_irradiance_get());
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uPrefilterTex, r3d_env_prefilter_get());
    R3D_SHADER_BIND_SAMPLER(deferred.ambient, uBrdfLutTex, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

    R3D_SHADER_SET_FLOAT(deferred.ambient, uSsrNumLevels, (float)r3d_target_get_num_levels(R3D_TARGET_SSR));
    R3D_SHADER_SET_FLOAT(deferred.ambient, uSsilEnergy, R3D.environment.ssil.energy);

    R3D_DRAW_SCREEN();
}

void pass_deferred_compose(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(true, sceneTarget);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    R3D_SHADER_USE(deferred.compose);

    R3D_SHADER_BIND_SAMPLER(deferred.compose, uDiffuseTex, r3d_target_get_levels(R3D_TARGET_DIFFUSE, 0, 0));
    R3D_SHADER_BIND_SAMPLER(deferred.compose, uSpecularTex, r3d_target_get_levels(R3D_TARGET_SPECULAR, 0, 0));

    R3D_DRAW_SCREEN();
}

void pass_scene_forward(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(true, sceneTarget);
    R3D_SHADER_USE(scene.forward);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    R3D_SHADER_BIND_SAMPLER(scene.forward, uShadowDirTex, r3d_light_shadow_get(R3D_LIGHT_DIR));
    R3D_SHADER_BIND_SAMPLER(scene.forward, uShadowSpotTex, r3d_light_shadow_get(R3D_LIGHT_SPOT));
    R3D_SHADER_BIND_SAMPLER(scene.forward, uShadowOmniTex, r3d_light_shadow_get(R3D_LIGHT_OMNI));

    R3D_SHADER_BIND_SAMPLER(scene.forward, uIrradianceTex, r3d_env_irradiance_get());
    R3D_SHADER_BIND_SAMPLER(scene.forward, uPrefilterTex, r3d_env_prefilter_get());
    R3D_SHADER_BIND_SAMPLER(scene.forward, uBrdfLutTex, r3d_texture_get(R3D_TEXTURE_IBL_BRDF_LUT));

    R3D_SHADER_SET_VEC3(scene.forward, uViewPosition, R3D.viewState.viewPosition);

    const r3d_frustum_t* frustum = &R3D.viewState.frustum;
    R3D_DRAW_FOR_EACH(call, true, frustum, R3D_DRAW_LIST_PREPASS_INST, R3D_DRAW_LIST_PREPASS, R3D_DRAW_LIST_FORWARD_INST, R3D_DRAW_LIST_FORWARD) {
        upload_light_array_block_for_mesh(call, true);
        raster_forward(call);
    }
}

void pass_scene_background(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND(true, sceneTarget);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    if (R3D.environment.background.sky.texture != 0) {
        R3D_SHADER_USE(scene.skybox);
        glDisable(GL_CULL_FACE);

        float lod = (float)r3d_get_mip_levels_1d(R3D.environment.background.sky.size);

        R3D_SHADER_BIND_SAMPLER(scene.skybox, uSkyMap, R3D.environment.background.sky.texture);
        R3D_SHADER_SET_FLOAT(scene.skybox, uSkyEnergy, R3D.environment.background.energy);
        R3D_SHADER_SET_FLOAT(scene.skybox, uSkyLod, R3D.environment.background.skyBlur * lod);
        R3D_SHADER_SET_VEC4(scene.skybox, uRotation, R3D.environment.background.rotation);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatView, R3D.viewState.view);
        R3D_SHADER_SET_MAT4(scene.skybox, uMatProj, R3D.viewState.proj);

        R3D_DRAW_CUBE();
    }
    else {
        Vector4 background = r3d_color_to_linear_scaled_vec4(
            R3D.environment.background.color, R3D.colorSpace,
            R3D.environment.background.energy
        );
        R3D_SHADER_USE(scene.background);
        R3D_SHADER_SET_VEC4(scene.background, uColor, background);
        R3D_DRAW_SCREEN();
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

    R3D_SHADER_BIND_SAMPLER(post.fog, uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER(post.fog, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 0, 0));

    R3D_SHADER_SET_INT(post.fog, uFogMode, R3D.environment.fog.mode);
    R3D_SHADER_SET_COL3(post.fog, uFogColor, R3D.colorSpace, R3D.environment.fog.color);
    R3D_SHADER_SET_FLOAT(post.fog, uFogStart, R3D.environment.fog.start);
    R3D_SHADER_SET_FLOAT(post.fog, uFogEnd, R3D.environment.fog.end);
    R3D_SHADER_SET_FLOAT(post.fog, uFogDensity, R3D.environment.fog.density);
    R3D_SHADER_SET_FLOAT(post.fog, uSkyAffect, R3D.environment.fog.skyAffect);

    R3D_DRAW_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_dof(r3d_target_t sceneTarget)	
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.dof);

    R3D_SHADER_BIND_SAMPLER(post.dof, uSceneTex, r3d_target_get(sceneTarget));
    R3D_SHADER_BIND_SAMPLER(post.dof, uDepthTex, r3d_target_get_levels(R3D_TARGET_DEPTH, 0, 0));

    R3D_SHADER_SET_FLOAT(post.dof, uFocusPoint, R3D.environment.dof.focusPoint);
    R3D_SHADER_SET_FLOAT(post.dof, uFocusScale, R3D.environment.dof.focusScale);
    R3D_SHADER_SET_FLOAT(post.dof, uMaxBlurSize, R3D.environment.dof.maxBlurSize);
    R3D_SHADER_SET_INT(post.dof, uDebugMode, R3D.environment.dof.debugMode);

    R3D_DRAW_SCREEN();

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

    R3D_DRAW_SCREEN();

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

        R3D_DRAW_SCREEN();
    }

    /* --- Bloom Upsampling --- */

    R3D_SHADER_USE(prepare.bloomUp);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    R3D_SHADER_BIND_SAMPLER(prepare.bloomUp, uTexture, r3d_target_get(R3D_TARGET_BLOOM));

    for (int dstLevel = maxLevel - 2; dstLevel >= 0; dstLevel--)
    {
        r3d_target_set_viewport(R3D_TARGET_BLOOM, dstLevel);
        r3d_target_set_write_level(0, dstLevel);

        r3d_target_get_texel_size(&txSrcW, &txSrcH, R3D_TARGET_BLOOM, dstLevel + 1);
        R3D_SHADER_SET_FLOAT(prepare.bloomUp, uSrcLevel, dstLevel + 1);
        R3D_SHADER_SET_VEC2(prepare.bloomUp, uFilterRadius, (Vector2) {
            R3D.environment.bloom.filterRadius * txSrcW,
            R3D.environment.bloom.filterRadius * txSrcH
        });

        R3D_DRAW_SCREEN();
    }

    glDisable(GL_BLEND);

    /* --- Apply bloom to the scene --- */

    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.bloom);

    R3D_SHADER_BIND_SAMPLER(post.bloom, uSceneTex, sceneSourceID);
    R3D_SHADER_BIND_SAMPLER(post.bloom, uBloomTex, r3d_target_get(R3D_TARGET_BLOOM));

    R3D_SHADER_SET_INT(post.bloom, uBloomMode, R3D.environment.bloom.mode);
    R3D_SHADER_SET_FLOAT(post.bloom, uBloomIntensity, R3D.environment.bloom.intensity);

    R3D_DRAW_SCREEN();

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

    R3D_DRAW_SCREEN();

    return sceneTarget;
}

r3d_target_t pass_post_fxaa(r3d_target_t sceneTarget)
{
    R3D_TARGET_BIND_AND_SWAP_SCENE(sceneTarget);
    R3D_SHADER_USE(post.fxaa);

    R3D_SHADER_BIND_SAMPLER(post.fxaa, uSourceTex, r3d_target_get(sceneTarget));

    R3D_SHADER_SET_VEC2(post.fxaa, uSourceTexel, (Vector2) {R3D_TARGET_TEXEL_SIZE});
    R3D_DRAW_SCREEN();

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

        R3D_DRAW_SCREEN();

        r3d_target_blit(0, true, dstId, dstX, dstY, dstW, dstH, false);
        return;
    }

    if (smaller && R3D.downscaleMode == R3D_DOWNSCALE_BOX) {
        glBindFramebuffer(GL_FRAMEBUFFER, dstId);
        glViewport(dstX, dstY, dstW, dstH);

        R3D_SHADER_USE(prepare.blurDown);
        R3D_SHADER_SET_INT(prepare.blurDown, uSourceLod, 0);
        R3D_SHADER_BIND_SAMPLER(prepare.blurDown, uSourceTex, r3d_target_get(source));

        R3D_DRAW_SCREEN();

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

    R3D_DRAW_SCREEN();

    r3d_target_blit(0, true, dstId, dstX, dstY, dstW, dstH, false);
}

void reset_raylib_state(void)
{
    r3d_shader_unbind_samplers();
    r3d_target_reset();

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

    rlSetBlendMode(RL_BLEND_ALPHA);

    // Here we reset the target sampling levels to facilitate debugging with RenderDoc

#ifndef NDEBUG
    for (int iTarget = 0; iTarget < R3D_TARGET_COUNT; iTarget++) {
        if (r3d_target_get_or_null(iTarget) != 0) {
            r3d_target_set_read_levels(iTarget, 0, r3d_target_get_num_levels(iTarget) - 1);
        }
    }
#endif
}
