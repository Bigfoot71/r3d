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

#include "../modules/r3d_texture.h"
#include "../modules/r3d_storage.h"
#include "../modules/r3d_shader.h"
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
static void r3d_drawcall_apply_depth_mode(R3D_DepthMode mode);
static void r3d_drawcall_apply_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode);
static GLenum r3d_drawcall_get_opengl_primitive(R3D_PrimitiveType primitive);

// This function supports instanced rendering when necessary
static void r3d_drawcall(const r3d_drawcall_t* call);
static void r3d_drawcall_instanced(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor);

// Comparison functions for sorting draw calls in the arrays
static int r3d_drawcall_compare_front_to_back(const void* a, const void* b);
static int r3d_drawcall_compare_back_to_front(const void* a, const void* b);

// Upload matrices function
static void r3d_drawcall_upload_matrices(const r3d_drawcall_t* call, int bindingSlot);

/* === Function definitions === */

void r3d_drawcall_sort_front_to_back(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_front_to_back);
}

void r3d_drawcall_sort_back_to_front(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_back_to_front);
}

bool r3d_drawcall_geometry_is_visible(const r3d_drawcall_t* call)
{
    if (r3d_matrix_is_identity(&call->transform)) {
        return r3d_frustum_is_aabb_in(&R3D.state.frustum.shape, &call->mesh.aabb);
    }
    return r3d_frustum_is_obb_in(&R3D.state.frustum.shape, &call->mesh.aabb, &call->transform);
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

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call, bool shadow, const Matrix* matVP)
{
    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depth, uMatModel, call->transform);
    R3D_SHADER_SET_MAT4(scene.depth, uMatVP, *matVP);

    /* --- Send skinning related data --- */

    if (call->player != NULL || R3D_IsSkeletonValid(&call->skeleton)) {
        r3d_drawcall_upload_matrices(call, R3D_SHADER_SLOT_SAMPLER_1D(scene.depth, uTexBoneMatrices));
        R3D_SHADER_SET_INT(scene.depth, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depth, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depth, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depth, uMatInvView, R3D.state.transform.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.depth, uTexCoordScale, call->material.uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.depth, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_SET_FLOAT(scene.depth, uAlpha, ((float)call->material.albedo.color.a / 255));
    R3D_SHADER_SET_FLOAT(scene.depth, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->mesh.shadowCastMode, call->material.cullMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material.cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    R3D_SHADER_SET_INT(scene.depth, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, -1);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind samplers --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.depth, uTexAlbedo);
}

void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call, bool shadow, const Matrix* matVP)
{
    /* --- Send matrices --- */

    R3D_SHADER_SET_MAT4(scene.depthCube, uMatModel, call->transform);
    R3D_SHADER_SET_MAT4(scene.depthCube, uMatVP, *matVP);

    /* --- Send skinning related data --- */

    if (call->player != NULL || R3D_IsSkeletonValid(&call->skeleton)) {
        r3d_drawcall_upload_matrices(call, R3D_SHADER_SLOT_SAMPLER_1D(scene.depthCube, uTexBoneMatrices));
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.depthCube, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.depthCube, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.depthCube, uMatInvView, R3D.state.transform.invView);
    }

    /* --- Set texcoord offset/scale --- */

    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordOffset, call->material.uvOffset);
    R3D_SHADER_SET_VEC2(scene.depthCube, uTexCoordScale, call->material.uvScale);

    /* --- Set transparency material data --- */

    R3D_SHADER_BIND_SAMPLER_2D(scene.depth, uTexAlbedo, R3D_TEXTURE_SELECT(call->material.albedo.texture.id, WHITE));
    R3D_SHADER_SET_FLOAT(scene.depth, uAlpha, ((float)call->material.albedo.color.a / 255));
    R3D_SHADER_SET_FLOAT(scene.depth, uAlphaCutoff, call->material.alphaCutoff);

    /* --- Applying material parameters that are independent of shaders --- */

    if (shadow) {
        r3d_drawcall_apply_shadow_cast_mode(call->mesh.shadowCastMode, call->material.cullMode);
    }
    else {
        r3d_drawcall_apply_cull_mode(call->material.cullMode);
    }

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    R3D_SHADER_SET_INT(scene.depthCube, uInstancing, instancing);

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

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.depthCube, uTexAlbedo);
}

void r3d_drawcall_raster_decal(const r3d_drawcall_t* call, const Matrix* matVP)
{
    /* --- Set additional matrix uniforms --- */

    Matrix matNormal = r3d_matrix_normal(&call->transform);

    R3D_SHADER_SET_MAT4(scene.decal, uMatModel, call->transform);
    R3D_SHADER_SET_MAT4(scene.decal, uMatNormal, matNormal);
    R3D_SHADER_SET_MAT4(scene.decal, uMatVP, *matVP);

    R3D_SHADER_SET_MAT4(scene.decal, uMatInvView, R3D.state.transform.invView);
    R3D_SHADER_SET_MAT4(scene.decal, uMatInvProj, R3D.state.transform.invProj);
    R3D_SHADER_SET_MAT4(scene.decal, uMatProj, R3D.state.transform.proj);

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

    r3d_drawcall_apply_blend_mode(call->material.blendMode);

    /* --- Disable face culling to avoid issues when camera is inside the decal bounding mesh --- */
    // TODO: Implement check for if camera is inside the mesh and apply the appropriate face culling / depth testing

    glDisable(GL_CULL_FACE);

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    R3D_SHADER_SET_INT(scene.decal, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, -1);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.decal, uTexORM);
}

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call, const Matrix* matVP)
{
    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&call->transform);

    R3D_SHADER_SET_MAT4(scene.geometry, uMatModel, call->transform);
    R3D_SHADER_SET_MAT4(scene.geometry, uMatNormal, matNormal);
    R3D_SHADER_SET_MAT4(scene.geometry, uMatVP, *matVP);

    /* --- Send skinning related data --- */

    if (call->player != NULL || R3D_IsSkeletonValid(&call->skeleton)) {
        r3d_drawcall_upload_matrices(call, R3D_SHADER_SLOT_SAMPLER_1D(scene.geometry, uTexBoneMatrices));
        R3D_SHADER_SET_INT(scene.geometry, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.geometry, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.geometry, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.geometry, uMatInvView, R3D.state.transform.invView);
    }

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

    r3d_drawcall_apply_cull_mode(call->material.cullMode);

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    R3D_SHADER_SET_INT(scene.geometry, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, 14);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.geometry, uTexORM);
}

void r3d_drawcall_raster_forward(const r3d_drawcall_t* call, const Matrix* matVP)
{
    /* --- Send matrices --- */

    Matrix matNormal = r3d_matrix_normal(&call->transform);

    R3D_SHADER_SET_MAT4(scene.forward, uMatModel, call->transform);
    R3D_SHADER_SET_MAT4(scene.forward, uMatNormal, matNormal);
    R3D_SHADER_SET_MAT4(scene.forward, uMatVP, *matVP);

    /* --- Send skinning related data --- */

    if (call->player != NULL || R3D_IsSkeletonValid(&call->skeleton)) {
        r3d_drawcall_upload_matrices(call, R3D_SHADER_SLOT_SAMPLER_1D(scene.forward, uTexBoneMatrices));
        R3D_SHADER_SET_INT(scene.forward, uSkinning, true);
    }
    else {
        R3D_SHADER_SET_INT(scene.forward, uSkinning, false);
    }

    /* --- Send billboard related data --- */

    R3D_SHADER_SET_INT(scene.forward, uBillboard, call->material.billboardMode);
    if (call->material.billboardMode != R3D_BILLBOARD_DISABLED) {
        R3D_SHADER_SET_MAT4(scene.forward, uMatInvView, R3D.state.transform.invView);
    }

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

    r3d_drawcall_apply_cull_mode(call->material.cullMode);
    r3d_drawcall_apply_blend_mode(call->material.blendMode);

    /* --- Rendering the object corresponding to the draw call --- */

    bool instancing = (call->instanced.count > 0 && call->instanced.transforms);
    R3D_SHADER_SET_INT(scene.forward, uInstancing, instancing);

    if (instancing) {
        r3d_drawcall_instanced(call, 10, 14);
    }
    else {
        r3d_drawcall(call);
    }

    /* --- Unbind all bound texture maps --- */

    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexAlbedo);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexNormal);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexEmission);
    R3D_SHADER_UNBIND_SAMPLER_2D(scene.forward, uTexORM);
}

/* === Internal functions === */

void r3d_drawcall_apply_cull_mode(R3D_CullMode mode)
{
    switch (mode) {
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
    switch (mode) {
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
    case R3D_BLEND_PREMULTIPLIED_ALPHA:
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    default:
        break;
    }
}

void r3d_drawcall_apply_depth_mode(R3D_DepthMode mode)
{
    switch (mode) {
    case R3D_DEPTH_DISABLED:
        glDisable(GL_DEPTH_TEST);
        break;
    case R3D_DEPTH_READ_ONLY:
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        break;
    default:
    case R3D_DEPTH_READ_WRITE:
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        break;
    }
}

static void r3d_drawcall_apply_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode)
{
    switch (castMode) {
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

GLenum r3d_drawcall_get_opengl_primitive(R3D_PrimitiveType primitive)
{
    switch (primitive) {
    case R3D_PRIMITIVE_POINTS:          return GL_POINTS;
    case R3D_PRIMITIVE_LINES:           return GL_LINES;
    case R3D_PRIMITIVE_LINE_STRIP:      return GL_LINE_STRIP;
    case R3D_PRIMITIVE_LINE_LOOP:       return GL_LINE_LOOP;
    case R3D_PRIMITIVE_TRIANGLES:       return GL_TRIANGLES;
    case R3D_PRIMITIVE_TRIANGLE_STRIP:  return GL_TRIANGLE_STRIP;
    case R3D_PRIMITIVE_TRIANGLE_FAN:    return GL_TRIANGLE_FAN;
    default: break;
    }

    return GL_TRIANGLES; // consider an error...
}

void r3d_drawcall(const r3d_drawcall_t* call)
{
    GLenum primitive = r3d_drawcall_get_opengl_primitive(call->mesh.primitiveType);
    r3d_drawcall_apply_depth_mode(call->material.depthMode);

    glBindVertexArray(call->mesh.vao);
    if (call->mesh.ebo == 0) glDrawArrays(primitive, 0, call->mesh.vertexCount);
    else glDrawElements(primitive, call->mesh.indexCount, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);
}

void r3d_drawcall_instanced(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor)
{
    // NOTE: All this mess here will be reviewed with the instance buffers

    r3d_drawcall_apply_depth_mode(call->material.depthMode);

    glBindVertexArray(call->mesh.vao);

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
    if (call->mesh.ebo == 0) {
        glDrawArraysInstanced(
            r3d_drawcall_get_opengl_primitive(call->mesh.primitiveType),
            0, call->mesh.vertexCount, (int)call->instanced.count
        );
    }
    else {
        glDrawElementsInstanced(
            r3d_drawcall_get_opengl_primitive(call->mesh.primitiveType),
            call->mesh.indexCount, GL_UNSIGNED_INT, NULL, (int)call->instanced.count
        );
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

    glBindVertexArray(0);
}

// Helper function to calculate AABB center distance in view space
static float r3d_drawcall_calculate_center_distance_to_camera(const r3d_drawcall_t* drawCall)
{
    Vector3 center = {
        (drawCall->mesh.aabb.min.x + drawCall->mesh.aabb.max.x) * 0.5f,
        (drawCall->mesh.aabb.min.y + drawCall->mesh.aabb.max.y) * 0.5f,
        (drawCall->mesh.aabb.min.z + drawCall->mesh.aabb.max.z) * 0.5f
    };
    center = Vector3Transform(center, drawCall->transform);

    return Vector3DistanceSqr(R3D.state.transform.viewPos, center);
}

// Helper function to calculate maximum AABB corner distance in view space
static float r3d_drawcall_calculate_max_distance_to_camera(const r3d_drawcall_t* drawCall)
{
    Vector3 corners[8] = {
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.max.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.max.z},
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.max.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.max.z}
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

// Upload matrices function
static void r3d_drawcall_upload_matrices(const r3d_drawcall_t* call, int bindingSlot)
{
    const R3D_Skeleton* skeleton = NULL;
    const Matrix* currentPose = NULL;

    if (call->player != NULL) {
        skeleton = &call->player->skeleton;
        currentPose = call->player->currentPose;
    }
    else {
        skeleton = &call->skeleton;
        currentPose = call->skeleton.bindPose;
    }

    static Matrix bones[R3D_STORAGE_MAX_BONE_MATRICES];
    r3d_matrix_multiply_batch(bones, skeleton->boneOffsets, currentPose, skeleton->boneCount);
    r3d_storage_use(R3D_STORAGE_BONE_MATRICES, bindingSlot, bones, skeleton->boneCount);
}
