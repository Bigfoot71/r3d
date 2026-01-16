/* r3d_mesh.h -- R3D Mesh Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_mesh_data.h>
#include <r3d/r3d_mesh.h>
#include <r3d_config.h>
#include <stdint.h>
#include <stddef.h>
#include <glad.h>

// ========================================
// PUBLIC API
// ========================================

R3D_Mesh R3D_LoadMesh(R3D_PrimitiveType type, R3D_MeshData data, const BoundingBox* aabb, R3D_MeshUsage usage)
{
    R3D_Mesh mesh = { 0 };

    if (data.vertexCount <= 0 || !data.vertices) {
        R3D_TRACELOG(LOG_WARNING, "R3D: Invalid mesh data passed to R3D_UpdateMesh");
        return mesh;
    }

    GLenum glUsage = GL_STATIC_DRAW;
    switch (usage) {
    case R3D_STATIC_MESH: glUsage = GL_STATIC_DRAW; break;
    case R3D_DYNAMIC_MESH: glUsage = GL_DYNAMIC_DRAW; break;
    case R3D_STREAMED_MESH: glUsage = GL_STREAM_DRAW; break;
    default:
        R3D_TRACELOG(LOG_WARNING, "R3D: Invalid mesh usage; R3D_STATIC_MESH will be used");
        break;
    }

    // Creation of the VAO
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    // Creation of the VBO
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, data.vertexCount * sizeof(R3D_Vertex), data.vertices, glUsage);

    // position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, position));

    // texcoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, texcoord));

    // normal (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, normal));

    // color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, color));

    // tangent (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, tangent));

    // boneIds (ivec4)
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, boneIds));

    // weights (vec4)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, weights));

    // instance position (vec3) (disabled)
    glVertexAttribDivisor(10, 1);
    glVertexAttrib3f(10, 0.0f, 0.0f, 0.0f);

    // instance rotation (vec4) (disabled)
    glVertexAttribDivisor(11, 1);
    glVertexAttrib4f(11, 0.0f, 0.0f, 0.0f, 1.0f);

    // instance scale (vec3) (disabled)
    glVertexAttribDivisor(12, 1);
    glVertexAttrib3f(12, 1.0f, 1.0f, 1.0f);

    // instance color (vec4) (disabled)
    glVertexAttribDivisor(13, 1);
    glVertexAttrib4f(13, 1.0f, 1.0f, 1.0f, 1.0f);

    // EBO if indices present
    if (data.indexCount > 0 && data.indices) {
        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.indexCount * sizeof(uint32_t), data.indices, glUsage);
    }

    // Cleaning
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // Fill mesh infos
    mesh.vertexCount = mesh.allocVertexCount = data.vertexCount;
    mesh.indexCount = mesh.allocIndexCount = data.indexCount;
    mesh.shadowCastMode = R3D_SHADOW_CAST_ON_AUTO;
    mesh.layerMask = R3D_LAYER_01;
    mesh.primitiveType = type;
    mesh.usage = usage;

    // Compute the bounding box, if needed
    mesh.aabb = (aabb != NULL) ? *aabb
        : R3D_CalculateMeshDataBoundingBox(data);

    return mesh;
}

void R3D_UnloadMesh(R3D_Mesh mesh)
{
    if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
    if (mesh.ebo != 0) glDeleteBuffers(1, &mesh.ebo);
}

bool R3D_IsMeshValid(R3D_Mesh mesh)
{
    return (mesh.vao != 0) && (mesh.vbo != 0);
}

R3D_Mesh R3D_GenMeshQuad(float width, float length, int resX, int resZ, Vector3 frontDir)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataQuad(width, length, resX, resZ, frontDir);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, NULL, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshPlane(float width, float length, int resX, int resZ)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataPlane(width, length, resX, resZ);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-width * 0.5f, 0.0f, -length * 0.5f},
        { width * 0.5f, 0.0f,  length * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshPoly(int sides, float radius)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataPoly(sides, radius);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius, 0.0f, -radius},
        { radius, 0.0f,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCube(float width, float height, float length)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataCube(width, height, length);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-width * 0.5f, -height * 0.5f, -length * 0.5f},
        { width * 0.5f,  height * 0.5f,  length * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshSphere(float radius, int rings, int slices)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataSphere(radius, rings, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius, -radius, -radius},
        { radius,  radius,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshHemiSphere(float radius, int rings, int slices)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataHemiSphere(radius, rings, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius,   0.0f, -radius},
        { radius, radius,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCylinder(float radius, float height, int slices)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataCylinder(radius, height, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius,   0.0f, -radius},
        { radius, height,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCone(float radius, float height, int slices)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataCone(radius, height, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius,   0.0f, -radius},
        { radius, height,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshTorus(float radius, float size, int radSeg, int sides)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataTorus(radius, size, radSeg, sides);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius - size, -size, -radius - size},
        { radius + size,  size,  radius + size}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshKnot(float radius, float size, int radSeg, int sides)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataKnot(radius, size, radSeg, sides);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius - size, -size, -radius - size},
        { radius + size,  size,  radius + size}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshHeightmap(Image heightmap, Vector3 size)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataHeightmap(heightmap, size);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-size.x * 0.5f,   0.0f, -size.z * 0.5f},
        { size.x * 0.5f, size.y,  size.z * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCubicmap(Image cubicmap, Vector3 cubeSize)
{
    R3D_Mesh mesh = { 0 };

    R3D_MeshData data = R3D_GenMeshDataCubicmap(cubicmap, cubeSize);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {0.0f, 0.0f, 0.0f},
        {cubicmap.width * cubeSize.x, cubeSize.y, cubicmap.height * cubeSize.z}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(data);

    return mesh;
}

bool R3D_UpdateMesh(R3D_Mesh* mesh, R3D_MeshData data, const BoundingBox* aabb)
{
    if (!mesh || mesh->vao == 0 || mesh->vbo == 0) {
        R3D_TRACELOG(LOG_WARNING, "R3D: Cannot update mesh; Invalid mesh instance");
        return false;
    }

    if (data.vertexCount <= 0 || !data.vertices) {
        R3D_TRACELOG(LOG_WARNING, "R3D: Invalid mesh data given to R3D_UpdateMesh");
        return false;
    }

    GLenum glUsage = GL_STATIC_DRAW;
    switch (mesh->usage) {
    case R3D_STATIC_MESH: glUsage = GL_STATIC_DRAW; break;
    case R3D_DYNAMIC_MESH: glUsage = GL_DYNAMIC_DRAW; break;
    case R3D_STREAMED_MESH: glUsage = GL_STREAM_DRAW; break;
    default:
        R3D_TRACELOG(LOG_WARNING, "R3D: Invalid mesh usage; R3D_STATIC_MESH will be used");
        break;
    }

    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);

    if (mesh->allocVertexCount < data.vertexCount) {
        glBufferData(GL_ARRAY_BUFFER, mesh->vertexCount * sizeof(R3D_Vertex), data.vertices, glUsage);
        mesh->allocVertexCount = data.vertexCount;
    }
    else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->vertexCount * sizeof(R3D_Vertex), data.vertices);
    }

    if (data.indexCount > 0) {
        if (mesh->allocIndexCount < data.indexCount) {
            if (mesh->ebo == 0) glGenBuffers(1, &mesh->ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * sizeof(uint32_t), data.indices, glUsage);
            mesh->allocIndexCount = data.indexCount;
        }
        else {
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, mesh->indexCount * sizeof(uint32_t), data.indices);
        }
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    mesh->vertexCount = data.vertexCount;
    mesh->indexCount = data.indexCount;

    return true;
}
