/* r3d_mesh.h -- R3D Mesh Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
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

#include "./modules/r3d_render.h"
#include "./common/r3d_helper.h"

// ========================================
// PUBLIC API
// ========================================

R3D_Mesh R3D_LoadMesh(R3D_PrimitiveType type, R3D_MeshData data, const BoundingBox* aabb)
{
    R3D_Mesh mesh = {0};

    if (!r3d_render_alloc_vertices(data.vertexCount, &mesh.vertexOffset)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load mesh; Vertices allocation in VRAM failed");
        return mesh;
    }

    if (data.indexCount > 0) {
        if (!r3d_render_alloc_elements(data.indexCount, &mesh.indexOffset)) {
            R3D_TRACELOG(LOG_WARNING, "Failed to load mesh; Elements allocation in VRAM failed");
            r3d_render_free_vertices(mesh.vertexOffset, data.vertexCount);
            mesh.vertexOffset = 0;
            return mesh;
        }
    }

    r3d_render_upload_vertices(mesh.vertexOffset, data.vertices, data.vertexCount);
    if (data.indexCount > 0) {
        r3d_render_upload_elements(mesh.indexOffset, data.indices, data.indexCount);
    }

    mesh.vertexCount = mesh.vertexCapacity = data.vertexCount;
    mesh.indexCount = mesh.indexCapacity = data.indexCount;
    mesh.shadowCastMode = R3D_SHADOW_CAST_ON_AUTO;
    mesh.layerMask = R3D_LAYER_01;
    mesh.primitiveType = type;

    // Compute the bounding box, if needed
    mesh.aabb = (aabb != NULL) ? *aabb
        : R3D_CalculateMeshDataBoundingBox(data);

    return mesh;
}

void R3D_UnloadMesh(R3D_Mesh mesh)
{
    r3d_render_free_vertices(mesh.vertexOffset, mesh.vertexCapacity);
    if (mesh.indexCapacity > 0) {
        r3d_render_free_elements(mesh.indexOffset, mesh.indexCapacity);
    }
}

bool R3D_IsMeshValid(R3D_Mesh mesh)
{
    return mesh.vertexCount > 0;
}

R3D_Mesh R3D_GenMeshQuad(float width, float length, int resX, int resZ, Vector3 frontDir)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataQuad(width, length, resX, resZ, frontDir);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, NULL);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshPlane(float width, float length, int resX, int resZ)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataPlane(width, length, resX, resZ);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-width * 0.5f, 0.0f, -length * 0.5f},
        { width * 0.5f, 0.0f,  length * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshPoly(int sides, float radius, Vector3 frontDir)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataPoly(sides, radius, frontDir);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius, 0.0f, -radius},
        { radius, 0.0f,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCube(float width, float height, float length)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataCube(width, height, length);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-width * 0.5f, -height * 0.5f, -length * 0.5f},
        { width * 0.5f,  height * 0.5f,  length * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCubeEx(float width, float height, float length, int resX, int resY, int resZ)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataCubeEx(width, height, length, resX, resY, resZ);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-width * 0.5f, -height * 0.5f, -length * 0.5f},
        { width * 0.5f,  height * 0.5f,  length * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshSlope(float width, float height, float length, Vector3 slopeNormal)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataSlope(width, height, length, slopeNormal);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-width * 0.5f, -height * 0.5f, -length * 0.5f},
        { width * 0.5f,  height * 0.5f,  length * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshSphere(float radius, int rings, int slices)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataSphere(radius, rings, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius, -radius, -radius},
        { radius,  radius,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshHemiSphere(float radius, int rings, int slices)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataHemiSphere(radius, rings, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius,   0.0f, -radius},
        { radius, radius,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCylinder(float radius, float height, int slices)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataCylinder(radius, height, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius, -height * 0.5f, -radius},
        { radius,  height * 0.5f,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCylinderEx(float bottomRadius, float topRadius, float height, int slices, int stacks, bool bottomCap, bool topCap)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataCylinderEx(bottomRadius, topRadius, height, slices, stacks, bottomCap, topCap);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    float radius = MAX(bottomRadius, topRadius);

    BoundingBox aabb = {
        {-radius, -height * 0.5f, -radius},
        { radius,  height * 0.5f,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCapsule(float radius, float height, int rings, int slices)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataCapsule(radius, height, rings, slices);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius, -radius,          -radius},
        { radius,  height + radius,  radius}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshTorus(float radius, float size, int radSeg, int sides)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataTorus(radius, size, radSeg, sides);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius - size, -size, -radius - size},
        { radius + size,  size,  radius + size}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshKnot(float radius, float size, int radSeg, int sides)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataKnot(radius, size, radSeg, sides);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-radius - size, -size, -radius - size},
        { radius + size,  size,  radius + size}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshHeightmap(Image heightmap, Vector3 size)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataHeightmap(heightmap, size);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {-size.x * 0.5f,   0.0f, -size.z * 0.5f},
        { size.x * 0.5f, size.y,  size.z * 0.5f}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

R3D_Mesh R3D_GenMeshCubicmap(Image cubicmap, Vector3 cubeSize)
{
    R3D_Mesh mesh = {0};

    R3D_MeshData data = R3D_GenMeshDataCubicmap(cubicmap, cubeSize);
    if (!R3D_IsMeshDataValid(data)) return mesh;

    BoundingBox aabb = {
        {0.0f, 0.0f, 0.0f},
        {cubicmap.width * cubeSize.x, cubeSize.y, cubicmap.height * cubeSize.z}
    };

    mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, data, &aabb);
    R3D_UnloadMeshData(data);

    return mesh;
}

bool R3D_UpdateMesh(R3D_Mesh* mesh, R3D_MeshData data, const BoundingBox* aabb)
{
    if (!mesh) {
        R3D_TRACELOG(LOG_WARNING, "Cannot update mesh; Invalid mesh instance");
        return false;
    }

    if (!data.vertices || data.vertexCount <= 0) {
        R3D_TRACELOG(LOG_WARNING, "Cannont update mesh; Invalid mesh data");
        return false;
    }

    if (mesh->vertexCapacity < data.vertexCount) {
        if (!r3d_render_realloc_vertices(&mesh->vertexOffset, &mesh->vertexCapacity, data.vertexCount, false)) {
            R3D_TRACELOG(LOG_WARNING, "Cannot update mesh; Vertex reallocation failed");
            return false;
        }
    }
    r3d_render_upload_vertices(mesh->vertexOffset, data.vertices, data.vertexCount);

    if (data.indexCount > 0) {
        if (mesh->indexCapacity < data.indexCount) {
            if (!r3d_render_realloc_elements(&mesh->indexOffset, &mesh->indexCapacity, data.indexCount, false)) {
                R3D_TRACELOG(LOG_WARNING, "Cannot update mesh; Element reallocation failed");
                return false;
            }
        }
        r3d_render_upload_elements(mesh->indexOffset, data.indices, data.indexCount);
    }

    mesh->vertexCount = data.vertexCount;
    mesh->indexCount = data.indexCount;

    mesh->aabb = (aabb != NULL) ? *aabb
        : R3D_CalculateMeshDataBoundingBox(data);

    return true;
}
