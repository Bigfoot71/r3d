/* r3d_mesh_data.c -- R3D Mesh Data Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_mesh_data.h>
#include <r3d_config.h>
#include <raymath.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool alloc_mesh(R3D_MeshData* meshData, int vertexCount, int indexCount);

static void gen_cube_face(
    R3D_Vertex **vertexPtr,
    uint32_t **indexPtr,
    uint32_t *vertexOffset,
    Vector3 origin,
    Vector3 uAxis,
    Vector3 vAxis,
    float uSize,
    float vSize,
    int uRes,
    int vRes,
    Vector3 normal,
    Vector3 tangent
);

// ========================================
// PUBLIC API
// ========================================

R3D_MeshData R3D_CreateMeshData(int vertexCount, int indexCount)
{
    R3D_MeshData meshData = {0};

    if (vertexCount <= 0) {
        R3D_TRACELOG(LOG_ERROR, "Invalid vertex count for mesh creation");
        return meshData;
    }

    meshData.vertices = RL_CALLOC(vertexCount, sizeof(*meshData.vertices));
    if (meshData.vertices == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to allocate memory for mesh vertices");
        return meshData;
    }

    meshData.vertexCount = vertexCount;

    if (indexCount > 0) {
        meshData.indices = RL_CALLOC(indexCount, sizeof(*meshData.indices));
        if (meshData.indices == NULL) {
            R3D_TRACELOG(LOG_ERROR, "Failed to allocate memory for mesh indices");
            RL_FREE(meshData.vertices);
            meshData.vertices = NULL;
            meshData.vertexCount = 0;
            return meshData;
        }
        meshData.indexCount = indexCount;
    }

    return meshData;
}

void R3D_UnloadMeshData(R3D_MeshData meshData)
{
    RL_FREE(meshData.vertices);
    RL_FREE(meshData.indices);
}

bool R3D_IsMeshDataValid(R3D_MeshData meshData)
{
    return (meshData.vertices != NULL) && (meshData.vertexCount > 0);
}

R3D_MeshData R3D_GenMeshDataQuad(float width, float length, int resX, int resZ, Vector3 frontDir)
{
    R3D_MeshData meshData = {0};

    Vector3 normal = Vector3Normalize(frontDir);
    Vector3 reference = (fabsf(normal.y) < 0.9f) ? (Vector3){0.0f, 1.0f, 0.0f} : (Vector3){1.0f, 0.0f, 0.0f};
    Vector3 tangent = Vector3Normalize(Vector3CrossProduct(normal, reference));
    Vector3 bitangent = Vector3CrossProduct(normal, tangent);

    Vector4 tangent4 = {tangent.x, tangent.y, tangent.z, 1.0f};
    float invResX = 1.0f / resX;
    float invResZ = 1.0f / resZ;
    int vertCountX = resX + 1;
    int vertCountZ = resZ + 1;

    if (!alloc_mesh(&meshData, vertCountX * vertCountZ, resX * resZ * 6)) {
        return meshData;
    }

    R3D_Vertex* vertex = meshData.vertices;
    for (int z = 0; z <= resZ; z++)
    {
        float v = z * invResZ;
        float localY = (v - 0.5f) * length;
        
        for (int x = 0; x <= resX; x++, vertex++)
        {
            float u = x * invResX;
            float localX = (u - 0.5f) * width;

            vertex->position = (Vector3){
                localX * tangent.x + localY * bitangent.x,
                localX * tangent.y + localY * bitangent.y,
                localX * tangent.z + localY * bitangent.z
            };
            vertex->texcoord = (Vector2){u, v};
            vertex->normal = normal;
            vertex->color = WHITE;
            vertex->tangent = tangent4;
        }
    }

    uint32_t* index = meshData.indices;
    for (int z = 0; z < resZ; z++)
    {
        uint32_t rowStart = z * vertCountX;
        uint32_t nextRowStart = rowStart + vertCountX;

        for (int x = 0; x < resX; x++)
        {
            uint32_t i0 = rowStart + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRowStart + x + 1;
            uint32_t i3 = nextRowStart + x;

            *index++ = i0; *index++ = i1; *index++ = i2;
            *index++ = i0; *index++ = i2; *index++ = i3;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataPlane(float width, float length, int resX, int resZ)
{
    R3D_MeshData meshData = {0};

    if (width <= 0.0f || length <= 0.0f || resX < 1 || resZ < 1) {
        return meshData;
    }

    int vertCountX = resX + 1;
    int vertCountZ = resZ + 1;

    if (!alloc_mesh(&meshData, vertCountX * vertCountZ, resX * resZ * 6)) {
        return meshData;
    }

    float halfWidth = width * 0.5f;
    float halfLength = length * 0.5f;
    float stepX = width / resX;
    float stepZ = length / resZ;
    float invResX = 1.0f / resX;
    float invResZ = 1.0f / resZ;

    R3D_Vertex* vertex = meshData.vertices;
    for (int z = 0; z <= resZ; z++)
    {
        float posZ = -halfLength + z * stepZ;
        float uvZ = z * invResZ;

        for (int x = 0; x <= resX; x++, vertex++)
        {
            float posX = -halfWidth + x * stepX;
            float uvX = x * invResX;

            *vertex = (R3D_Vertex){
                .position = {posX, 0.0f, posZ},
                .texcoord = {uvX, uvZ},
                .normal = {0.0f, 1.0f, 0.0f},
                .color = WHITE,
                .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
            };
        }
    }

    uint32_t* index = meshData.indices;
    for (int z = 0; z < resZ; z++)
    {
        uint32_t rowStart = z * vertCountX;
        uint32_t nextRowStart = rowStart + vertCountX;

        for (int x = 0; x < resX; x++)
        {
            uint32_t i0 = rowStart + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRowStart + x;
            uint32_t i3 = i2 + 1;

            *index++ = i0; *index++ = i2; *index++ = i1;
            *index++ = i1; *index++ = i2; *index++ = i3;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataPoly(int sides, float radius, Vector3 frontDir)
{
    R3D_MeshData meshData = {0};

    if (sides < 3 || radius <= 0.0f) {
        return meshData;
    }

    if (!alloc_mesh(&meshData, sides + 1, sides * 3)) {
        return meshData;
    }

    Vector3 normal = Vector3Normalize(frontDir);
    Vector3 reference = (fabsf(normal.y) < 0.9f) ? (Vector3){0.0f, 1.0f, 0.0f} : (Vector3){1.0f, 0.0f, 0.0f};
    Vector3 tangent = Vector3Normalize(Vector3CrossProduct(normal, reference));
    Vector3 bitangent = Vector3CrossProduct(normal, tangent);

    Vector4 tangent4 = {tangent.x, tangent.y, tangent.z, 1.0f};
    float angleStep = 2.0f * PI / sides;

    meshData.vertices[0] = (R3D_Vertex){
        .position = {0.0f, 0.0f, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = normal,
        .color = WHITE,
        .tangent = tangent4
    };

    R3D_Vertex* vertex = &meshData.vertices[1];
    uint32_t* index = meshData.indices;

    for (int i = 0; i < sides; i++, vertex++)
    {
        float angle = i * angleStep;
        float cosAngle = cosf(angle);
        float sinAngle = sinf(angle);

        float localX = radius * cosAngle;
        float localY = radius * sinAngle;

        vertex->position = (Vector3){
            localX * tangent.x + localY * bitangent.x,
            localX * tangent.y + localY * bitangent.y,
            localX * tangent.z + localY * bitangent.z
        };

        vertex->texcoord = (Vector2){
            0.5f + 0.5f * cosAngle,
            0.5f + 0.5f * sinAngle
        };
        vertex->normal = normal;
        vertex->color = WHITE;
        vertex->tangent = tangent4;

        *index++ = 0;
        *index++ = i + 1;
        *index++ = (i + 1) % sides + 1;
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCube(float width, float height, float length)
{
    R3D_MeshData meshData = {0};

    if (width <= 0.0f || height <= 0.0f || length <= 0.0f) {
        return meshData;
    }

    if (!alloc_mesh(&meshData, 24, 36)) {
        return meshData;
    }

    float halfW = width * 0.5f;
    float halfH = height * 0.5f;
    float halfL = length * 0.5f;

    Vector2 uvs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    R3D_Vertex* v = meshData.vertices;

    // Back face (+Z)
    *v++ = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[0], {0.0f, 0.0f, 1.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[1], {0.0f, 0.0f, 1.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, halfH, halfL}, uvs[2], {0.0f, 0.0f, 1.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[3], {0.0f, 0.0f, 1.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};

    // Front face (-Z)
    *v++ = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[0], {0.0f, 0.0f, -1.0f}, WHITE, {-1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[1], {0.0f, 0.0f, -1.0f}, WHITE, {-1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[2], {0.0f, 0.0f, -1.0f}, WHITE, {-1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[3], {0.0f, 0.0f, -1.0f}, WHITE, {-1.0f, 0.0f, 0.0f, 1.0f}};

    // Right face (+X)
    *v++ = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[0], {1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, -1.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[1], {1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, -1.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[2], {1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, -1.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, halfH, halfL}, uvs[3], {1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, -1.0f, 1.0f}};

    // Left face (-X)
    *v++ = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[0], {-1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, 1.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[1], {-1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, 1.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[2], {-1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, 1.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[3], {-1.0f, 0.0f, 0.0f}, WHITE, {0.0f, 0.0f, 1.0f, 1.0f}};

    // Top face (+Y)
    *v++ = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[0], {0.0f, 1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, halfH, halfL}, uvs[1], {0.0f, 1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[2], {0.0f, 1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[3], {0.0f, 1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};

    // Bottom face (-Y)
    *v++ = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[0], {0.0f, -1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[1], {0.0f, -1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[2], {0.0f, -1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};
    *v++ = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[3], {0.0f, -1.0f, 0.0f}, WHITE, {1.0f, 0.0f, 0.0f, 1.0f}};

    // Indices
    uint32_t* index = meshData.indices;
    for (int face = 0; face < 6; face++) {
        uint32_t base = face * 4;
        *index++ = base; *index++ = base + 1; *index++ = base + 2;
        *index++ = base + 2; *index++ = base + 3; *index++ = base;
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCubeEx(float width, float height, float length, int resX, int resY, int resZ)
{
    R3D_MeshData meshData = {0};

    if (width <= 0 || height <= 0 || length <= 0 || resX < 1 || resY < 1 || resZ < 1) {
        return meshData;
    }

    int vertXY = (resX + 1) * (resY + 1);
    int vertXZ = (resX + 1) * (resZ + 1);
    int vertYZ = (resY + 1) * (resZ + 1);

    int idxXY = resX * resY * 6;
    int idxXZ = resX * resZ * 6;
    int idxYZ = resY * resZ * 6;

    int totalVertices = 2 * (vertXY + vertXZ + vertYZ);
    int totalIndices  = 2 * (idxXY + idxXZ + idxYZ);

    if (!alloc_mesh(&meshData, totalVertices, totalIndices)) {
        return meshData;
    }

    float hw = width  * 0.5f;
    float hh = height * 0.5f;
    float hl = length * 0.5f;

    R3D_Vertex *vertex = meshData.vertices;
    uint32_t *index = meshData.indices;
    uint32_t vertexOffset = 0;

    // +Z
    gen_cube_face(&vertex, &index, &vertexOffset,
        (Vector3){0, 0,  hl}, (Vector3){ 1, 0, 0}, (Vector3){0, 1, 0},
        width, height, resX, resY,
        (Vector3){0, 0,  1}, (Vector3){ 1, 0, 0});

    // -Z
    gen_cube_face(&vertex, &index, &vertexOffset,
        (Vector3){0, 0, -hl}, (Vector3){-1, 0, 0}, (Vector3){0, 1, 0},
        width, height, resX, resY,
        (Vector3){0, 0, -1}, (Vector3){-1, 0, 0});

    // +X
    gen_cube_face(&vertex, &index, &vertexOffset,
        (Vector3){ hw, 0, 0}, (Vector3){0, 0,-1}, (Vector3){0, 1, 0},
        length, height, resZ, resY,
        (Vector3){ 1, 0, 0}, (Vector3){0, 0,-1});

    // -X
    gen_cube_face(&vertex, &index, &vertexOffset,
        (Vector3){-hw, 0, 0}, (Vector3){0, 0, 1}, (Vector3){0, 1, 0},
        length, height, resZ, resY,
        (Vector3){-1, 0, 0}, (Vector3){0, 0, 1});

    // +Y
    gen_cube_face(&vertex, &index, &vertexOffset,
        (Vector3){0,  hh, 0}, (Vector3){1, 0, 0}, (Vector3){0, 0,-1},
        width, length, resX, resZ,
        (Vector3){0, 1, 0}, (Vector3){1, 0, 0});

    // -Y
    gen_cube_face(&vertex, &index, &vertexOffset,
        (Vector3){0, -hh, 0}, (Vector3){1, 0, 0}, (Vector3){0, 0, 1},
        width, length, resX, resZ,
        (Vector3){0,-1, 0}, (Vector3){1, 0, 0});

    return meshData;
}

R3D_MeshData R3D_GenMeshDataSphere(float radius, int rings, int slices)
{
    R3D_MeshData meshData = {0};

    if (radius <= 0.0f || rings < 2 || slices < 3) {
        return meshData;
    }

    int vertCountPerRing = slices + 1;
    if (!alloc_mesh(&meshData, (rings + 1) * vertCountPerRing, rings * slices * 6)) {
        return meshData;
    }

    float ringStep = PI / rings;
    float sliceStep = 2.0f * PI / slices;
    float invRings = 1.0f / rings;
    float invSlices = 1.0f / slices;
    float invRadius = 1.0f / radius;

    R3D_Vertex* vertex = meshData.vertices;
    for (int ring = 0; ring <= rings; ring++)
    {
        float phi = ring * ringStep;
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);
        float y = radius * cosPhi;
        float ringRadius = radius * sinPhi;
        float v = ring * invRings;

        for (int slice = 0; slice <= slices; slice++, vertex++)
        {
            float theta = slice * sliceStep;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            float x = ringRadius * cosTheta;
            float z = ringRadius * -sinTheta;

            vertex->position = (Vector3){x, y, z};
            vertex->texcoord = (Vector2){slice * invSlices, v};
            vertex->normal = (Vector3){x * invRadius, y * invRadius, z * invRadius};
            vertex->color = WHITE;
            vertex->tangent = (Vector4){-sinTheta, 0.0f, -cosTheta, 1.0f};
        }
    }

    uint32_t* index = meshData.indices;
    for (int ring = 0; ring < rings; ring++)
    {
        uint32_t currentRow = ring * vertCountPerRing;
        uint32_t nextRow = currentRow + vertCountPerRing;

        for (int slice = 0; slice < slices; slice++)
        {
            uint32_t i0 = currentRow + slice;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRow + slice;
            uint32_t i3 = i2 + 1;

            *index++ = i0; *index++ = i2; *index++ = i3;
            *index++ = i0; *index++ = i3; *index++ = i1;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataHemiSphere(float radius, int rings, int slices)
{
    R3D_MeshData meshData = {0};

    if (radius <= 0.0f || rings < 1 || slices < 3) {
        return meshData;
    }

    int vertCountPerRing = slices + 1;
    int hemisphereVertCount = (rings + 1) * vertCountPerRing;
    int totalVertCount = hemisphereVertCount + 1 + vertCountPerRing;
    int totalIndexCount = rings * slices * 6 + slices * 3;

    if (!alloc_mesh(&meshData, totalVertCount, totalIndexCount)) {
        return meshData;
    }

    float ringStep = (PI * 0.5f) / rings;
    float sliceStep = 2.0f * PI / slices;
    float invRings = 1.0f / rings;
    float invSlices = 1.0f / slices;
    float invRadius = 1.0f / radius;

    R3D_Vertex* vertex = meshData.vertices;
    for (int ring = 0; ring <= rings; ring++)
    {
        float phi = ring * ringStep;
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);
        float y = radius * cosPhi;
        float ringRadius = radius * sinPhi;
        float v = ring * invRings;

        for (int slice = 0; slice <= slices; slice++, vertex++)
        {
            float theta = slice * sliceStep;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            float x = ringRadius * cosTheta;
            float z = ringRadius * -sinTheta;

            vertex->position = (Vector3){x, y, z};
            vertex->texcoord = (Vector2){slice * invSlices, v};
            vertex->normal = (Vector3){x * invRadius, y * invRadius, z * invRadius};
            vertex->color = WHITE;
            vertex->tangent = (Vector4){-sinTheta, 0.0f, -cosTheta, 1.0f};
        }
    }

    uint32_t baseCenterIdx = hemisphereVertCount;
    *vertex++ = (R3D_Vertex){
        .position = {0.0f, 0.0f, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = {0.0f, -1.0f, 0.0f},
        .color = WHITE,
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
    };

    for (int slice = 0; slice <= slices; slice++, vertex++)
    {
        float theta = slice * sliceStep;
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);

        float x = radius * cosTheta;
        float z = radius * -sinTheta;

        vertex->position = (Vector3){x, 0.0f, z};
        vertex->texcoord = (Vector2){0.5f + 0.5f * cosTheta, 0.5f - 0.5f * sinTheta};
        vertex->normal = (Vector3){0.0f, -1.0f, 0.0f};
        vertex->color = WHITE;
        vertex->tangent = (Vector4){1.0f, 0.0f, 0.0f, 1.0f};
    }

    uint32_t* index = meshData.indices;
    for (int ring = 0; ring < rings; ring++)
    {
        uint32_t currentRow = ring * vertCountPerRing;
        uint32_t nextRow = currentRow + vertCountPerRing;

        for (int slice = 0; slice < slices; slice++)
        {
            uint32_t i0 = currentRow + slice;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRow + slice;
            uint32_t i3 = i2 + 1;

            *index++ = i0; *index++ = i2; *index++ = i3;
            *index++ = i0; *index++ = i3; *index++ = i1;
        }
    }

    uint32_t basePerimeterStart = baseCenterIdx + 1;
    for (int slice = 0; slice < slices; slice++)
    {
        *index++ = basePerimeterStart + slice;
        *index++ = baseCenterIdx;
        *index++ = basePerimeterStart + slice + 1;
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCylinder(float bottomRadius, float topRadius, float height, int slices)
{
    R3D_MeshData meshData = {0};

    if (bottomRadius < 0.0f || topRadius < 0.0f || height <= 0.0f || slices < 3) {
        return meshData;
    }

    if (bottomRadius == 0.0f && topRadius == 0.0f) {
        return meshData;
    }

    int vertCountPerRing = slices + 1;
    int bodyVertCount = 2 * vertCountPerRing;
    int capVertCount = 0;
    if (bottomRadius > 0.0f) capVertCount += 1 + slices;
    if (topRadius > 0.0f) capVertCount += 1 + slices;
    
    int totalVertCount = bodyVertCount + capVertCount;
    int bodyIndexCount = slices * 6;
    int capIndexCount = 0;
    if (bottomRadius > 0.0f) capIndexCount += slices * 3;
    if (topRadius > 0.0f) capIndexCount += slices * 3;
    int totalIndexCount = bodyIndexCount + capIndexCount;

    if (!alloc_mesh(&meshData, totalVertCount, totalIndexCount)) {
        return meshData;
    }

    float halfHeight = height * 0.5f;
    float sliceStep = 2.0f * PI / slices;
    float invSlices = 1.0f / slices;

    R3D_Vertex* vertex = meshData.vertices;

    for (int slice = 0; slice <= slices; slice++, vertex++)
    {
        float theta = slice * sliceStep;
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);

        float x = bottomRadius * cosTheta;
        float z = -bottomRadius * sinTheta;

        vertex->position = (Vector3){x, -halfHeight, z};
        vertex->texcoord = (Vector2){slice * invSlices, 0.0f};
        vertex->normal = (Vector3){cosTheta, 0.0f, -sinTheta};
        vertex->color = WHITE;
        vertex->tangent = (Vector4){-sinTheta, 0.0f, -cosTheta, 1.0f};
    }

    for (int slice = 0; slice <= slices; slice++, vertex++)
    {
        float theta = slice * sliceStep;
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);

        float x = topRadius * cosTheta;
        float z = -topRadius * sinTheta;

        vertex->position = (Vector3){x, halfHeight, z};
        vertex->texcoord = (Vector2){slice * invSlices, 1.0f};
        vertex->normal = (Vector3){cosTheta, 0.0f, -sinTheta};
        vertex->color = WHITE;
        vertex->tangent = (Vector4){-sinTheta, 0.0f, -cosTheta, 1.0f};
    }

    uint32_t bottomCapStart = 0;
    uint32_t topCapStart = 0;

    if (bottomRadius > 0.0f)
    {
        bottomCapStart = bodyVertCount;
        *vertex++ = (R3D_Vertex){
            .position = {0.0f, -halfHeight, 0.0f},
            .texcoord = {0.5f, 0.5f},
            .normal = {0.0f, -1.0f, 0.0f},
            .color = WHITE,
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
        };

        for (int slice = 0; slice < slices; slice++, vertex++)
        {
            float theta = slice * sliceStep;
            float cosTheta = cosf(theta);
            float sinTheta = sinf(theta);

            float x = bottomRadius * cosTheta;
            float z = -bottomRadius * sinTheta;

            vertex->position = (Vector3){x, -halfHeight, z};
            vertex->texcoord = (Vector2){0.5f + 0.5f * cosTheta, 0.5f - 0.5f * sinTheta};
            vertex->normal = (Vector3){0.0f, -1.0f, 0.0f};
            vertex->color = WHITE;
            vertex->tangent = (Vector4){1.0f, 0.0f, 0.0f, 1.0f};
        }
    }

    if (topRadius > 0.0f)
    {
        topCapStart = bottomRadius > 0.0f ? bottomCapStart + 1 + slices : bodyVertCount;
        *vertex++ = (R3D_Vertex){
            .position = {0.0f, halfHeight, 0.0f},
            .texcoord = {0.5f, 0.5f},
            .normal = {0.0f, 1.0f, 0.0f},
            .color = WHITE,
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
        };

        for (int slice = 0; slice < slices; slice++, vertex++)
        {
            float theta = slice * sliceStep;
            float cosTheta = cosf(theta);
            float sinTheta = sinf(theta);

            float x = topRadius * cosTheta;
            float z = -topRadius * sinTheta;

            vertex->position = (Vector3){x, halfHeight, z};
            vertex->texcoord = (Vector2){0.5f + 0.5f * cosTheta, 0.5f - 0.5f * sinTheta};
            vertex->normal = (Vector3){0.0f, 1.0f, 0.0f};
            vertex->color = WHITE;
            vertex->tangent = (Vector4){1.0f, 0.0f, 0.0f, 1.0f};
        }
    }

    uint32_t* index = meshData.indices;

    for (int slice = 0; slice < slices; slice++) {
        uint32_t i0 = slice;
        uint32_t i1 = slice + 1;
        uint32_t i2 = vertCountPerRing + slice;
        uint32_t i3 = vertCountPerRing + slice + 1;
        *index++ = i0; *index++ = i1; *index++ = i3;
        *index++ = i0; *index++ = i3; *index++ = i2;
    }

    if (bottomRadius > 0.0f) {
        uint32_t centerIdx = bottomCapStart;
        uint32_t perimeterStart = bottomCapStart + 1;
        for (int slice = 0; slice < slices; slice++) {
            uint32_t current = perimeterStart + slice;
            uint32_t next = perimeterStart + (slice + 1) % slices;
            *index++ = centerIdx;
            *index++ = next;
            *index++ = current;
        }
    }

    if (topRadius > 0.0f) {
        uint32_t centerIdx = topCapStart;
        uint32_t perimeterStart = topCapStart + 1;
        for (int slice = 0; slice < slices; slice++) {
            uint32_t current = perimeterStart + slice;
            uint32_t next = perimeterStart + (slice + 1) % slices;
            *index++ = centerIdx;
            *index++ = current;
            *index++ = next;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataTorus(float radius, float size, int radSeg, int sides)
{
    R3D_MeshData meshData = {0};

    if (radius <= 0.0f || size <= 0.0f || radSeg < 3 || sides < 3) {
        return meshData;
    }

    int rings = radSeg + 1;
    int segments = sides + 1;

    if (!alloc_mesh(&meshData, rings * segments, radSeg * sides * 6)) {
        return meshData;
    }

    float ringStep = 2.0f * PI / radSeg;
    float sideStep = 2.0f * PI / sides;
    float invRadSeg = 1.0f / radSeg;
    float invSides = 1.0f / sides;

    R3D_Vertex* vertex = meshData.vertices;
    for (int ring = 0; ring <= radSeg; ring++)
    {
        float theta = ring * ringStep;
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);

        float ringCenterX = radius * cosTheta;
        float ringCenterZ = -radius * sinTheta;
        float u = ring * invRadSeg;

        for (int side = 0; side <= sides; side++, vertex++)
        {
            float phi = side * sideStep;
            float cosPhi = cosf(phi);
            float sinPhi = sinf(phi);

            Vector3 normal = {
                cosTheta * cosPhi,
                sinPhi,
                -sinTheta * cosPhi
            };

            vertex->position = (Vector3){
                ringCenterX + size * normal.x,
                size * normal.y,
                ringCenterZ + size * normal.z
            };
            vertex->texcoord = (Vector2){u, side * invSides};
            vertex->normal = normal;
            vertex->color = WHITE;
            vertex->tangent = (Vector4){-sinTheta, 0.0f, -cosTheta, 1.0f};
        }
    }

    uint32_t* index = meshData.indices;
    for (int ring = 0; ring < radSeg; ring++)
    {
        uint32_t currentRow = ring * segments;
        uint32_t nextRow = currentRow + segments;

        for (int side = 0; side < sides; side++)
        {
            uint32_t i0 = currentRow + side;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRow + side;
            uint32_t i3 = i2 + 1;

            *index++ = i0; *index++ = i2; *index++ = i3;
            *index++ = i0; *index++ = i3; *index++ = i1;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataKnot(float radius, float size, int radSeg, int sides)
{
    R3D_MeshData meshData = {0};

    if (radius <= 0.0f || size <= 0.0f || radSeg < 6 || sides < 3) {
        return meshData;
    }

    int knotSegments = radSeg + 1;
    int tubeSides = sides + 1;

    if (!alloc_mesh(&meshData, knotSegments * tubeSides, radSeg * sides * 6)) {
        return meshData;
    }

    float segmentStep = 2.0f * PI / radSeg;
    float sideStep = 2.0f * PI / sides;
    float invRadSeg = 1.0f / radSeg;
    float invSides = 1.0f / sides;
    float knotScale = radius * 0.2f;

    R3D_Vertex* vertex = meshData.vertices;
    for (int seg = 0; seg <= radSeg; seg++)
    {
        float t = seg * segmentStep;
        float t2 = 2.0f * t;
        float t3 = 3.0f * t;
        
        float sinT = sinf(t);
        float cosT = cosf(t);
        float sin2T = sinf(t2);
        float cos2T = cosf(t2);
        float sin3T = sinf(t3);
        float cos3T = cosf(t3);

        Vector3 knotCenter = {
            knotScale * (sinT + 2.0f * sin2T),
            knotScale * (cosT - 2.0f * cos2T),
            knotScale * -sin3T
        };

        Vector3 tangent = {
            cosT + 4.0f * cos2T,
            -sinT + 4.0f * sin2T,
            -3.0f * cos3T
        };
        float tangentLen = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (tangentLen > 0.0f) {
            float invTangentLen = 1.0f / tangentLen;
            tangent.x *= invTangentLen;
            tangent.y *= invTangentLen;
            tangent.z *= invTangentLen;
        }

        Vector3 binormal = {
            -sinT - 8.0f * sin2T,
            -cosT + 8.0f * cos2T,
            9.0f * sin3T
        };
        float binormalLen = sqrtf(binormal.x * binormal.x + binormal.y * binormal.y + binormal.z * binormal.z);
        if (binormalLen > 0.0f) {
            float invBinormalLen = 1.0f / binormalLen;
            binormal.x *= invBinormalLen;
            binormal.y *= invBinormalLen;
            binormal.z *= invBinormalLen;
        }

        Vector3 normal = {
            tangent.y * binormal.z - tangent.z * binormal.y,
            tangent.z * binormal.x - tangent.x * binormal.z,
            tangent.x * binormal.y - tangent.y * binormal.x
        };

        float u = seg * invRadSeg;

        for (int side = 0; side <= sides; side++, vertex++)
        {
            float phi = side * sideStep;
            float cosPhi = cosf(phi);
            float sinPhi = sinf(phi);

            Vector3 surfaceNormal = {
                normal.x * cosPhi + binormal.x * sinPhi,
                normal.y * cosPhi + binormal.y * sinPhi,
                normal.z * cosPhi + binormal.z * sinPhi
            };

            vertex->position = (Vector3){
                knotCenter.x + size * surfaceNormal.x,
                knotCenter.y + size * surfaceNormal.y,
                knotCenter.z + size * surfaceNormal.z
            };
            vertex->texcoord = (Vector2){u, side * invSides};
            vertex->normal = surfaceNormal;
            vertex->color = WHITE;
            vertex->tangent = (Vector4){tangent.x, tangent.y, tangent.z, 1.0f};
        }
    }

    uint32_t* index = meshData.indices;
    for (int seg = 0; seg < radSeg; seg++)
    {
        uint32_t currentRow = seg * tubeSides;
        uint32_t nextRow = currentRow + tubeSides;

        for (int side = 0; side < sides; side++)
        {
            uint32_t i0 = currentRow + side;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRow + side;
            uint32_t i3 = i2 + 1;

            *index++ = i0; *index++ = i2; *index++ = i3;
            *index++ = i0; *index++ = i3; *index++ = i1;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataHeightmap(Image heightmap, Vector3 size)
{
    R3D_MeshData meshData = {0};

    if (heightmap.data == NULL || heightmap.width <= 1 || heightmap.height <= 1) {
        return meshData;
    }

    if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
        return meshData;
    }

    const int mapWidth = heightmap.width;
    const int mapHeight = heightmap.height;

    if (!alloc_mesh(&meshData, mapWidth * mapHeight, (mapWidth - 1) * (mapHeight - 1) * 6)) {
        return meshData;
    }

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    const float halfSizeX = size.x * 0.5f;
    const float halfSizeZ = size.z * 0.5f;
    const float stepX     = size.x / (mapWidth - 1);
    const float stepZ     = size.z / (mapHeight - 1);
    const float inv2StepX = 1.0f / (2.0f * stepX);
    const float inv2StepZ = 1.0f / (2.0f * stepZ);
    const float stepU     = 1.0f / (mapWidth - 1);
    const float stepV     = 1.0f / (mapHeight - 1);

    #define GET_HEIGHT_VALUE(x, y) \
        ((x) < 0 || (x) >= mapWidth || (y) < 0 || (y) >= mapHeight) \
            ? 0.0f : ((float)GetImageColor(heightmap, x, y).r / 255)

    int vertexIndex = 0;
    for (int z = 0; z < mapHeight; z++) {
        for (int x = 0; x < mapWidth; x++)
        {
            float posX = -halfSizeX + x * stepX;
            float posZ = -halfSizeZ + z * stepZ;
            float posY = GET_HEIGHT_VALUE(x, z) * size.y;

            float heightL = GET_HEIGHT_VALUE(x - 1, z);
            float heightR = GET_HEIGHT_VALUE(x + 1, z);
            float heightD = GET_HEIGHT_VALUE(x, z - 1);
            float heightU = GET_HEIGHT_VALUE(x, z + 1);

            float gradX = (heightR - heightL) * inv2StepX;
            float gradZ = (heightU - heightD) * inv2StepZ;

            Vector3 normal = Vector3Normalize((Vector3) {-gradX, 1.0f, -gradZ});
            Vector3 tangent = Vector3Normalize((Vector3) {1.0f, gradX, 0.0f});

            meshData.vertices[vertexIndex] = (R3D_Vertex){
                .position = {posX, posY, posZ},
                .texcoord = {x * stepU, z * stepV},
                .normal = normal,
                .color = WHITE,
                .tangent = (Vector4) {tangent.x, tangent.y, tangent.z, 1.0f}
            };
            vertexIndex++;
        }
    }

    int indexOffset = 0;
    for (int z = 0; z < mapHeight - 1; z++) {
        for (int x = 0; x < mapWidth - 1; x++)
        {
            uint32_t topLeft = z * mapWidth + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * mapWidth + x;
            uint32_t bottomRight = bottomLeft + 1;

            meshData.indices[indexOffset++] = topLeft;
            meshData.indices[indexOffset++] = bottomLeft;
            meshData.indices[indexOffset++] = topRight;

            meshData.indices[indexOffset++] = topRight;
            meshData.indices[indexOffset++] = bottomLeft;
            meshData.indices[indexOffset++] = bottomRight;
        }
    }

    #undef GET_HEIGHT_VALUE

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCubicmap(Image cubicmap, Vector3 cubeSize)
{
    R3D_MeshData meshData = {0};

    if (cubicmap.width <= 0 || cubicmap.height <= 0) {
        return meshData;
    }

    if (cubeSize.x <= 0.0f || cubeSize.y <= 0.0f || cubeSize.z <= 0.0f) {
        return meshData;
    }

    Color* pixels = LoadImageColors(cubicmap);
    if (!pixels) return meshData;

    const float halfW = cubeSize.x * 0.5f;
    const float halfH = cubeSize.y * 0.5f;
    const float halfL = cubeSize.z * 0.5f;

    const Vector3 normals[6] = {
        { 1.0f,  0.0f,  0.0f},
        {-1.0f,  0.0f,  0.0f},
        { 0.0f,  1.0f,  0.0f},
        { 0.0f, -1.0f,  0.0f},
        { 0.0f,  0.0f, -1.0f},
        { 0.0f,  0.0f,  1.0f}
    };

    const Vector4 tangents[6] = {
        { 0.0f,  0.0f, -1.0f,  1.0f},
        { 0.0f,  0.0f,  1.0f,  1.0f},
        { 1.0f,  0.0f,  0.0f,  1.0f},
        { 1.0f,  0.0f,  0.0f,  1.0f},
        {-1.0f,  0.0f,  0.0f,  1.0f},
        { 1.0f,  0.0f,  0.0f,  1.0f}
    };

    typedef struct { float x, y, width, height; } RectangleF;
    const RectangleF texUVs[6] = {
        {0.0f, 0.0f, 0.5f, 0.5f},
        {0.5f, 0.0f, 0.5f, 0.5f},
        {0.0f, 0.5f, 0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f, 0.5f},
        {0.5f, 0.0f, 0.5f, 0.5f},
        {0.0f, 0.0f, 0.5f, 0.5f}
    };

    // Estimate the maximum number of faces needed
    int maxFaces = 0;
    for (int z = 0; z < cubicmap.height; z++) {
        for (int x = 0; x < cubicmap.width; x++) {
            Color pixel = pixels[z * cubicmap.width + x];
            if (pixel.r >= 127) maxFaces += 6;  // (white) complete cube
            else maxFaces += 2;                 // (black) floor and ceiling only
        }
    }

    if (!alloc_mesh(&meshData, maxFaces * 4, maxFaces * 6)) {
        UnloadImageColors(pixels);
        return meshData;
    }

    R3D_Vertex* vertices = meshData.vertices;
    uint32_t* indices = meshData.indices;

    int vertexCount = 0;
    int indexCount = 0;

    for (int z = 0; z < cubicmap.height; z++) {
        for (int x = 0; x < cubicmap.width; x++)
        {
            Color pixel = pixels[z * cubicmap.width + x];

            // Position of the center of the cube
            float posX = cubeSize.x * (x - cubicmap.width * 0.5f + 0.5f);
            float posZ = cubeSize.z * (z - cubicmap.height * 0.5f + 0.5f);

            if (ColorIsEqual(pixel, WHITE))
            {
                // Face up (always generated for white cubes)
                if (true)
                {
                    Vector2 uvs[4] = {
                        {texUVs[2].x, texUVs[2].y},
                        {texUVs[2].x, texUVs[2].y + texUVs[2].height},
                        {texUVs[2].x + texUVs[2].width, texUVs[2].y + texUVs[2].height},
                        {texUVs[2].x + texUVs[2].width, texUVs[2].y}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs[0], normals[2], {255, 255, 255, 255}, tangents[2]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs[1], normals[2], {255, 255, 255, 255}, tangents[2]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs[2], normals[2], {255, 255, 255, 255}, tangents[2]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs[3], normals[2], {255, 255, 255, 255}, tangents[2]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 0;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Face down (always generated for white cubes)
                if (true)
                {
                    Vector2 uvs[4] = {
                        {texUVs[3].x + texUVs[3].width, texUVs[3].y},
                        {texUVs[3].x, texUVs[3].y + texUVs[3].height},
                        {texUVs[3].x + texUVs[3].width, texUVs[3].y + texUVs[3].height},
                        {texUVs[3].x, texUVs[3].y}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs[0], normals[3], {255, 255, 255, 255}, tangents[3]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs[1], normals[3], {255, 255, 255, 255}, tangents[3]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs[2], normals[3], {255, 255, 255, 255}, tangents[3]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs[3], normals[3], {255, 255, 255, 255}, tangents[3]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 1;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Checking the lateral faces (occlusion culling)

                // Back face (+Z)
                if ((z == cubicmap.height - 1) || !ColorIsEqual(pixels[(z + 1) * cubicmap.width + x], WHITE))
                {
                    Vector2 uvs[4] = {
                        {texUVs[5].x, texUVs[5].y},
                        {texUVs[5].x, texUVs[5].y + texUVs[5].height},
                        {texUVs[5].x + texUVs[5].width, texUVs[5].y},
                        {texUVs[5].x + texUVs[5].width, texUVs[5].y + texUVs[5].height}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs[0], normals[5], {255, 255, 255, 255}, tangents[5]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs[1], normals[5], {255, 255, 255, 255}, tangents[5]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs[2], normals[5], {255, 255, 255, 255}, tangents[5]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs[3], normals[5], {255, 255, 255, 255}, tangents[5]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 1;
                    indices[indexCount + 5] = vertexCount + 3;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Front face (-Z)
                if ((z == 0) || !ColorIsEqual(pixels[(z - 1) * cubicmap.width + x], WHITE))
                {
                    Vector2 uvs[4] = {
                        {texUVs[4].x + texUVs[4].width, texUVs[4].y},
                        {texUVs[4].x, texUVs[4].y + texUVs[4].height},
                        {texUVs[4].x + texUVs[4].width, texUVs[4].y + texUVs[4].height},
                        {texUVs[4].x, texUVs[4].y}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs[0], normals[4], {255, 255, 255, 255}, tangents[4]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs[1], normals[4], {255, 255, 255, 255}, tangents[4]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs[2], normals[4], {255, 255, 255, 255}, tangents[4]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs[3], normals[4], {255, 255, 255, 255}, tangents[4]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 2;
                    indices[indexCount + 2] = vertexCount + 1;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 1;
                    indices[indexCount + 5] = vertexCount + 3;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Right face (+X)
                if ((x == cubicmap.width - 1) || !ColorIsEqual(pixels[z * cubicmap.width + (x + 1)], WHITE))
                {
                    Vector2 uvs[4] = {
                        {texUVs[0].x, texUVs[0].y},
                        {texUVs[0].x, texUVs[0].y + texUVs[0].height},
                        {texUVs[0].x + texUVs[0].width, texUVs[0].y},
                        {texUVs[0].x + texUVs[0].width, texUVs[0].y + texUVs[0].height}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs[0], normals[0], {255, 255, 255, 255}, tangents[0]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs[1], normals[0], {255, 255, 255, 255}, tangents[0]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs[2], normals[0], {255, 255, 255, 255}, tangents[0]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs[3], normals[0], {255, 255, 255, 255}, tangents[0]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 1;
                    indices[indexCount + 5] = vertexCount + 3;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Left face (-X)
                if ((x == 0) || !ColorIsEqual(pixels[z * cubicmap.width + (x - 1)], WHITE))
                {
                    Vector2 uvs[4] = {
                        {texUVs[1].x, texUVs[1].y},
                        {texUVs[1].x + texUVs[1].width, texUVs[1].y + texUVs[1].height},
                        {texUVs[1].x + texUVs[1].width, texUVs[1].y},
                        {texUVs[1].x, texUVs[1].y + texUVs[1].height}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs[0], normals[1], {255, 255, 255, 255}, tangents[1]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs[1], normals[1], {255, 255, 255, 255}, tangents[1]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs[2], normals[1], {255, 255, 255, 255}, tangents[1]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs[3], normals[1], {255, 255, 255, 255}, tangents[1]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 1;

                    vertexCount += 4;
                    indexCount += 6;
                }
            }
            else if (ColorIsEqual(pixel, BLACK))
            {
                // Ceiling face (inverted to be visible from below)
                Vector2 uvs_top[4] = {
                    {texUVs[2].x, texUVs[2].y},
                    {texUVs[2].x + texUVs[2].width, texUVs[2].y + texUVs[2].height},
                    {texUVs[2].x, texUVs[2].y + texUVs[2].height},
                    {texUVs[2].x + texUVs[2].width, texUVs[2].y}
                };

                vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs_top[0], normals[3], {255, 255, 255, 255}, tangents[3]};
                vertices[vertexCount + 1] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs_top[1], normals[3], {255, 255, 255, 255}, tangents[3]};
                vertices[vertexCount + 2] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs_top[2], normals[3], {255, 255, 255, 255}, tangents[3]};
                vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs_top[3], normals[3], {255, 255, 255, 255}, tangents[3]};

                indices[indexCount + 0] = vertexCount + 0;
                indices[indexCount + 1] = vertexCount + 1;
                indices[indexCount + 2] = vertexCount + 2;
                indices[indexCount + 3] = vertexCount + 0;
                indices[indexCount + 4] = vertexCount + 3;
                indices[indexCount + 5] = vertexCount + 1;

                vertexCount += 4;
                indexCount += 6;

                // Ground face
                Vector2 uvs_bottom[4] = {
                    {texUVs[3].x + texUVs[3].width, texUVs[3].y},
                    {texUVs[3].x + texUVs[3].width, texUVs[3].y + texUVs[3].height},
                    {texUVs[3].x, texUVs[3].y + texUVs[3].height},
                    {texUVs[3].x, texUVs[3].y}
                };

                vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs_bottom[0], normals[2], {255, 255, 255, 255}, tangents[2]};
                vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs_bottom[1], normals[2], {255, 255, 255, 255}, tangents[2]};
                vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs_bottom[2], normals[2], {255, 255, 255, 255}, tangents[2]};
                vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs_bottom[3], normals[2], {255, 255, 255, 255}, tangents[2]};

                indices[indexCount + 0] = vertexCount + 0;
                indices[indexCount + 1] = vertexCount + 1;
                indices[indexCount + 2] = vertexCount + 2;
                indices[indexCount + 3] = vertexCount + 2;
                indices[indexCount + 4] = vertexCount + 3;
                indices[indexCount + 5] = vertexCount + 0;

                vertexCount += 4;
                indexCount += 6;
            }
        }
    }

    // Cleaning
    UnloadImageColors(pixels);

    return meshData;
}

R3D_MeshData R3D_DuplicateMeshData(R3D_MeshData meshData)
{
    R3D_MeshData duplicate = {0};

    if (meshData.vertices == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Cannot duplicate null mesh data");
        return duplicate;
    }

    duplicate = R3D_CreateMeshData(meshData.vertexCount, meshData.indexCount);
    if (duplicate.vertices == NULL) {
        return duplicate;
    }

    memcpy(duplicate.vertices, meshData.vertices, meshData.vertexCount * sizeof(*meshData.vertices));

    if (meshData.indexCount > 0 && meshData.indices != NULL && duplicate.indices != NULL) {
        memcpy(duplicate.indices, meshData.indices, meshData.indexCount * sizeof(*meshData.indices));
    }

    return duplicate;
}

R3D_MeshData R3D_MergeMeshData(R3D_MeshData a, R3D_MeshData b)
{
    R3D_MeshData merged = {0};

    if (a.vertices == NULL || b.vertices == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Cannot merge null mesh data");
        return merged;
    }

    int totalVertices = a.vertexCount + b.vertexCount;
    int totalIndices = a.indexCount + b.indexCount;

    merged = R3D_CreateMeshData(totalVertices, totalIndices);

    if (merged.vertices == NULL) {
        return merged;
    }

    memcpy(merged.vertices, a.vertices, a.vertexCount * sizeof(*merged.vertices));
    memcpy(merged.vertices + a.vertexCount, b.vertices, b.vertexCount * sizeof(*merged.vertices));

    if (a.indexCount > 0 && a.indices != NULL) {
        memcpy(merged.indices, a.indices, a.indexCount * sizeof(*merged.indices));
    }

    if (b.indexCount > 0 && b.indices != NULL) {
        for (int i = 0; i < b.indexCount; i++) {
            merged.indices[a.indexCount + i] = b.indices[i] + a.vertexCount;
        }
    }

    return merged;
}

void R3D_TranslateMeshData(R3D_MeshData* meshData, Vector3 translation)
{
    if (meshData == NULL || meshData->vertices == NULL) return;
    
    for (int i = 0; i < meshData->vertexCount; i++) {
        meshData->vertices[i].position.x += translation.x;
        meshData->vertices[i].position.y += translation.y;
        meshData->vertices[i].position.z += translation.z;
    }
}

void R3D_RotateMeshData(R3D_MeshData* meshData, Quaternion rotation)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    for (int i = 0; i < meshData->vertexCount; i++)
    {
        meshData->vertices[i].position = Vector3RotateByQuaternion(meshData->vertices[i].position, rotation);
        meshData->vertices[i].normal = Vector3RotateByQuaternion(meshData->vertices[i].normal, rotation);

        // Preserve w component for handedness
        Vector3 tangentVec = (Vector3) {
            meshData->vertices[i].tangent.x, 
            meshData->vertices[i].tangent.y, 
            meshData->vertices[i].tangent.z
        };
        tangentVec = Vector3RotateByQuaternion(tangentVec, rotation);

        meshData->vertices[i].tangent.x = tangentVec.x;
        meshData->vertices[i].tangent.y = tangentVec.y;
        meshData->vertices[i].tangent.z = tangentVec.z;
    }
}

void R3D_ScaleMeshData(R3D_MeshData* meshData, Vector3 scale)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    for (int i = 0; i < meshData->vertexCount; i++) {
        meshData->vertices[i].position.x *= scale.x;
        meshData->vertices[i].position.y *= scale.y;
        meshData->vertices[i].position.z *= scale.z;
    }

    if (scale.x != scale.y || scale.y != scale.z) {
        R3D_GenMeshDataNormals(meshData);
        R3D_GenMeshDataTangents(meshData);
    }
}

void R3D_GenMeshDataUVsPlanar(R3D_MeshData* meshData, Vector2 uvScale, Vector3 axis)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    axis = Vector3Normalize(axis);

    Vector3 up = (fabsf(axis.y) < 0.999f) ? (Vector3) {0, 1, 0 } : (Vector3) {1, 0, 0};
    Vector3 tangent = Vector3Normalize(Vector3CrossProduct(up, axis));
    Vector3 bitangent = Vector3CrossProduct(axis, tangent);
    
    for (int i = 0; i < meshData->vertexCount; i++) {
        Vector3 pos = meshData->vertices[i].position;
        float u = Vector3DotProduct(pos, tangent) * uvScale.x;
        float v = Vector3DotProduct(pos, bitangent) * uvScale.y;
        meshData->vertices[i].texcoord = (Vector2) {u, v};
    }
}

void R3D_GenMeshDataUVsSpherical(R3D_MeshData* meshData)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    for (int i = 0; i < meshData->vertexCount; i++) {
        Vector3 pos = Vector3Normalize(meshData->vertices[i].position);
        float u = 0.5f + atan2f(pos.z, pos.x) / (2.0f * PI);
        float v = 0.5f - asinf(pos.y) * (1.0f / PI);
        meshData->vertices[i].texcoord = (Vector2) {u, v};
    }
}

void R3D_GenMeshDataUVsCylindrical(R3D_MeshData* meshData)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    for (int i = 0; i < meshData->vertexCount; i++) {
        Vector3 pos = meshData->vertices[i].position;
        float u = 0.5f + atan2f(pos.z, pos.x) / (2.0f * PI);
        float v = pos.y;
        meshData->vertices[i].texcoord = (Vector2) {u, v};
    }
}

void R3D_GenMeshDataNormals(R3D_MeshData* meshData)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    for (int i = 0; i < meshData->vertexCount; i++) {
        meshData->vertices[i].normal = (Vector3) {0};
    }

    if (meshData->indexCount > 0 && meshData->indices != NULL) {
        for (int i = 0; i < meshData->indexCount; i += 3)
        {
            uint32_t i0 = meshData->indices[i];
            uint32_t i1 = meshData->indices[i + 1];
            uint32_t i2 = meshData->indices[i + 2];

            Vector3 v0 = meshData->vertices[i0].position;
            Vector3 v1 = meshData->vertices[i1].position;
            Vector3 v2 = meshData->vertices[i2].position;

            Vector3 edge1 = Vector3Subtract(v1, v0);
            Vector3 edge2 = Vector3Subtract(v2, v0);
            Vector3 faceNormal = Vector3CrossProduct(edge1, edge2);

            meshData->vertices[i0].normal = Vector3Add(meshData->vertices[i0].normal, faceNormal);
            meshData->vertices[i1].normal = Vector3Add(meshData->vertices[i1].normal, faceNormal);
            meshData->vertices[i2].normal = Vector3Add(meshData->vertices[i2].normal, faceNormal);
        }
    }
    else {
        for (int i = 0; i < meshData->vertexCount; i += 3)
        {
            Vector3 v0 = meshData->vertices[i].position;
            Vector3 v1 = meshData->vertices[i + 1].position;
            Vector3 v2 = meshData->vertices[i + 2].position;

            Vector3 edge1 = Vector3Subtract(v1, v0);
            Vector3 edge2 = Vector3Subtract(v2, v0);
            Vector3 faceNormal = Vector3CrossProduct(edge1, edge2);

            meshData->vertices[i + 0].normal = Vector3Add(meshData->vertices[i + 0].normal, faceNormal);
            meshData->vertices[i + 1].normal = Vector3Add(meshData->vertices[i + 1].normal, faceNormal);
            meshData->vertices[i + 2].normal = Vector3Add(meshData->vertices[i + 2].normal, faceNormal);
        }
    }

    for (int i = 0; i < meshData->vertexCount; i++) {
        meshData->vertices[i].normal = Vector3Normalize(meshData->vertices[i].normal);
    }
}

void R3D_GenMeshDataTangents(R3D_MeshData* meshData)
{
    if (meshData == NULL || meshData->vertices == NULL) return;

    Vector3* bitangents = RL_CALLOC(meshData->vertexCount, sizeof(Vector3));
    if (bitangents == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to allocate memory for tangent calculation");
        return;
    }

    for (int i = 0; i < meshData->vertexCount; i++) {
        meshData->vertices[i].tangent = (Vector4) {0};
    }

    #define PROCESS_TRIANGLE(i0, i1, i2)                                \
    do {                                                                \
        Vector3 v0 = meshData->vertices[i0].position;                   \
        Vector3 v1 = meshData->vertices[i1].position;                   \
        Vector3 v2 = meshData->vertices[i2].position;                   \
                                                                        \
        Vector2 uv0 = meshData->vertices[i0].texcoord;                  \
        Vector2 uv1 = meshData->vertices[i1].texcoord;                  \
        Vector2 uv2 = meshData->vertices[i2].texcoord;                  \
                                                                        \
        Vector3 edge1 = Vector3Subtract(v1, v0);                        \
        Vector3 edge2 = Vector3Subtract(v2, v0);                        \
                                                                        \
        Vector2 deltaUV1 = Vector2Subtract(uv1, uv0);                   \
        Vector2 deltaUV2 = Vector2Subtract(uv2, uv0);                   \
                                                                        \
        float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;  \
                                                                        \
        /* Skip the degenerate cases (collinear UVs) */                 \
        if (fabsf(det) < 1e-6f) {                                       \
            break;                                                      \
        }                                                               \
                                                                        \
        float invDet = 1.0f / det;                                      \
                                                                        \
        Vector3 tangent = {                                             \
            invDet * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),     \
            invDet * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),     \
            invDet * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)      \
        };                                                              \
                                                                        \
        Vector3 bitangent = {                                           \
            invDet * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x),    \
            invDet * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y),    \
            invDet * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z)     \
        };                                                              \
                                                                        \
        meshData->vertices[i0].tangent.x += tangent.x;                  \
        meshData->vertices[i0].tangent.y += tangent.y;                  \
        meshData->vertices[i0].tangent.z += tangent.z;                  \
                                                                        \
        meshData->vertices[i1].tangent.x += tangent.x;                  \
        meshData->vertices[i1].tangent.y += tangent.y;                  \
        meshData->vertices[i1].tangent.z += tangent.z;                  \
                                                                        \
        meshData->vertices[i2].tangent.x += tangent.x;                  \
        meshData->vertices[i2].tangent.y += tangent.y;                  \
        meshData->vertices[i2].tangent.z += tangent.z;                  \
                                                                        \
        bitangents[i0].x += bitangent.x;                                \
        bitangents[i0].y += bitangent.y;                                \
        bitangents[i0].z += bitangent.z;                                \
                                                                        \
        bitangents[i1].x += bitangent.x;                                \
        bitangents[i1].y += bitangent.y;                                \
        bitangents[i1].z += bitangent.z;                                \
                                                                        \
        bitangents[i2].x += bitangent.x;                                \
        bitangents[i2].y += bitangent.y;                                \
        bitangents[i2].z += bitangent.z;                                \
    } while(0);

    if (meshData->indexCount > 0 && meshData->indices != NULL) {
        for (int i = 0; i < meshData->indexCount; i += 3) {
            PROCESS_TRIANGLE(
                meshData->indices[i],
                meshData->indices[i + 1],
                meshData->indices[i + 2]
            );
        }
    }
    else {
        for (int i = 0; i < meshData->vertexCount; i += 3) {
            PROCESS_TRIANGLE(i, i + 1, i + 2);
        }
    }

    #undef PROCESS_TRIANGLE

    // Orthogonalization (Gram-Schmidt) and handedness calculation
    for (int i = 0; i < meshData->vertexCount; i++)
    {
        Vector3 n = meshData->vertices[i].normal;
        Vector3 t = {
            meshData->vertices[i].tangent.x,
            meshData->vertices[i].tangent.y,
            meshData->vertices[i].tangent.z
        };

        // Gram-Schmidt orthogonalization
        t = Vector3Subtract(t, Vector3Scale(n, Vector3DotProduct(n, t)));
        float tLength = Vector3Length(t);
        if (tLength > 1e-6f) {
            t = Vector3Scale(t, 1.0f / tLength);
        }
        else {
            // Fallback: generate an arbitrary tangent perpendicular to the normal
            t = fabsf(n.x) < 0.9f ? (Vector3) {1.0f, 0.0f, 0.0f } : (Vector3) {0.0f, 1.0f, 0.0f };
            t = Vector3Normalize(Vector3Subtract(t, Vector3Scale(n, Vector3DotProduct(n, t))));
        }

        float handedness = (Vector3DotProduct(Vector3CrossProduct(n, t), bitangents[i]) < 0.0f) ? -1.0f : 1.0f;
        meshData->vertices[i].tangent = (Vector4) {t.x, t.y, t.z, handedness };
    }

    RL_FREE(bitangents);
}

BoundingBox R3D_CalculateMeshDataBoundingBox(R3D_MeshData meshData)
{
    BoundingBox bounds = {0};

    if (meshData.vertices == NULL || meshData.vertexCount == 0) {
        return bounds;
    }

    bounds.min = meshData.vertices[0].position;
    bounds.max = meshData.vertices[0].position;

    for (int i = 1; i < meshData.vertexCount; i++) {
        Vector3 pos = meshData.vertices[i].position;
        bounds.min = Vector3Min(bounds.min, pos);
        bounds.max = Vector3Max(bounds.max, pos);
    }

    return bounds;
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

bool alloc_mesh(R3D_MeshData* meshData, int vertexCount, int indexCount)
{
    meshData->vertices = RL_CALLOC(vertexCount, sizeof(*meshData->vertices));
    meshData->indices = RL_CALLOC(indexCount, sizeof(*meshData->indices));

    if (!meshData->vertices || !meshData->indices) {
        if (meshData->vertices) RL_FREE(meshData->vertices);
        if (meshData->indices) RL_FREE(meshData->indices);
        return false;
    }

    meshData->vertexCount = vertexCount;
    meshData->indexCount = indexCount;

    return true;
}

void gen_cube_face(
    R3D_Vertex **vertexPtr,
    uint32_t **indexPtr,
    uint32_t *vertexOffset,
    Vector3 origin,
    Vector3 uAxis,
    Vector3 vAxis,
    float uSize,
    float vSize,
    int uRes,
    int vRes,
    Vector3 normal,
    Vector3 tangent
)
{
    float invU = 1.0f / (float)uRes;
    float invV = 1.0f / (float)vRes;
    Vector4 tangent4 = { tangent.x, tangent.y, tangent.z, 1.0f };

    uint32_t faceVertStart = *vertexOffset;
    R3D_Vertex *vertex = *vertexPtr;
    uint32_t *index = *indexPtr;

    for (int v = 0; v <= vRes; v++)
    {
        float vt = v * invV;
        float localV = (vt - 0.5f) * vSize;

        for (int u = 0; u <= uRes; u++)
        {
            float ut = u * invU;
            float localU = (ut - 0.5f) * uSize;

            vertex->position = (Vector3){
                origin.x + localU * uAxis.x + localV * vAxis.x,
                origin.y + localU * uAxis.y + localV * vAxis.y,
                origin.z + localU * uAxis.z + localV * vAxis.z
            };

            vertex->texcoord = (Vector2){ ut, vt };
            vertex->normal   = normal;
            vertex->color    = WHITE;
            vertex->tangent  = tangent4;

            vertex++;
            (*vertexOffset)++;
        }
    }

    for (int v = 0; v < vRes; v++)
    {
        uint32_t rowStart     = faceVertStart + v * (uRes + 1);
        uint32_t nextRowStart = rowStart + (uRes + 1);

        for (int u = 0; u < uRes; u++)
        {
            uint32_t i0 = rowStart + u;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = nextRowStart + u;
            uint32_t i3 = i2 + 1;

            *index++ = i0; *index++ = i1; *index++ = i2;
            *index++ = i2; *index++ = i1; *index++ = i3;
        }
    }

    *vertexPtr = vertex;
    *indexPtr  = index;
}
