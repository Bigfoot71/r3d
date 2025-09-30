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

#include "./r3d_drawcall.h"

#include "./r3d_primitives.h"
#include "./r3d_frustum.h"
#include "../r3d_state.h"
#include "./r3d_math.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include <stdlib.h>
#include <assert.h>
#include <float.h>

/* === Internal functions === */

// Functions applying OpenGL states defined by the material but unrelated to shaders
static void r3d_drawcall_apply_cull_mode(R3D_CullMode mode);
static void r3d_drawcall_apply_blend_mode(R3D_BlendMode mode);
static void r3d_drawcall_apply_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode);

// This function supports instanced rendering when necessary
static void r3d_drawcall(const r3d_drawcall_t* call);
static void r3d_drawcall_instanced(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor);

// Comparison functions for sorting draw calls in the arrays
static int r3d_drawcall_compare_front_to_back(const void* a, const void* b);
static int r3d_drawcall_compare_back_to_front(const void* a, const void* b);
static int r3d_drawcall_compare_mixed_forward(const void* a, const void* b);

// Upload matrices function
static void r3d_drawcall_upload_matrices(const r3d_drawcall_t* call);

/* === Function definitions === */

void r3d_drawcall_sort_front_to_back(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_front_to_back);
}

void r3d_drawcall_sort_back_to_front(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_back_to_front);
}

void r3d_drawcall_sort_mixed_forward(r3d_drawcall_t* calls, size_t count)
{
    // Sort objects forward in case the array can contain both opaque and transparent objects
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_mixed_forward);
}

bool r3d_drawcall_geometry_is_visible(const r3d_drawcall_t* call)
{
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MODEL) {
        if (r3d_matrix_is_identity(&call->transform)) {
            return r3d_frustum_is_aabb_in(&R3D.state.frustum.shape, &call->geometry.model.mesh->aabb);
        }
        return r3d_frustum_is_obb_in(&R3D.state.frustum.shape, &call->geometry.model.mesh->aabb, &call->transform);
    }

    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        return r3d_frustum_is_points_in(&R3D.state.frustum.shape, call->geometry.sprite.quad, 4);
    }

    return false;
}

bool r3d_drawcall_instanced_geometry_is_visible(const r3d_drawcall_t* call)
{
    if (call->instanced.allAabb.min.x == -FLT_MAX) {
        return true;
    }

    if (r3d_matrix_is_identity(&call->transform)) {
        return r3d_frustum_is_aabb_in(&R3D.state.frustum.shape, &call->instanced.allAabb);
    }

    return r3d_frustum_is_obb_in(&R3D.state.frustum.shape, &call->instanced.allAabb, &call->transform);
}

void r3d_drawcall_update_model_animation(const r3d_drawcall_t* call)
{
    int frame = call->geometry.model.frame;
    if (frame >= call->geometry.model.anim->frameCount) {
        frame = frame % call->geometry.model.anim->frameCount;
    }

    r3d_matrix_multiply_batch(
        call->geometry.model.mesh->boneMatrices,
        call->geometry.model.boneOffsets,
        call->geometry.model.anim->frameGlobalPoses[frame],
        call->geometry.model.anim->boneCount
    );
}

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call, bool forward, bool shadow, const Matrix* matVP)
{
    /* --- Send matrices --- */

    r3d_shader_set_mat4(raster.depth, uMatModel, call->transform);
    r3d_shader_set_mat4(raster.depth, uMatVP, *matVP);

    /* --- Send billboard related data --- */

    r3d_shader_set_int(raster.depth, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.depth, uMatInvView, R3D.state.transform.invView);
    }

    /* --- Set texcoord offset/scale --- */

    r3d_shader_set_vec2(raster.depth, uTexCoordOffset, call->material.uvOffset);
    r3d_shader_set_vec2(raster.depth, uTexCoordScale, call->material.uvScale);

    /* --- Setup geometry type related uniforms --- */

    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        {
            // Send bone matrices and animation related data
            if (call->geometry.model.anim != NULL && call->geometry.model.boneOffsets != NULL) {
                r3d_shader_set_int(raster.depth, uSkinning, true);
                r3d_drawcall_upload_matrices(call);
            }
            else {
                r3d_shader_set_int(raster.depth, uSkinning, false);
            }
        }
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        {
            // Send bone matrices and animation related data
            r3d_shader_set_int(raster.depth, uSkinning, false);
        }
        break;
    }

    /* --- Set forward material data --- */

    if (forward) {
        r3d_shader_set_float(raster.depth, uAlphaCutoff, call->material.alphaCutoff);
        r3d_shader_set_float(raster.depth, uAlpha, ((float)call->material.albedo.color.a / 255));
        r3d_shader_bind_sampler2D_opt(raster.depth, uTexAlbedo, call->material.albedo.texture.id, white);
    }
    else {
        r3d_shader_set_float(raster.depth, uAlpha, 1.0f);
        r3d_shader_set_float(raster.depth, uAlphaCutoff, 0.0f);
        r3d_shader_bind_sampler2D(raster.depth, uTexAlbedo, R3D.texture.white);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->shadowCastMode, call->material.cullMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material.cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    r3d_shader_set_int(raster.depth, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, -1);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind samplers --- */

    r3d_shader_unbind_sampler2D(raster.depth, uTexAlbedo);
}

void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call, bool forward, bool shadow, const Matrix* matVP)
{
    /* --- Send matrices --- */

    r3d_shader_set_mat4(raster.depthCube, uMatModel, call->transform);
    r3d_shader_set_mat4(raster.depthCube, uMatVP, *matVP);

    /* --- Send billboard related data --- */

    r3d_shader_set_int(raster.depthCube, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.depthCube, uMatInvView, R3D.state.transform.invView);
    }

    /* --- Set texcoord offset/scale --- */

    r3d_shader_set_vec2(raster.depthCube, uTexCoordOffset, call->material.uvOffset);
    r3d_shader_set_vec2(raster.depthCube, uTexCoordScale, call->material.uvScale);

    /* --- Setup geometry type related uniforms --- */

    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        {
            // Send bone matrices and animation related data
            if (call->geometry.model.anim != NULL && call->geometry.model.boneOffsets != NULL) {
                r3d_shader_set_int(raster.depthCube, uSkinning, true);
                r3d_drawcall_upload_matrices(call);
            }
            else {
                r3d_shader_set_int(raster.depthCube, uSkinning, false);
            }
        }
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        {
            // Send bone matrices and animation related data
            r3d_shader_set_int(raster.depthCube, uSkinning, false);
        }
        break;
    }

    /* --- Set forward material data --- */

    if (forward) {
        r3d_shader_set_float(raster.depthCube, uAlphaCutoff, call->material.alphaCutoff);
        r3d_shader_set_float(raster.depthCube, uAlpha, ((float)call->material.albedo.color.a / 255));
        r3d_shader_bind_sampler2D_opt(raster.depthCube, uTexAlbedo, call->material.albedo.texture.id, white);
    }
    else {
        r3d_shader_set_float(raster.depthCube, uAlpha, 1.0f);
        r3d_shader_set_float(raster.depthCube, uAlphaCutoff, 0.0f);
        r3d_shader_bind_sampler2D(raster.depthCube, uTexAlbedo, R3D.texture.white);
    }

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->shadowCastMode, call->material.cullMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material.cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    r3d_shader_set_int(raster.depthCube, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, -1);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind vertex buffers --- */

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    /* --- Unbind samplers --- */

    r3d_shader_unbind_sampler2D(raster.depthCube, uTexAlbedo);
}

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call, const Matrix* matVP)
{
    /* --- Set additional matrix uniforms --- */

    Matrix matNormal = r3d_matrix_normal(&call->transform);

    r3d_shader_set_mat4(raster.geometry, uMatModel, call->transform);
    r3d_shader_set_mat4(raster.geometry, uMatNormal, matNormal);
    r3d_shader_set_mat4(raster.geometry, uMatVP, *matVP);

    /* --- Set factor material maps --- */

    r3d_shader_set_float(raster.geometry, uEmissionEnergy, call->material.emission.energy);
    r3d_shader_set_float(raster.geometry, uNormalScale, call->material.normal.scale);
    r3d_shader_set_float(raster.geometry, uOcclusion, call->material.orm.occlusion);
    r3d_shader_set_float(raster.geometry, uRoughness, call->material.orm.roughness);
    r3d_shader_set_float(raster.geometry, uMetalness, call->material.orm.metalness);

    /* --- Set misc material values --- */

    r3d_shader_set_float(raster.geometry, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    r3d_shader_set_vec2(raster.geometry, uTexCoordOffset, call->material.uvOffset);
    r3d_shader_set_vec2(raster.geometry, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    r3d_shader_set_col4(raster.geometry, uAlbedoColor, call->material.albedo.color);
    r3d_shader_set_col3(raster.geometry, uEmissionColor, call->material.emission.color);

    /* --- Setup billboard mode --- */

    r3d_shader_set_int(raster.geometry, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.geometry, uMatInvView, R3D.state.transform.invView);
    }

    /* --- Setup geometry type related uniforms --- */

    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        {
            // Send bone matrices and animation related data
            if (call->geometry.model.anim != NULL && call->geometry.model.boneOffsets != NULL) {
                r3d_shader_set_int(raster.geometry, uSkinning, true);
                r3d_drawcall_upload_matrices(call);
            }
            else {
                r3d_shader_set_int(raster.geometry, uSkinning, false);
            }
        }
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        {
            // Send bone matrices and animation related data
            r3d_shader_set_int(raster.geometry, uSkinning, false);
        }
        break;
    }

    /* --- Bind active texture maps --- */

    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexAlbedo, call->material.albedo.texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexNormal, call->material.normal.texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexEmission, call->material.emission.texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexORM, call->material.orm.texture.id, white);

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_drawcall_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    r3d_shader_set_int(raster.geometry, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, 14);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind all bound texture maps --- */

    r3d_shader_unbind_sampler2D(raster.geometry, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexORM);
}

void r3d_drawcall_raster_forward(const r3d_drawcall_t* call, const Matrix* matVP)
{
    /* --- Set additional matrix uniforms --- */

    Matrix matNormal = r3d_matrix_normal(&call->transform);

    r3d_shader_set_mat4(raster.forward, uMatModel, call->transform);
    r3d_shader_set_mat4(raster.forward, uMatNormal, matNormal);
    r3d_shader_set_mat4(raster.forward, uMatVP, *matVP);

    /* --- Set factor material maps --- */

    r3d_shader_set_float(raster.forward, uEmissionEnergy, call->material.emission.energy);
    r3d_shader_set_float(raster.forward, uNormalScale, call->material.normal.scale);
    r3d_shader_set_float(raster.forward, uOcclusion, call->material.orm.occlusion);
    r3d_shader_set_float(raster.forward, uRoughness, call->material.orm.roughness);
    r3d_shader_set_float(raster.forward, uMetalness, call->material.orm.metalness);

    /* --- Set misc material values --- */

    r3d_shader_set_float(raster.forward, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Set texcoord offset/scale --- */

    r3d_shader_set_vec2(raster.forward, uTexCoordOffset, call->material.uvOffset);
    r3d_shader_set_vec2(raster.forward, uTexCoordScale, call->material.uvScale);

    /* --- Set color material maps --- */

    r3d_shader_set_col4(raster.forward, uAlbedoColor, call->material.albedo.color);
    r3d_shader_set_col3(raster.forward, uEmissionColor, call->material.emission.color);

    /* --- Setup billboard mode --- */

    r3d_shader_set_int(raster.forward, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        r3d_shader_set_mat4(raster.forward, uMatInvView, R3D.state.transform.invView);
    }

    /* --- Setup geometry type related uniforms --- */

    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        {
            // Send bone matrices and animation related data
            if (call->geometry.model.anim != NULL && call->geometry.model.boneOffsets != NULL) {
                r3d_shader_set_int(raster.forward, uSkinning, true);
                r3d_drawcall_upload_matrices(call);
            }
            else {
                r3d_shader_set_int(raster.forward, uSkinning, false);
            }
        }
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        {
            // Send bone matrices and animation related data
            r3d_shader_set_int(raster.forward, uSkinning, false);
        }
        break;
    }

    /* --- Bind active texture maps --- */

    r3d_shader_bind_sampler2D_opt(raster.forward, uTexAlbedo, call->material.albedo.texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexNormal, call->material.normal.texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexEmission, call->material.emission.texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.forward, uTexORM, call->material.orm.texture.id, white);

    /* --- Applying material parameters that are independent of shaders --- */

    r3d_drawcall_apply_cull_mode(call->material.cullMode);
    r3d_drawcall_apply_blend_mode(call->material.blendMode);

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    r3d_shader_set_int(raster.forward, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, 14);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind all bound texture maps --- */

    r3d_shader_unbind_sampler2D(raster.forward, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.forward, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.forward, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.forward, uTexORM);
}

/* === Internal functions === */

void r3d_drawcall_apply_cull_mode(R3D_CullMode mode)
{
    switch (mode)
    {
    case R3D_CULL_NONE:
        glDisable(GL_CULL_FACE);
        break;
    case R3D_CULL_BACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_CULL_FRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    }
}

void r3d_drawcall_apply_blend_mode(R3D_BlendMode mode)
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

static void r3d_drawcall_apply_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode)
{
    switch (castMode)
    {
    case R3D_SHADOW_CAST_ON_AUTO:
        r3d_drawcall_apply_cull_mode(cullMode);
        break;
    case R3D_SHADOW_CAST_ON_DOUBLE_SIDED:
        glDisable(GL_CULL_FACE);
        break;
    case R3D_SHADOW_CAST_ON_FRONT_SIDE:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_SHADOW_CAST_ON_BACK_SIDE:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    case R3D_SHADOW_CAST_ONLY_AUTO:
        r3d_drawcall_apply_cull_mode(cullMode);
        break;
    case R3D_SHADOW_CAST_ONLY_DOUBLE_SIDED:
        glDisable(GL_CULL_FACE);
        break;
    case R3D_SHADOW_CAST_ONLY_FRONT_SIDE:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_SHADOW_CAST_ONLY_BACK_SIDE:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    case R3D_SHADOW_CAST_DISABLED:
    default:
        assert("This shouldn't happen" && false);
        break;
    }
}

static void r3d_drawcall_bind_geometry_mesh(const R3D_Mesh* mesh)
{
    if (rlEnableVertexArray(mesh->vao)) {
        return;
    }

    // Enable the vertex buffer (fallback if vao is not available)
    rlEnableVertexBuffer(mesh->vbo);

    // Bind positions
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, position));
    rlEnableVertexAttribute(0);

    // Bind texcoords
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, texcoord));
    rlEnableVertexAttribute(1);

    // Bind normals
    rlSetVertexAttribute(2, 3, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, normal));
    rlEnableVertexAttribute(2);

    // Bind colors
    rlSetVertexAttribute(3, 4, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, color));
    rlEnableVertexAttribute(3);

    // Bind tangents
    rlSetVertexAttribute(4, 4, RL_FLOAT, false, sizeof(R3D_Vertex), offsetof(R3D_Vertex, tangent));
    rlEnableVertexAttribute(4);

    // Bind index buffer
    if (mesh->ebo > 0) {
        rlEnableVertexBufferElement(mesh->ebo);
    }
}

static void r3d_drawcall_unbind_geometry_mesh(void)
{
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();
}

void r3d_drawcall(const r3d_drawcall_t* call)
{
    if (call->geometryType == R3D_DRAWCALL_GEOMETRY_MODEL) {
        r3d_drawcall_bind_geometry_mesh(call->geometry.model.mesh);
        if (call->geometry.model.mesh->indices == NULL) {
            glDrawArrays(GL_TRIANGLES, 0, call->geometry.model.mesh->vertexCount);
        }
        else {
            glDrawElements(GL_TRIANGLES, call->geometry.model.mesh->indexCount, GL_UNSIGNED_INT, NULL);
        }
        r3d_drawcall_unbind_geometry_mesh();
    }

    // Sprite mode only requires to render a generic quad
    else if (call->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        r3d_primitive_bind_and_draw_quad();
    }
}

void r3d_drawcall_instanced(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor)
{
    // Bind the geometry
    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        r3d_drawcall_bind_geometry_mesh(call->geometry.model.mesh);
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        r3d_primitive_bind(&R3D.primitive.quad);
        break;
    }

    unsigned int vboTransforms = 0;
    unsigned int vboColors = 0;

    // Enable the attribute for the transformation matrix (decomposed into 4 vec4 vectors)
    if (locInstanceModel >= 0 && call->instanced.transforms) {
        size_t stride = (call->instanced.transStride == 0) ? sizeof(Matrix) : call->instanced.transStride;
        vboTransforms = rlLoadVertexBuffer(call->instanced.transforms, (int)(call->instanced.count * stride), true);
        rlEnableVertexBuffer(vboTransforms);
        for (int i = 0; i < 4; i++) {
            rlSetVertexAttribute(locInstanceModel + i, 4, RL_FLOAT, false, (int)stride, i * sizeof(Vector4));
            rlSetVertexAttributeDivisor(locInstanceModel + i, 1);
            rlEnableVertexAttribute(locInstanceModel + i);
        }
    }

    // Handle per-instance colors if available
    if (locInstanceColor >= 0 && call->instanced.colors) {
        size_t stride = (call->instanced.colStride == 0) ? sizeof(Color) : call->instanced.colStride;
        vboColors = rlLoadVertexBuffer(call->instanced.colors, (int)(call->instanced.count * stride), true);
        rlEnableVertexBuffer(vboColors);
        rlSetVertexAttribute(locInstanceColor, 4, RL_UNSIGNED_BYTE, true, (int)call->instanced.colStride, 0);
        rlSetVertexAttributeDivisor(locInstanceColor, 1);
        rlEnableVertexAttribute(locInstanceColor);
    }

    // Draw the geometry
    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        if (call->geometry.model.mesh->indices == NULL) {
            glDrawArraysInstanced(GL_TRIANGLES, 0, call->geometry.model.mesh->vertexCount, (int)call->instanced.count);
        }
        else {
            glDrawElementsInstanced(GL_TRIANGLES, call->geometry.model.mesh->indexCount, GL_UNSIGNED_INT, NULL, (int)call->instanced.count);
        }
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        r3d_primitive_draw_instanced(&R3D.primitive.quad, (int)call->instanced.count);
        break;
    }

    // Clean up instanced data
    if (vboTransforms > 0) {
        for (int i = 0; i < 4; i++) {
            rlDisableVertexAttribute(locInstanceModel + i);
            rlSetVertexAttributeDivisor(locInstanceModel + i, 0);
        }
        rlUnloadVertexBuffer(vboTransforms);
    }
    if (vboColors > 0) {
        rlDisableVertexAttribute(locInstanceColor);
        rlSetVertexAttributeDivisor(locInstanceColor, 0);
        rlUnloadVertexBuffer(vboColors);
    }

    // Unbind the geometry
    switch (call->geometryType) {
    case R3D_DRAWCALL_GEOMETRY_MODEL:
        r3d_drawcall_unbind_geometry_mesh();
        break;
    case R3D_DRAWCALL_GEOMETRY_SPRITE:
        r3d_primitive_unbind();
        break;
    }
}

// Helper function to calculate AABB center distance in view space
static float r3d_drawcall_calculate_center_distance_to_camera(const r3d_drawcall_t* drawCall)
{
    Vector3 center = { 0 };
    if (drawCall->geometryType == R3D_DRAWCALL_GEOMETRY_MODEL) {
        center.x = (drawCall->geometry.model.mesh->aabb.min.x + drawCall->geometry.model.mesh->aabb.max.x) * 0.5f;
        center.y = (drawCall->geometry.model.mesh->aabb.min.y + drawCall->geometry.model.mesh->aabb.max.y) * 0.5f;
        center.z = (drawCall->geometry.model.mesh->aabb.min.z + drawCall->geometry.model.mesh->aabb.max.z) * 0.5f;
    }
    else if (drawCall->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        center.x = drawCall->transform.m12;
        center.y = drawCall->transform.m13;
        center.z = drawCall->transform.m14;
    }

    center = Vector3Transform(center, drawCall->transform);
    return Vector3DistanceSqr(R3D.state.transform.viewPos, center);
}

// Helper function to calculate maximum AABB corner distance in view space
static float r3d_drawcall_calculate_max_distance_to_camera(const r3d_drawcall_t* drawCall)
{
    if (drawCall->geometryType == R3D_DRAWCALL_GEOMETRY_SPRITE) {
        Vector3 center = { drawCall->transform.m12, drawCall->transform.m13, drawCall->transform.m14 };
        return Vector3DistanceSqr(R3D.state.transform.viewPos, center);
    }

    Vector3 corners[8] = {
        {drawCall->geometry.model.mesh->aabb.min.x, drawCall->geometry.model.mesh->aabb.min.y, drawCall->geometry.model.mesh->aabb.min.z},
        {drawCall->geometry.model.mesh->aabb.max.x, drawCall->geometry.model.mesh->aabb.min.y, drawCall->geometry.model.mesh->aabb.min.z},
        {drawCall->geometry.model.mesh->aabb.min.x, drawCall->geometry.model.mesh->aabb.max.y, drawCall->geometry.model.mesh->aabb.min.z},
        {drawCall->geometry.model.mesh->aabb.max.x, drawCall->geometry.model.mesh->aabb.max.y, drawCall->geometry.model.mesh->aabb.min.z},
        {drawCall->geometry.model.mesh->aabb.min.x, drawCall->geometry.model.mesh->aabb.min.y, drawCall->geometry.model.mesh->aabb.max.z},
        {drawCall->geometry.model.mesh->aabb.max.x, drawCall->geometry.model.mesh->aabb.min.y, drawCall->geometry.model.mesh->aabb.max.z},
        {drawCall->geometry.model.mesh->aabb.min.x, drawCall->geometry.model.mesh->aabb.max.y, drawCall->geometry.model.mesh->aabb.max.z},
        {drawCall->geometry.model.mesh->aabb.max.x, drawCall->geometry.model.mesh->aabb.max.y, drawCall->geometry.model.mesh->aabb.max.z}
    };

    float maxDistSq = 0.0f;
    for (int i = 0; i < 8; ++i) {
        Vector3 corner = Vector3Transform(corners[i], drawCall->transform);
        float distSq = Vector3DistanceSqr(R3D.state.transform.viewPos, corner);
        if (distSq > maxDistSq) maxDistSq = distSq;
    }
    return maxDistSq;
}

// Comparison function for opaque objects (front-to-back, using center distance)
int r3d_drawcall_compare_front_to_back(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    // REVIEW: Would sorting the closest AABB corners first be better?
    float distA = r3d_drawcall_calculate_center_distance_to_camera(drawCallA);
    float distB = r3d_drawcall_calculate_center_distance_to_camera(drawCallB);

    // Front-to-back: smaller distance first
    return (distA > distB) - (distA < distB);
}

// Comparison function for transparent objects (back-to-front, using max distance with fallback)
int r3d_drawcall_compare_back_to_front(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    float maxDistA = r3d_drawcall_calculate_max_distance_to_camera(drawCallA);
    float maxDistB = r3d_drawcall_calculate_max_distance_to_camera(drawCallB);

    return (maxDistA < maxDistB) - (maxDistA > maxDistB);
}

// Comparison function for forward objects (opaque/transparent mixed, front-to-back or back-to-front)
int r3d_drawcall_compare_mixed_forward(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    // Assume there's a way to check if a drawcall is opaque
    // You'll need to implement this based on your blend mode system
    bool isOpaqueA = (drawCallA->material.blendMode == R3D_BLEND_OPAQUE);
    bool isOpaqueB = (drawCallB->material.blendMode == R3D_BLEND_OPAQUE);

    // If one is opaque and the other is not, opaque comes first
    if (isOpaqueA && !isOpaqueB) return -1;
    if (!isOpaqueA && isOpaqueB) return +1;

    // If both are opaque, sort front-to-back
    if (isOpaqueA && isOpaqueB) {
        return r3d_drawcall_compare_front_to_back(a, b);
    }

    // If both are transparent, sort back-to-front
    return r3d_drawcall_compare_back_to_front(a, b);
}

// Upload matrices function
static void r3d_drawcall_upload_matrices(const r3d_drawcall_t* call)
{
    // WARNING: Pay attention to any changes in the binding slot for uTexBoneMatrices.
    //          In theory, being the only texture sampled in the vertex shader,
    //          it should be kept in the first slot '0' for consistency.

    const int bindingSlot = 0;

    if (call->geometry.model.boneOverride == NULL) {
        r3d_storage_bind_and_upload_matrices(
            call->geometry.model.mesh->boneMatrices,
            call->geometry.model.mesh->boneCount,
            bindingSlot
        );
    }
    else {
        r3d_storage_bind_and_upload_matrices(
            call->geometry.model.boneOverride,
            call->geometry.model.anim->boneCount,
            bindingSlot
        );
    }
}
