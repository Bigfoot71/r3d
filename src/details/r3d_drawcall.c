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

#include "../embedded/r3d_shaders.h"
#include "../r3d_state.h"

#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

/* === Internal functions === */

// This function supports instanced rendering when necessary
static void r3d_draw_vertex_arrays(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor);

// Comparison functions for sorting draw calls in the arrays
static int r3d_drawcall_compare_front_to_back(const void* a, const void* b);
static int r3d_drawcall_compare_back_to_front(const void* a, const void* b);


/* === Function definitions === */

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call)
{
    Matrix matModel = MatrixIdentity();
    Matrix matView = rlGetMatrixModelview();
    Matrix matModelView = MatrixIdentity();
    Matrix matProjection = rlGetMatrixProjection();

    // Compute model and model/view matrices
    matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    matModelView = MatrixMultiply(matModel, matView);

    // Set additional matrix uniforms
    if (call->instanced.count == 0) {
        r3d_shader_set_mat4(raster.geometry, uMatNormal, MatrixTranspose(MatrixInvert(matModel)));
    }
    r3d_shader_set_mat4(raster.geometry, uMatModel, matModel);

    // Set factor material maps
    r3d_shader_set_float(raster.geometry, uValEmission, call->material.maps[MATERIAL_MAP_EMISSION].value);
    r3d_shader_set_float(raster.geometry, uValOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].value);
    r3d_shader_set_float(raster.geometry, uValRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].value);
    r3d_shader_set_float(raster.geometry, uValMetalness, call->material.maps[MATERIAL_MAP_METALNESS].value);

    // Set color material maps
    r3d_shader_set_col3(raster.geometry, uColAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].color);
    r3d_shader_set_col3(raster.geometry, uColEmission, call->material.maps[MATERIAL_MAP_EMISSION].color);

    // Bind active texture maps
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexAlbedo, call->material.maps[MATERIAL_MAP_ALBEDO].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexNormal, call->material.maps[MATERIAL_MAP_NORMAL].texture.id, normal);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexEmission, call->material.maps[MATERIAL_MAP_EMISSION].texture.id, black);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexOcclusion, call->material.maps[MATERIAL_MAP_OCCLUSION].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexRoughness, call->material.maps[MATERIAL_MAP_ROUGHNESS].texture.id, white);
    r3d_shader_bind_sampler2D_opt(raster.geometry, uTexMetalness, call->material.maps[MATERIAL_MAP_METALNESS].texture.id, black);

    // Try binding vertex array objects (VAO) or use VBOs if not possible
    // WARNING: UploadMesh() enables all vertex attributes available in mesh and sets default attribute values
    // for shader expected vertex attributes that are not provided by the mesh (i.e. colors)
    // This could be a dangerous approach because different meshes with different shaders can enable/disable some attributes
    if (!rlEnableVertexArray(call->mesh.vaoId)) {
        // Bind mesh VBO - Positions
        rlEnableVertexBuffer(call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
        rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(0);

        // Bind mesh VBO - TexCoords
        rlEnableVertexBuffer(call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD]);
        rlSetVertexAttribute(1, 2, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(1);

        // Bind mesh VBO - Normals
        rlEnableVertexBuffer(call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL]);
        rlSetVertexAttribute(2, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(2);

        // Bind mesh VBO - Tagents
        if (call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT] != 0) {
            rlEnableVertexBuffer(call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT]);
            rlSetVertexAttribute(3, 4, RL_FLOAT, 0, 0, 0);
            rlEnableVertexAttribute(3);
        }
        else {
            // Set default value for defined vertex attribute in shader but not provided by mesh
            // WARNING: It could result in GPU undefined behaviour
            float value[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
            rlSetVertexAttributeDefault(3, value, SHADER_ATTRIB_VEC4, 4);
            rlDisableVertexAttribute(3);
        }

        // Bind mesh VBO - Colors
        if (call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR] != 0) {
            rlEnableVertexBuffer(call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR]);
            rlSetVertexAttribute(4, 4, RL_UNSIGNED_BYTE, 1, 0, 0);
            rlEnableVertexAttribute(4);
        }
        else {
            // Set default value for defined vertex attribute in shader but not provided by mesh
            // WARNING: It could result in GPU undefined behaviour
            float value[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            rlSetVertexAttributeDefault(4, value, SHADER_ATTRIB_VEC4, 4);
            rlDisableVertexAttribute(4);
        }

        if (call->mesh.indices != NULL) {
            rlEnableVertexBufferElement(call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
        }
    }

    int eyeCount = 1;
    if (rlIsStereoRenderEnabled()) eyeCount = 2;

    for (int eye = 0; eye < eyeCount; eye++) {
        // Calculate model-view-projection matrix (MVP)
        Matrix matModelViewProjection = MatrixIdentity();
        if (eyeCount == 1) {
            matModelViewProjection = MatrixMultiply(matModelView, matProjection);
        }
        else {
            // Setup current eye viewport (half screen width)
            rlViewport(eye * rlGetFramebufferWidth() / 2, 0, rlGetFramebufferWidth() / 2, rlGetFramebufferHeight());
            matModelViewProjection = MatrixMultiply(MatrixMultiply(matModelView, rlGetMatrixViewOffsetStereo(eye)), rlGetMatrixProjectionStereo(eye));
        }

        // Send combined model-view-projection matrix to shader
        r3d_shader_set_mat4(raster.geometry, uMatMVP, matModelViewProjection);

        // Rasterisation du mesh en tenant compte du rendu instanci� si besoin
        if (call->instanced.count == 0) {
            r3d_draw_vertex_arrays(call, -1, -1);
        }
        else {
            r3d_draw_vertex_arrays(call, 10, 14);
        }
    }

    // Unbind all bound texture maps
    r3d_shader_unbind_sampler2D(raster.geometry, uTexAlbedo);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexNormal);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexEmission);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexOcclusion);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexRoughness);
    r3d_shader_unbind_sampler2D(raster.geometry, uTexMetalness);

    // Disable all possible vertex array objects (or VBOs)
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();

    // Restore rlgl internal modelview and projection matrices
    rlSetMatrixModelview(matView);
    rlSetMatrixProjection(matProjection);
}

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call)
{
    Matrix matMVP = MatrixMultiply(call->transform, rlGetMatrixTransform());
    matMVP = MatrixMultiply(matMVP, rlGetMatrixModelview());
    matMVP = MatrixMultiply(matMVP, rlGetMatrixProjection());

    r3d_shader_set_mat4(raster.depth, uMatMVP, matMVP);

    if (!rlEnableVertexArray(call->mesh.vaoId)) {
        rlEnableVertexBuffer(call->mesh.vboId[0]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
        if (call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] > 0) {
            rlEnableVertexBufferElement(call->mesh.vboId[6]);
        }
    }

    if (call->instanced.count == 0) {
        r3d_draw_vertex_arrays(call, -1, -1);
    }
    else {
        r3d_draw_vertex_arrays(call, 10, -1);
    }

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();
}

void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call, Vector3 viewPos)
{
    Matrix matModel = MatrixMultiply(call->transform, rlGetMatrixTransform());
    Matrix matMVP = MatrixMultiply(matModel, rlGetMatrixModelview());
    matMVP = MatrixMultiply(matMVP, rlGetMatrixProjection());

    r3d_shader_set_vec3(raster.depthCube, uViewPosition, viewPos);
    r3d_shader_set_mat4(raster.depthCube, uMatModel, matModel);
    r3d_shader_set_mat4(raster.depthCube, uMatMVP, matMVP);

    if (!rlEnableVertexArray(call->mesh.vaoId)) {
        rlEnableVertexBuffer(call->mesh.vboId[0]);
        rlSetVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION, 3, RL_FLOAT, 0, 0, 0);
        rlEnableVertexAttribute(RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION);
        if (call->mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES] > 0) {
            rlEnableVertexBufferElement(call->mesh.vboId[6]);
        }
    }

    if (call->instanced.count == 0) {
        r3d_draw_vertex_arrays(call, -1, -1);
    }
    else {
        r3d_draw_vertex_arrays(call, 10, -1);
    }

    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();
}

void r3d_drawcall_sort_front_to_back(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_front_to_back);
}

void r3d_drawcall_sort_back_to_front(r3d_drawcall_t* calls, size_t count)
{
    qsort(calls, count, sizeof(r3d_drawcall_t), r3d_drawcall_compare_back_to_front);
}


/* === Internal functions === */

static void r3d_draw_vertex_arrays(const r3d_drawcall_t* call, int locInstanceModel, int locInstanceColor)
{
    // WARNING: Always use the same attribute locations in shaders for instance matrices and colors.
    // If attribute locations differ between shaders (e.g., between the depth shader and the geometry shader),
    // it will break the rendering. This is because the vertex attributes are assigned based on specific 
    // attribute locations, and if those locations are not consistent across shaders, the attributes 
    // for instance transforms and colors will not be correctly bound. 
    // This results in undefined or incorrect behavior, such as missing or incorrectly transformed meshes.

    unsigned int vboTransforms = 0;
    unsigned int vboColors = 0;

    // Determine whether instanced rendering is used
    bool useInstancing = (
        call->instanced.count > 0 && (call->instanced.transforms || call->instanced.colors)
    );

    // Enable the attribute for the transformation matrix (decomposed into 4 vec4 vectors)
    if (useInstancing && call->instanced.transforms && locInstanceModel >= 0) {
        vboTransforms = rlLoadVertexBuffer(call->instanced.transforms, call->instanced.count * sizeof(Matrix), true);
        rlEnableVertexBuffer(vboTransforms);
        for (int i = 0; i < 4; i++) {
            rlSetVertexAttribute(locInstanceModel + i, 4, RL_FLOAT, false, sizeof(Matrix), i * sizeof(Vector4));
            rlSetVertexAttributeDivisor(locInstanceModel + i, 1);
            rlEnableVertexAttribute(locInstanceModel + i);
        }
    }
    else if (locInstanceModel >= 0) {
        const float defaultTransform[4 * 4] = {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        };
        for (int i = 0; i < 4; i++) {
            glVertexAttrib4fv(locInstanceModel + i, defaultTransform + i * 4);
            rlDisableVertexAttribute(locInstanceModel + i);
        }
    }

    // Handle per-instance colors if available
    if (useInstancing && call->instanced.colors && locInstanceColor >= 0) {
        vboColors = rlLoadVertexBuffer(call->instanced.colors, call->instanced.count * sizeof(Color), true);
        rlEnableVertexBuffer(vboColors);
        rlSetVertexAttribute(locInstanceColor, 4, RL_UNSIGNED_BYTE, true, 0, 0);
        rlSetVertexAttributeDivisor(locInstanceColor, 1);
        rlEnableVertexAttribute(locInstanceColor);
    }
    else if (locInstanceColor >= 0) {
        const float defaultColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glVertexAttrib4fv(locInstanceColor, defaultColor);
        rlDisableVertexAttribute(locInstanceColor);
    }

    // Draw instances or a single object depending on the case
    if (useInstancing) {
        if (call->mesh.indices != NULL) {
            rlDrawVertexArrayElementsInstanced(0, call->mesh.triangleCount * 3, 0, call->instanced.count);
        }
        else {
            rlDrawVertexArrayInstanced(0, call->mesh.vertexCount, call->instanced.count);
        }
    }
    else {
        if (call->mesh.indices != NULL) {
            rlDrawVertexArrayElements(0, call->mesh.triangleCount * 3, 0);
        }
        else {
            rlDrawVertexArray(0, call->mesh.vertexCount);
        }
    }

    // Clean up resources
    if (vboTransforms > 0) {
        for (int i = 0; i < 4; i++) {
            rlSetVertexAttributeDivisor(locInstanceModel + i, 0);
            rlDisableVertexAttribute(locInstanceModel + i);
        }
        rlUnloadVertexBuffer(vboTransforms);
    }
    if (vboColors > 0) {
        rlSetVertexAttributeDivisor(locInstanceColor, 0);
        rlDisableVertexAttribute(locInstanceColor);
        rlUnloadVertexBuffer(vboColors);
    }
}

static int r3d_drawcall_compare_front_to_back(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    Vector3 posA = { 0 };
    Vector3 posB = { 0 };

    posA.x = drawCallA->transform.m12;
    posA.y = drawCallA->transform.m13;
    posA.z = drawCallA->transform.m14;

    posB.x = drawCallB->transform.m12;
    posB.y = drawCallB->transform.m13;
    posB.z = drawCallB->transform.m14;

    float distA = Vector3DistanceSqr(R3D.state.transform.position, posA);
    float distB = Vector3DistanceSqr(R3D.state.transform.position, posB);

    return (distA > distB) - (distA < distB);
}

static int r3d_drawcall_compare_back_to_front(const void* a, const void* b)
{
    const r3d_drawcall_t* drawCallA = a;
    const r3d_drawcall_t* drawCallB = b;

    Vector3 posA = { 0 };
    Vector3 posB = { 0 };

    posA.x = drawCallA->transform.m12;
    posA.y = drawCallA->transform.m13;
    posA.z = drawCallA->transform.m14;

    posB.x = drawCallB->transform.m12;
    posB.y = drawCallB->transform.m13;
    posB.z = drawCallB->transform.m14;

    float distA = Vector3DistanceSqr(R3D.state.transform.position, posA);
    float distB = Vector3DistanceSqr(R3D.state.transform.position, posB);

    return (distA < distB) - (distA > distB);
}
