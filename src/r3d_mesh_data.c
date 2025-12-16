/* r3d_mesh_data.c -- R3D Mesh Data Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_mesh_data.h>
#include <raymath.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

// ========================================
// PUBLIC API
// ========================================

R3D_MeshData R3D_CreateMeshData(int vertexCount, int indexCount)
{
    R3D_MeshData meshData = {0};

    if (vertexCount <= 0) {
        TraceLog(LOG_ERROR, "R3D: Invalid vertex count for mesh creation");
        return meshData;
    }

    meshData.vertices = RL_CALLOC(vertexCount, sizeof(*meshData.vertices));
    if (meshData.vertices == NULL) {
        TraceLog(LOG_ERROR, "R3D: Failed to allocate memory for mesh vertices");
        return meshData;
    }

    meshData.vertexCount = vertexCount;

    if (indexCount > 0) {
        meshData.indices = RL_CALLOC(indexCount, sizeof(*meshData.indices));
        if (meshData.indices == NULL) {
            TraceLog(LOG_ERROR, "R3D: Failed to allocate memory for mesh indices");
            RL_FREE(meshData.vertices);
            meshData.vertices = NULL;
            meshData.vertexCount = 0;
            return meshData;
        }
        meshData.indexCount = indexCount;
    }

    return meshData;
}

void R3D_UnloadMeshData(R3D_MeshData* meshData)
{
    RL_FREE(meshData->vertices);
    RL_FREE(meshData->indices);
}

bool R3D_IsMeshDataValid(const R3D_MeshData* meshData)
{
    return (meshData->vertices != NULL) && (meshData->vertexCount > 0);
}

R3D_MeshData R3D_GenMeshDataPoly(int sides, float radius)
{
    R3D_MeshData meshData = {0};

    // Validation of parameters
    if (sides < 3 || radius <= 0.0f) return meshData;

    // Memory allocation
    // For a polygon: 1 central vertex + peripheral vertices
    meshData.vertexCount = sides + 1;
    meshData.indexCount = sides * 3; // sides triangles, 3 indices per triangle

    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Pre-compute some values
    const float angleStep = 2.0f * PI / sides;

    // Central vertex (index 0)
    meshData.vertices[0] = (R3D_Vertex){
        .position = {0.0f, 0.0f, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = {0.0f, 1.0f, 0.0f},
        .color = {255, 255, 255, 255},
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
    };

    for (int i = 0; i < sides; i++) {
        const float angle = i * angleStep;
        const float cosAngle = cosf(angle);
        const float sinAngle = sinf(angle);

        // Position on the circle
        const float x = radius * cosAngle;
        const float y = radius * sinAngle;

        // Peripheral vertex
        meshData.vertices[i + 1] = (R3D_Vertex){
            .position = {x, y, 0.0f},
            .texcoord = {
                0.5f + 0.5f * cosAngle, // Circular UV mapping
                0.5f + 0.5f * sinAngle
            },
            .normal = {0.0f, 1.0f, 0.0f},
            .color = {255, 255, 255, 255},
            .tangent = {-sinAngle, cosAngle, 0.0f, 1.0f} // Tangent perpendicular to the radius
        };

        // Indices for the triangle (center, current vertex, next vertex)
        const int baseIdx = i * 3;
        meshData.indices[baseIdx] = 0; // Center
        meshData.indices[baseIdx + 1] = i + 1; // Current vertex
        meshData.indices[baseIdx + 2] = (i + 1) % sides + 1; // Next vertex (with wrap)
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataPlane(float width, float length, int resX, int resZ)
{
    R3D_MeshData meshData = { 0 };

    // Validation of parameters
    if (width <= 0.0f || length <= 0.0f || resX < 1 || resZ < 1) return meshData;

    // Calculating grid dimensions
    const int verticesPerRow = resX + 1;
    const int verticesPerCol = resZ + 1;
    meshData.vertexCount = verticesPerRow * verticesPerCol;
    meshData.indexCount = resX * resZ * 6; // 2 triangles per quad, 3 indices per triangle

    // Memory allocation
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Pre-compute some values
    const float halfWidth = width * 0.5f;
    const float halfLength = length * 0.5f;
    const float stepX = width / resX;
    const float stepZ = length / resZ;
    const float uvStepX = 1.0f / resX;
    const float uvStepZ = 1.0f / resZ;

    // Vertex generation
    int vertexIndex = 0;
    for (int z = 0; z <= resZ; z++) {
        const float posZ = -halfLength + z * stepZ;
        const float uvZ = (float)z * uvStepZ;
    
        for (int x = 0; x <= resX; x++) {
            const float posX = -halfWidth + x * stepX;
            const float uvX = (float)x * uvStepX;
        
            meshData.vertices[vertexIndex] = (R3D_Vertex){
                .position = {posX, 0.0f, posZ},
                .texcoord = {uvX, uvZ},
                .normal = {0.0f, 1.0f, 0.0f},
                .color = {255, 255, 255, 255},
                .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
            };
            vertexIndex++;
        }
    }

    // Generation of indices (counter-clockwise order)
    int indexOffset = 0;
    for (int z = 0; z < resZ; z++) {
        const int rowStart = z * verticesPerRow;
        const int nextRowStart = (z + 1) * verticesPerRow;
    
        for (int x = 0; x < resX; x++) {
            // Clues from the 4 corners of the quad
            const uint32_t topLeft = rowStart + x;
            const uint32_t topRight = rowStart + x + 1;
            const uint32_t bottomLeft = nextRowStart + x;
            const uint32_t bottomRight = nextRowStart + x + 1;
        
            // First triangle (topLeft, bottomLeft, topRight)
            meshData.indices[indexOffset++] = topLeft;
            meshData.indices[indexOffset++] = bottomLeft;
            meshData.indices[indexOffset++] = topRight;
        
            // Second triangle (topRight, bottomLeft, bottomRight)
            meshData.indices[indexOffset++] = topRight;
            meshData.indices[indexOffset++] = bottomLeft;
            meshData.indices[indexOffset++] = bottomRight;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCube(float width, float height, float length)
{
    R3D_MeshData meshData = { 0 };

    // Validation of parameters
    if (width <= 0.0f || height <= 0.0f || length <= 0.0f) return meshData;

    // Cube dimensions
    meshData.vertexCount = 24; // 4 vertices per face, 6 faces
    meshData.indexCount = 36;  // 2 triangles per face, 3 indices per triangle, 6 faces

    // Memory allocation
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Pre-compute some values
    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;
    const float halfL = length * 0.5f;

    // Standard UV coordinates for each face
    const Vector2 uvs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f},
        {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    // Generation of the 6 faces of the cube
    int vertexOffset = 0;
    int indexOffset = 0;

    // Back face (+Z)
    const Vector3 frontNormal = {0.0f, 0.0f, 1.0f};
    const Vector4 frontTangent = {1.0f, 0.0f, 0.0f, 1.0f};
    meshData.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[0], frontNormal, {255, 255, 255, 255}, frontTangent};
    meshData.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[1], frontNormal, {255, 255, 255, 255}, frontTangent};
    meshData.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, halfH, halfL}, uvs[2], frontNormal, {255, 255, 255, 255}, frontTangent};
    meshData.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[3], frontNormal, {255, 255, 255, 255}, frontTangent};
    vertexOffset += 4;

    // Front face (-Z)
    const Vector3 backNormal = {0.0f, 0.0f, -1.0f};
    const Vector4 backTangent = {-1.0f, 0.0f, 0.0f, 1.0f};
    meshData.vertices[vertexOffset + 0] = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[0], backNormal, {255, 255, 255, 255}, backTangent};
    meshData.vertices[vertexOffset + 1] = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[1], backNormal, {255, 255, 255, 255}, backTangent};
    meshData.vertices[vertexOffset + 2] = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[2], backNormal, {255, 255, 255, 255}, backTangent};
    meshData.vertices[vertexOffset + 3] = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[3], backNormal, {255, 255, 255, 255}, backTangent};
    vertexOffset += 4;

    // Right face (+X)
    const Vector3 rightNormal = {1.0f, 0.0f, 0.0f};
    const Vector4 rightTangent = {0.0f, 0.0f, -1.0f, 1.0f};
    meshData.vertices[vertexOffset + 0] = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[0], rightNormal, {255, 255, 255, 255}, rightTangent};
    meshData.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[1], rightNormal, {255, 255, 255, 255}, rightTangent};
    meshData.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[2], rightNormal, {255, 255, 255, 255}, rightTangent};
    meshData.vertices[vertexOffset + 3] = (R3D_Vertex){{halfW, halfH, halfL}, uvs[3], rightNormal, {255, 255, 255, 255}, rightTangent};
    vertexOffset += 4;

    // Left face (-X)
    const Vector3 leftNormal = {-1.0f, 0.0f, 0.0f};
    const Vector4 leftTangent = {0.0f, 0.0f, 1.0f, 1.0f};
    meshData.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[0], leftNormal, {255, 255, 255, 255}, leftTangent};
    meshData.vertices[vertexOffset + 1] = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[1], leftNormal, {255, 255, 255, 255}, leftTangent};
    meshData.vertices[vertexOffset + 2] = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[2], leftNormal, {255, 255, 255, 255}, leftTangent};
    meshData.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[3], leftNormal, {255, 255, 255, 255}, leftTangent};
    vertexOffset += 4;

    // Face up (+Y)
    const Vector3 topNormal = {0.0f, 1.0f, 0.0f};
    const Vector4 topTangent = {1.0f, 0.0f, 0.0f, 1.0f};
    meshData.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[0], topNormal, {255, 255, 255, 255}, topTangent};
    meshData.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, halfH, halfL}, uvs[1], topNormal, {255, 255, 255, 255}, topTangent};
    meshData.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[2], topNormal, {255, 255, 255, 255}, topTangent};
    meshData.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[3], topNormal, {255, 255, 255, 255}, topTangent};
    vertexOffset += 4;

    // Face down (-Y)
    const Vector3 bottomNormal = {0.0f, -1.0f, 0.0f};
    const Vector4 bottomTangent = {1.0f, 0.0f, 0.0f, 1.0f};
    meshData.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[0], bottomNormal, {255, 255, 255, 255}, bottomTangent};
    meshData.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[1], bottomNormal, {255, 255, 255, 255}, bottomTangent};
    meshData.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[2], bottomNormal, {255, 255, 255, 255}, bottomTangent};
    meshData.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[3], bottomNormal, {255, 255, 255, 255}, bottomTangent};

    // Generation of indices (same pattern for each face)
    for (int face = 0; face < 6; face++) {
        const uint32_t baseVertex = face * 4;
        const int baseIndex = face * 6;
    
        // First triangle (0, 1, 2)
        meshData.indices[baseIndex + 0] = baseVertex + 0;
        meshData.indices[baseIndex + 1] = baseVertex + 1;
        meshData.indices[baseIndex + 2] = baseVertex + 2;
    
        // Second triangle (2, 3, 0)
        meshData.indices[baseIndex + 3] = baseVertex + 2;
        meshData.indices[baseIndex + 4] = baseVertex + 3;
        meshData.indices[baseIndex + 5] = baseVertex + 0;
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataSphere(float radius, int rings, int slices)
{
    R3D_MeshData meshData = { 0 };

    // Parameter validation
    if (radius <= 0.0f || rings < 2 || slices < 3) return meshData;

    // Calculate meshData dimensions
    meshData.vertexCount = (rings + 1) * (slices + 1);
    meshData.indexCount = rings * slices * 6; // 2 triangles per quad

    // Allocate memory for vertices and indices
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Pre-calculate angular steps and default color
    const float ringStep = PI / rings;        // Vertical angle increment (phi: 0 to PI)
    const float sliceStep = 2.0f * PI / slices; // Horizontal angle increment (theta: 0 to 2PI)

    // Generate vertices
    int vertexIndex = 0;
    for (int ring = 0; ring <= rings; ring++) {
        const float phi = ring * ringStep;          // Vertical angle from +Y (North Pole)
        const float sinPhi = sinf(phi);
        const float cosPhi = cosf(phi);

        const float y = radius * cosPhi;            // Y-coordinate (up/down)
        const float ringRadius = radius * sinPhi;   // Radius of the current ring

        const float v = (float)ring / rings;        // V texture coordinate (0 at North Pole, 1 at South Pole)
    
        for (int slice = 0; slice <= slices; slice++) {
            const float theta = slice * sliceStep;   // Horizontal angle (around Y-axis)
            const float sinTheta = sinf(theta);
            const float cosTheta = cosf(theta);
        
            // Calculate vertex position for right-handed, -Z forward, +Y up system
            const float x = ringRadius * cosTheta;
            const float z = ringRadius * -sinTheta; // Invert Z for -Z forward
            
            // Normals point outwards from the sphere center
            const Vector3 normal = {x / radius, y / radius, z / radius};
        
            // UV coordinates
            const float u = (float)slice / slices;
        
            // Calculate tangent vector (points in the direction of increasing U)
            // Adjusted for -Z forward system: tangent = d(position)/d(theta) normalized
            const Vector3 tangentDir = {-sinTheta, 0.0f, -cosTheta};
            const Vector4 tangent = {tangentDir.x, tangentDir.y, tangentDir.z, 1.0f}; // W for bitangent handedness
        
            meshData.vertices[vertexIndex++] = (R3D_Vertex){
                .position = {x, y, z},
                .texcoord = {u, v},
                .normal = normal,
                .color = {255, 255, 255, 255},
                .tangent = tangent
            };
        }
    }

    // Generate indices
    int indexOffset = 0;
    const int verticesPerRing = slices + 1;

    for (int ring = 0; ring < rings; ring++) {
        const int currentRingStartIdx = ring * verticesPerRing;
        const int nextRingStartIdx = (ring + 1) * verticesPerRing;
    
        for (int slice = 0; slice < slices; slice++) {
            // Get indices of the 4 corners of the quad
            const uint32_t current = currentRingStartIdx + slice;
            const uint32_t next = currentRingStartIdx + slice + 1;
            const uint32_t currentNext = nextRingStartIdx + slice;
            const uint32_t nextNext = nextRingStartIdx + slice + 1;
        
            // Define triangles with clockwise winding for back-face culling (right-handed system)
            // First triangle of the quad
            meshData.indices[indexOffset++] = current;
            meshData.indices[indexOffset++] = currentNext;
            meshData.indices[indexOffset++] = nextNext;

            // Second triangle of the quad
            meshData.indices[indexOffset++] = current;
            meshData.indices[indexOffset++] = nextNext;
            meshData.indices[indexOffset++] = next;
        }
    }

    // Set final index count
    meshData.indexCount = indexOffset;

    return meshData;
}

R3D_MeshData R3D_GenMeshDataHemiSphere(float radius, int rings, int slices)
{
    R3D_MeshData meshData = { 0 };

    // Parameter validation
    if (radius <= 0.0f || rings < 1 || slices < 3) return meshData;

    // Calculate vertex counts for hemisphere and base
    const int hemisphereVertexCount = (rings + 1) * (slices + 1);
    const int baseVertexCount = slices + 1; // Circular base includes center + points on edge
    meshData.vertexCount = hemisphereVertexCount + baseVertexCount;

    // Calculate index counts for hemisphere and base
    const int hemisphereIndexCount = rings * slices * 6; // 2 triangles per quad
    const int baseIndexCount = slices * 3;               // 1 triangle per slice for the base
    meshData.indexCount = hemisphereIndexCount + baseIndexCount;

    // Allocate memory
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Pre-compute angles and default color
    const float ringStep = (PI * 0.5f) / rings;   // Vertical angle increment (phi: 0 to PI/2 for hemisphere)
    const float sliceStep = 2.0f * PI / slices;   // Horizontal angle increment (theta: 0 to 2PI)

    // Generate hemisphere vertices
    int vertexIndex = 0;
    for (int ring = 0; ring <= rings; ring++) {
        const float phi = ring * ringStep;          // Vertical angle (0 at +Y, PI/2 at Y=0)
        const float sinPhi = sinf(phi);
        const float cosPhi = cosf(phi);
        const float y = radius * cosPhi;            // Y-position (radius down to 0)
        const float ringRadius = radius * sinPhi;   // Radius of the current ring

        const float v = (float)ring / rings;        // V texture coordinate
    
        for (int slice = 0; slice <= slices; slice++) {
            const float theta = slice * sliceStep;   // Horizontal angle
            const float sinTheta = sinf(theta);
            const float cosTheta = cosf(theta);
        
            // Position for right-handed, -Z forward, +Y up
            const float x = ringRadius * cosTheta;
            const float z = ringRadius * -sinTheta; // Invert Z for -Z forward
        
            // Normal (points outwards from the sphere center)
            const Vector3 normal = {x / radius, y / radius, z / radius};
        
            // UV coordinates
            const float u = (float)slice / slices;
        
            // Tangent: adjusted for -Z forward (derivative of position wrt theta)
            const Vector3 tangentDir = {-sinTheta, 0.0f, -cosTheta};
            const Vector4 tangent = {tangentDir.x, tangentDir.y, tangentDir.z, 1.0f};
        
            meshData.vertices[vertexIndex++] = (R3D_Vertex){
                .position = {x, y, z},
                .texcoord = {u, v},
                .normal = normal,
                .color = {255, 255, 255, 255},
                .tangent = tangent
            };
        }
    }

    // Generate base vertices (at y = 0)
    // The base needs a central vertex and vertices around the edge.
    // Let's make the first vertex of the base ring the center.
    const int baseCenterVertexIndex = vertexIndex;
    meshData.vertices[vertexIndex++] = (R3D_Vertex){
        .position = {0.0f, 0.0f, 0.0f},
        .texcoord = {0.5f, 0.5f}, // Center of UV map
        .normal = {0.0f, -1.0f, 0.0f}, // Normal pointing downwards
        .color = {255, 255, 255, 255},
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f} // Arbitrary tangent for a flat surface
    };

    // Then, the perimeter vertices for the base
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float sinTheta = sinf(theta);
        const float cosTheta = cosf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * -sinTheta; // Invert Z for consistency
    
        // Circular UV mapping for the base
        const float u = 0.5f + 0.5f * cosTheta;
        const float v = 0.5f + 0.5f * -sinTheta; // Invert V based on Z inversion if desired
                                                 // Or keep it standard for circular mapping: 0.5f + 0.5f * sinTheta;
                                                 // Let's assume standard circular mapping (no direct link to -Z)
    
        meshData.vertices[vertexIndex++] = (R3D_Vertex) {
            .position = {x, 0.0f, z},
            .texcoord = {u, v},
            .normal = {0.0f, -1.0f, 0.0f}, // Normal pointing downwards
            .color = {255, 255, 255, 255},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f} // Tangent for flat base
        };
    }

    // Generate indices for the hemisphere
    int indexOffset = 0;
    const int verticesPerRing = slices + 1;

    for (int ring = 0; ring < rings; ring++) {
        const int currentRingStartIdx = ring * verticesPerRing;
        const int nextRingStartIdx = (ring + 1) * verticesPerRing;
    
        for (int slice = 0; slice < slices; slice++) {
            const uint32_t current = currentRingStartIdx + slice;
            const uint32_t next = currentRingStartIdx + slice + 1;
            const uint32_t currentNext = nextRingStartIdx + slice;
            const uint32_t nextNext = nextRingStartIdx + slice + 1;
        
            // Triangles with clockwise winding for back-face culling (right-handed system)
            // First triangle of the quad
            meshData.indices[indexOffset++] = current;
            meshData.indices[indexOffset++] = currentNext;
            meshData.indices[indexOffset++] = nextNext;

            // Second triangle of the quad
            meshData.indices[indexOffset++] = current;
            meshData.indices[indexOffset++] = nextNext;
            meshData.indices[indexOffset++] = next;
        }
    }

    // Generate indices for the base
    // The first vertex of the base section (baseCenterVertexIndex) is the center point.
    // The subsequent vertices form the perimeter.
    for (int slice = 0; slice < slices; slice++) {
        // Base vertices start after hemisphere vertices and the center vertex
        const uint32_t currentPerimeter = hemisphereVertexCount + 1 + slice; // +1 to skip center vertex
        const uint32_t nextPerimeter = hemisphereVertexCount + 1 + slice + 1;
    
        // Triangle for the base (clockwise winding when viewed from below, i.e., normal direction)
        meshData.indices[indexOffset++] = currentPerimeter;
        meshData.indices[indexOffset++] = baseCenterVertexIndex; // Center vertex
        meshData.indices[indexOffset++] = nextPerimeter;
    }

    // Set final total index count
    meshData.indexCount = indexOffset;

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCylinder(float radius, float height, int slices)
{
    R3D_MeshData meshData = { 0 };

    // Validate parameters
    if (radius <= 0.0f || height <= 0.0f || slices < 3) return meshData;

    // Calculate vertex and index counts
    // Body vertices: 2 rows * (slices+1) vertices (top and bottom per slice)
    // Cap vertices: 2 * (1 center + slices perimeter vertices)
    const int bodyVertexCount = 2 * (slices + 1);
    const int capVertexCount = 2 * (1 + slices);
    meshData.vertexCount = bodyVertexCount + capVertexCount;

    // Indices: body + 2 caps
    const int bodyIndexCount = slices * 6;      // 2 triangles per slice
    const int capIndexCount = 2 * slices * 3;   // slices triangles per cap
    meshData.indexCount = bodyIndexCount + capIndexCount;

    // Allocate memory
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Pre-compute values
    const float halfHeight = height * 0.5f;
    // For -Z forward, +Y up: theta starts at +X and increases toward -Z (clockwise when viewed from above)
    const float sliceStep = 2.0f * PI / slices;

    // Generate body vertices
    int vertexIndex = 0;

    // Bottom row (y = -halfHeight)
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        // For -Z forward: x = cos(theta), z = -sin(theta)
        // This makes theta=0 point toward +X, theta=PI/2 toward -Z
        const float x = radius * cosf(theta);
        const float z = -radius * sinf(theta);

        // Normal: points radially outward
        const Vector3 normal = {cosf(theta), 0.0f, -sinf(theta)};

        // UV mapping: u = angular position, v = height
        const float u = (float)slice / slices;
        const float v = 0.0f; // Bottom of cylinder

        // Tangent: perpendicular to the normal and along the circumference
        // To be consistent with -Z forward, tangent is in the direction of increasing theta
        // If normal is (cos(θ), 0, -sin(θ)), then tangent is (-sin(θ), 0, -cos(θ))
        const Vector4 tangent = {-sinf(theta), 0.0f, -cosf(theta), 1.0f};

        meshData.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = normal,
            .color = {255, 255, 255, 255},
            .tangent = tangent
        };
        vertexIndex++;
    }

    // Top row (y = halfHeight)
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float x = radius * cosf(theta);
        const float z = -radius * sinf(theta);

        // Normal: points outwards
        const Vector3 normal = {cosf(theta), 0.0f, -sinf(theta)};

        const float u = (float)slice / slices;
        const float v = 1.0f; // Top of cylinder

        // Tangent: consistent with bottom row
        const Vector4 tangent = {-sinf(theta), 0.0f, -cosf(theta), 1.0f};

        meshData.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, halfHeight, z},
            .texcoord = {u, v},
            .normal = normal,
            .color = {255, 255, 255, 255},
            .tangent = tangent
        };
        vertexIndex++;
    }

    // Generate bottom cap vertices
    // Normal points downwards
    const Vector3 bottomNormal = {0.0f, -1.0f, 0.0f};
    // Tangent for a flat surface facing down, +X is a valid tangent
    const Vector4 bottomTangent = {1.0f, 0.0f, 0.0f, 1.0f};

    // Center of bottom cap
    meshData.vertices[vertexIndex] = (R3D_Vertex){
        .position = {0.0f, -halfHeight, 0.0f},
        .texcoord = {0.5f, 0.5f}, // Center of UV space
        .normal = bottomNormal,
        .color = {255, 255, 255, 255},
        .tangent = bottomTangent
    };
    const int bottomCenterIndex = vertexIndex;
    vertexIndex++;

    // Perimeter of bottom cap
    for (int slice = 0; slice < slices; slice++) {
        const float theta = slice * sliceStep;
        const float x = radius * cosf(theta);
        const float z = -radius * sinf(theta); // Consistent with -Z forward

        // Circular UV mapping
        const float u = 0.5f + 0.5f * cosf(theta);
        const float v = 0.5f - 0.5f * sinf(theta); // Flip V for -Z forward

        meshData.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = bottomNormal,
            .color = {255, 255, 255, 255},
            .tangent = bottomTangent
        };
        vertexIndex++;
    }

    // Generate top cap vertices
    // Normal points upwards
    const Vector3 topNormal = {0.0f, 1.0f, 0.0f};
    // Tangent for a flat surface facing up, +X is a valid tangent
    const Vector4 topTangent = {1.0f, 0.0f, 0.0f, 1.0f};

    // Center of top cap
    meshData.vertices[vertexIndex] = (R3D_Vertex){
        .position = {0.0f, halfHeight, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = topNormal,
        .color = {255, 255, 255, 255},
        .tangent = topTangent
    };
    const int topCenterIndex = vertexIndex;
    vertexIndex++;

    // Perimeter of top cap
    for (int slice = 0; slice < slices; slice++) {
        const float theta = slice * sliceStep;
        const float x = radius * cosf(theta);
        const float z = -radius * sinf(theta); // Consistent with -Z forward

        // Circular UV mapping
        const float u = 0.5f + 0.5f * cosf(theta);
        const float v = 0.5f - 0.5f * sinf(theta); // Flip V for -Z forward

        meshData.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, halfHeight, z},
            .texcoord = {u, v},
            .normal = topNormal,
            .color = {255, 255, 255, 255},
            .tangent = topTangent
        };
        vertexIndex++;
    }

    // Generate indices
    int indexOffset = 0;
    const int verticesPerRow = slices + 1; // Vertices in bottom and top rows of cylinder body

    // Body indices (CCW winding from outside)
    // For -Z forward, CCW order from outside remains the same
    for (int slice = 0; slice < slices; slice++) {
        const uint32_t bottomLeft = slice;
        const uint32_t bottomRight = slice + 1;
        const uint32_t topLeft = verticesPerRow + slice;
        const uint32_t topRight = verticesPerRow + slice + 1;

        // First triangle: bottomLeft -> bottomRight -> topRight (CCW from outside)
        meshData.indices[indexOffset++] = bottomLeft;
        meshData.indices[indexOffset++] = bottomRight;
        meshData.indices[indexOffset++] = topRight;

        // Second triangle: bottomLeft -> topRight -> topLeft (CCW from outside)
        meshData.indices[indexOffset++] = bottomLeft;
        meshData.indices[indexOffset++] = topRight;
        meshData.indices[indexOffset++] = topLeft;
    }

    // Bottom cap indices (CCW winding from normal's perspective: looking from -Y up)
    // With -Z forward, the order must be reversed to maintain correct winding
    const int bottomPerimeterStart = bottomCenterIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const uint32_t current = bottomPerimeterStart + slice;
        const uint32_t next = bottomPerimeterStart + (slice + 1) % slices;

        // Reverse order for -Z forward: center -> next -> current
        meshData.indices[indexOffset++] = bottomCenterIndex;
        meshData.indices[indexOffset++] = next;
        meshData.indices[indexOffset++] = current;
    }

    // Top cap indices (CCW winding from normal's perspective: looking from +Y down)
    // With -Z forward, the order must be reversed to maintain correct winding
    const int topPerimeterStart = topCenterIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const uint32_t current = topPerimeterStart + slice;
        const uint32_t next = topPerimeterStart + (slice + 1) % slices;

        // Reverse order for -Z forward: center -> current -> next
        meshData.indices[indexOffset++] = topCenterIndex;
        meshData.indices[indexOffset++] = current;
        meshData.indices[indexOffset++] = next;
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCone(float radius, float height, int slices)
{
    R3D_MeshData meshData = { 0 };

    // Validate parameters
    if (radius <= 0.0f || height <= 0.0f || slices < 3) return meshData;

    // Vertex counts
    // Side: slices+1 base vertices + 1 tip vertex
    // Base: 1 center vertex + slices perimeter
    const int sideVertexCount = slices + 1 + 1;  // base + tip
    const int baseVertexCount = 1 + slices;      // center + perimeter
    meshData.vertexCount = sideVertexCount + baseVertexCount;

    // Index counts
    // Side: slices triangles
    // Base: slices triangles
    const int sideIndexCount = slices * 3;
    const int baseIndexCount = slices * 3;
    meshData.indexCount = sideIndexCount + baseIndexCount;

    // Memory allocation
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    const float halfHeight = height * 0.5f;
    const float sliceStep = 2.0f * PI / slices;

    int vertexIndex = 0;

    // Base ring vertices (shared between side and base)
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float x = radius * cosf(theta);
        const float z = -radius * sinf(theta); // -Z forward

        const float u = (float)slice / slices;
        const float v = 0.0f;

        const Vector3 pos = {x, -halfHeight, z};
        const Vector3 normal = {cosf(theta), radius / height, -sinf(theta)};
        const float len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        const Vector3 norm = {normal.x / len, normal.y / len, normal.z / len};

        const Vector4 tangent = {-sinf(theta), 0.0f, -cosf(theta), 1.0f};

        meshData.vertices[vertexIndex++] = (R3D_Vertex){
            .position = pos,
            .texcoord = {u, 0.0f},
            .normal = norm,
            .color = {255, 255, 255, 255},
            .tangent = tangent
        };
    }

    // Tip of the cone (at y = +halfHeight)
    const int tipIndex = vertexIndex;
    meshData.vertices[vertexIndex++] = (R3D_Vertex){
        .position = {0.0f, halfHeight, 0.0f},
        .texcoord = {0.5f, 1.0f},
        .normal = {0.0f, 1.0f, 0.0f}, // Rough default normal
        .color = {255, 255, 255, 255},
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
    };

    // Base center vertex
    const int baseCenterIndex = vertexIndex;
    meshData.vertices[vertexIndex++] = (R3D_Vertex){
        .position = {0.0f, -halfHeight, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = {0.0f, -1.0f, 0.0f},
        .color = {255, 255, 255, 255},
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
    };

    // Base perimeter
    for (int slice = 0; slice < slices; slice++) {
        const float theta = slice * sliceStep;
        const float x = radius * cosf(theta);
        const float z = -radius * sinf(theta);

        const float u = 0.5f + 0.5f * cosf(theta);
        const float v = 0.5f - 0.5f * sinf(theta);

        meshData.vertices[vertexIndex++] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = {0.0f, -1.0f, 0.0f},
            .color = {255, 255, 255, 255},
            .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
        };
    }

    // Indices
    int index = 0;

    // Side triangles (each slice connects base to tip)
    for (int slice = 0; slice < slices; slice++) {
        const uint32_t base0 = slice;
        const uint32_t base1 = slice + 1;
        meshData.indices[index++] = base0;
        meshData.indices[index++] = base1;
        meshData.indices[index++] = tipIndex; // All triangles meet at the tip
    }

    // Base triangles (CCW order from below, so reversed here)
    const int basePerimeterStart = baseCenterIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const uint32_t curr = basePerimeterStart + slice;
        const uint32_t next = basePerimeterStart + ((slice + 1) % slices);
        meshData.indices[index++] = baseCenterIndex;
        meshData.indices[index++] = next;
        meshData.indices[index++] = curr;
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataTorus(float radius, float size, int radSeg, int sides)
{
    R3D_MeshData meshData = { 0 };

    if (radius <= 0.0f || size <= 0.0f || radSeg < 3 || sides < 3) {
        return meshData;
    }

    const int rings = radSeg + 1;
    const int segments = sides + 1;

    meshData.vertexCount = rings * segments;
    meshData.indexCount = radSeg * sides * 6;

    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    const float ringStep = 2.0f * PI / radSeg;
    const float sideStep = 2.0f * PI / sides;

    int vertexIndex = 0;

    for (int ring = 0; ring <= radSeg; ring++) {
        float theta = ring * ringStep;
        float cosTheta = cosf(theta);
        float sinTheta = sinf(theta);

        // Center of current ring
        Vector3 ringCenter = {
            radius * cosTheta,
            0.0f,
            -radius * sinTheta
        };

        for (int side = 0; side <= sides; side++) {
            float phi = side * sideStep;
            float cosPhi = cosf(phi);
            float sinPhi = sinf(phi);

            // Normal at vertex
            Vector3 normal = {
                cosTheta * cosPhi,
                sinPhi,
                -sinTheta * cosPhi
            };

            // Position = ringCenter + normal * size
            Vector3 pos = {
                ringCenter.x + size * normal.x,
                ringCenter.y + size * normal.y,
                ringCenter.z + size * normal.z
            };

            // Tangent along ring (around main circle)
            Vector4 tangent = {
                -sinTheta, 0.0f, -cosTheta, 1.0f
            };

            // UV coordinates
            float u = (float)ring / radSeg;
            float v = (float)side / sides;

            meshData.vertices[vertexIndex++] = (R3D_Vertex){
                .position = pos,
                .texcoord = {u, v},
                .normal = normal,
                .color = {255, 255, 255, 255},
                .tangent = tangent
            };
        }
    }

    // Indices
    int index = 0;
    for (int ring = 0; ring < radSeg; ring++) {
        for (int side = 0; side < sides; side++) {
            int current = ring * segments + side;
            int next = (ring + 1) * segments + side;

            // Triangle 1
            meshData.indices[index++] = current;
            meshData.indices[index++] = next;
            meshData.indices[index++] = next + 1;

            // Triangle 2
            meshData.indices[index++] = current;
            meshData.indices[index++] = next + 1;
            meshData.indices[index++] = current + 1;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataKnot(float radius, float size, int radSeg, int sides)
{
    R3D_MeshData meshData = { 0 };

    if (radius <= 0.0f || radius <= 0.0f || radSeg < 6 || sides < 3) {
        return meshData;
    }

    const int knotSegments = radSeg + 1;
    const int tubeSides = sides + 1;

    meshData.vertexCount = knotSegments * tubeSides;
    meshData.indexCount = radSeg * sides * 6;

    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    const float segmentStep = 2.0f * PI / radSeg;
    const float sideStep = 2.0f * PI / sides;

    int vertexIndex = 0;

    for (int seg = 0; seg <= radSeg; seg++) {
        float t = seg * segmentStep;
        
        // Trefoil knot parametric equations
        float x = sinf(t) + 2.0f * sinf(2.0f * t);
        float y = cosf(t) - 2.0f * cosf(2.0f * t);
        float z = -sinf(3.0f * t);
        
        // Scale by radius
        Vector3 knotCenter = {
            radius * x * 0.2f,  // Scale factor to normalize the knot size
            radius * y * 0.2f,
            radius * z * 0.2f
        };

        // Calculate tangent vector (derivative of knot curve)
        float dx = cosf(t) + 4.0f * cosf(2.0f * t);
        float dy = -sinf(t) + 4.0f * sinf(2.0f * t);
        float dz = -3.0f * cosf(3.0f * t);
        
        Vector3 tangent = {dx, dy, dz};
        
        // Normalize tangent
        float tangentLength = sqrtf(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (tangentLength > 0.0f) {
            tangent.x /= tangentLength;
            tangent.y /= tangentLength;
            tangent.z /= tangentLength;
        }

        // Calculate binormal (second derivative for better frame)
        float d2x = -sinf(t) - 8.0f * sinf(2.0f * t);
        float d2y = -cosf(t) + 8.0f * cosf(2.0f * t);
        float d2z = 9.0f * sinf(3.0f * t);
        
        Vector3 binormal = {d2x, d2y, d2z};
        
        // Normalize binormal
        float binormalLength = sqrtf(binormal.x * binormal.x + binormal.y * binormal.y + binormal.z * binormal.z);
        if (binormalLength > 0.0f) {
            binormal.x /= binormalLength;
            binormal.y /= binormalLength;
            binormal.z /= binormalLength;
        }

        // Calculate normal (cross product of tangent and binormal)
        Vector3 normal = {
            tangent.y * binormal.z - tangent.z * binormal.y,
            tangent.z * binormal.x - tangent.x * binormal.z,
            tangent.x * binormal.y - tangent.y * binormal.x
        };

        for (int side = 0; side <= sides; side++) {
            float phi = side * sideStep;
            float cosPhi = cosf(phi);
            float sinPhi = sinf(phi);

            // Create tube cross-section
            Vector3 tubeOffset = {
                radius * (normal.x * cosPhi + binormal.x * sinPhi),
                radius * (normal.y * cosPhi + binormal.y * sinPhi),
                radius * (normal.z * cosPhi + binormal.z * sinPhi)
            };

            // Final vertex position
            Vector3 pos = {
                knotCenter.x + tubeOffset.x,
                knotCenter.y + tubeOffset.y,
                knotCenter.z + tubeOffset.z
            };

            // Surface normal at this point
            Vector3 surfaceNormal = {
                normal.x * cosPhi + binormal.x * sinPhi,
                normal.y * cosPhi + binormal.y * sinPhi,
                normal.z * cosPhi + binormal.z * sinPhi
            };

            // Tangent along the knot curve
            Vector4 vertexTangent = {
                tangent.x, tangent.y, tangent.z, 1.0f
            };

            // UV coordinates
            float u = (float)seg / radSeg;
            float v = (float)side / sides;

            meshData.vertices[vertexIndex++] = (R3D_Vertex){
                .position = pos,
                .texcoord = {u, v},
                .normal = surfaceNormal,
                .color = {255, 255, 255, 255},
                .tangent = vertexTangent
            };
        }
    }

    // Generate indices
    int index = 0;
    for (int seg = 0; seg < radSeg; seg++) {
        for (int side = 0; side < sides; side++) {
            int current = seg * tubeSides + side;
            int next = (seg + 1) * tubeSides + side;

            // Triangle 1
            meshData.indices[index++] = current;
            meshData.indices[index++] = next;
            meshData.indices[index++] = next + 1;

            // Triangle 2
            meshData.indices[index++] = current;
            meshData.indices[index++] = next + 1;
            meshData.indices[index++] = current + 1;
        }
    }

    return meshData;
}

R3D_MeshData R3D_GenMeshDataHeightmap(Image heightmap, Vector3 size)
{
    R3D_MeshData meshData = { 0 };

    // Parameter validation
    if (heightmap.data == NULL || heightmap.width <= 1 || heightmap.height <= 1 ||
        size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
        return meshData;
    }

    // Heightmap dimensions
    const int mapWidth = heightmap.width;
    const int mapHeight = heightmap.height;

    // Mesh dimensions calculation
    meshData.vertexCount = mapWidth * mapHeight;
    meshData.indexCount = (mapWidth - 1) * (mapHeight - 1) * 6; // 2 triangles per quad

    // Memory allocation
    meshData.vertices = RL_MALLOC(meshData.vertexCount * sizeof(*meshData.vertices));
    meshData.indices = RL_MALLOC(meshData.indexCount * sizeof(*meshData.indices));

    if (!meshData.vertices || !meshData.indices) {
        if (meshData.vertices) RL_FREE(meshData.vertices);
        if (meshData.indices) RL_FREE(meshData.indices);
        return meshData;
    }

    // Precompute some values
    const float halfSizeX = size.x * 0.5f;
    const float halfSizeZ = size.z * 0.5f;
    const float stepX = size.x / (mapWidth - 1);
    const float stepZ = size.z / (mapHeight - 1);
    const float stepU = 1.0f / (mapWidth - 1);
    const float stepV = 1.0f / (mapHeight - 1);

    // Macro to extract height from a pixel
    #define GET_HEIGHT_VALUE(x, y) \
        ((x) < 0 || (x) >= mapWidth || (y) < 0 || (y) >= mapHeight) \
            ? 0.0f : ((float)GetImageColor(heightmap, x, y).r / 255)

    // Generate vertices
    int vertexIndex = 0;
    float minY = FLT_MAX, maxY = -FLT_MAX;

    for (int z = 0; z < mapHeight; z++) {
        for (int x = 0; x < mapWidth; x++) {
            // Vertex position
            float posX = -halfSizeX + x * stepX;
            float posZ = -halfSizeZ + z * stepZ;
            float posY = GET_HEIGHT_VALUE(x, z) * size.y;

            // Update Y bounds for AABB
            if (posY < minY) minY = posY;
            if (posY > maxY) maxY = posY;

            // Calculate normal by finite differences (gradient method)
            float heightL = GET_HEIGHT_VALUE(x - 1, z);     // Left
            float heightR = GET_HEIGHT_VALUE(x + 1, z);     // Right
            float heightD = GET_HEIGHT_VALUE(x, z - 1);     // Down
            float heightU = GET_HEIGHT_VALUE(x, z + 1);     // Up

            // Gradient in X and Z
            float gradX = (heightR - heightL) / (2.0f * stepX);
            float gradZ = (heightU - heightD) / (2.0f * stepZ);

            // Normal (cross product of tangent vectors)
            Vector3 normal = Vector3Normalize((Vector3) {-gradX, 1.0f, -gradZ});

            // UV mapping
            float u = x * stepU;
            float v = z * stepV;

            // Tangent (X direction in texture space)
            Vector3 tangentDir = {1.0f, gradX, 0.0f};
            float tangentLength = sqrtf(tangentDir.x * tangentDir.x + tangentDir.y * tangentDir.y + tangentDir.z * tangentDir.z);
            Vector4 tangent = {
                tangentDir.x / tangentLength,
                tangentDir.y / tangentLength,
                tangentDir.z / tangentLength,
                1.0f
            };

            meshData.vertices[vertexIndex] = (R3D_Vertex){
                .position = {posX, posY, posZ},
                .texcoord = {u, v},
                .normal = normal,
                .color = {255, 255, 255, 255},
                .tangent = tangent
            };
            vertexIndex++;
        }
    }

    // Generate indices
    int indexOffset = 0;
    for (int z = 0; z < mapHeight - 1; z++) {
        for (int x = 0; x < mapWidth - 1; x++) {
            // Indices of the 4 corners of the current quad
            uint32_t topLeft = z * mapWidth + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * mapWidth + x;
            uint32_t bottomRight = bottomLeft + 1;
        
            // First triangle (topLeft, bottomLeft, topRight)
            meshData.indices[indexOffset++] = topLeft;
            meshData.indices[indexOffset++] = bottomLeft;
            meshData.indices[indexOffset++] = topRight;
        
            // Second triangle (topRight, bottomLeft, bottomRight)
            meshData.indices[indexOffset++] = topRight;
            meshData.indices[indexOffset++] = bottomLeft;
            meshData.indices[indexOffset++] = bottomRight;
        }
    }

    // Cleanup macro
    #undef GET_HEIGHT_VALUE

    return meshData;
}

R3D_MeshData R3D_GenMeshDataCubicmap(Image cubicmap, Vector3 cubeSize)
{
    R3D_MeshData meshData = { 0 };

    // Validation of parameters
    if (cubicmap.width <= 0 || cubicmap.height <= 0 || 
        cubeSize.x <= 0.0f || cubeSize.y <= 0.0f || cubeSize.z <= 0.0f) {
        return meshData;
    }

    Color* pixels = LoadImageColors(cubicmap);
    if (!pixels) return meshData;

    // Pre-compute some values
    const float halfW = cubeSize.x * 0.5f;
    const float halfH = cubeSize.y * 0.5f;
    const float halfL = cubeSize.z * 0.5f;

    // Normals of the 6 faces of the cube
    const Vector3 normals[6] = {
        {1.0f, 0.0f, 0.0f},   // right (+X)
        {-1.0f, 0.0f, 0.0f},  // left (-X)
        {0.0f, 1.0f, 0.0f},   // up (+Y)
        {0.0f, -1.0f, 0.0f},  // down (-Y)
        {0.0f, 0.0f, -1.0f},  // forward (-Z)
        {0.0f, 0.0f, 1.0f}    // backward (+Z)
    };

    // Corresponding tangents
    const Vector4 tangents[6] = {
        {0.0f, 0.0f, -1.0f, 1.0f},  // right
        {0.0f, 0.0f, 1.0f, 1.0f},   // left
        {1.0f, 0.0f, 0.0f, 1.0f},   // up
        {1.0f, 0.0f, 0.0f, 1.0f},   // down
        {-1.0f, 0.0f, 0.0f, 1.0f},  // forward
        {1.0f, 0.0f, 0.0f, 1.0f}    // backward
    };

    // UV coordinates for the 6 faces (2x3 atlas texture)
    typedef struct { float x, y, width, height; } RectangleF;
    const RectangleF texUVs[6] = {
        {0.0f, 0.0f, 0.5f, 0.5f},    // right
        {0.5f, 0.0f, 0.5f, 0.5f},    // left
        {0.0f, 0.5f, 0.5f, 0.5f},    // up
        {0.5f, 0.5f, 0.5f, 0.5f},    // down
        {0.5f, 0.0f, 0.5f, 0.5f},    // backward
        {0.0f, 0.0f, 0.5f, 0.5f}     // forward
    };

    // Estimate the maximum number of faces needed
    int maxFaces = 0;
    for (int z = 0; z < cubicmap.height; z++) {
        for (int x = 0; x < cubicmap.width; x++) {
            Color pixel = pixels[z * cubicmap.width + x];
            if (ColorIsEqual(pixel, WHITE)) {
                maxFaces += 6; // complete cube
            }
            else if (ColorIsEqual(pixel, BLACK)) {
                maxFaces += 2; // floor and ceiling only
            }
        }
    }

    // Allocation of temporary tables
    R3D_Vertex* vertices = RL_MALLOC(maxFaces * 4 * sizeof(R3D_Vertex));
    uint32_t* indices = RL_MALLOC(maxFaces * 6 * sizeof(uint32_t));

    if (!vertices || !indices) {
        if (vertices) RL_FREE(vertices);
        if (indices) RL_FREE(indices);
        UnloadImageColors(pixels);
        return meshData;
    }

    int vertexCount = 0;
    int indexCount = 0;

    // Variables for calculating AABB
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

    // Mesh generation
    for (int z = 0; z < cubicmap.height; z++) {
        for (int x = 0; x < cubicmap.width; x++) {
            Color pixel = pixels[z * cubicmap.width + x];

            // Position of the center of the cube
            float posX = cubeSize.x * (x - cubicmap.width * 0.5f + 0.5f);
            float posZ = cubeSize.z * (z - cubicmap.height * 0.5f + 0.5f);

            // AABB Update
            minX = fminf(minX, posX - halfW);
            maxX = fmaxf(maxX, posX + halfW);
            minZ = fminf(minZ, posZ - halfL);
            maxZ = fmaxf(maxZ, posZ + halfL);

            if (ColorIsEqual(pixel, WHITE)) {
                // Complete cube - generate all necessary faces
                minY = fminf(minY, 0.0f);
                maxY = fmaxf(maxY, cubeSize.y);

                // Face up (always generated for white cubes)
                if (true) { // Top side still visible
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

                    // Clues for 2 triangles
                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 0;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Face down
                if (true) {
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
                if ((z == cubicmap.height - 1) || !ColorIsEqual(pixels[(z + 1) * cubicmap.width + x], WHITE)) {
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
                if ((z == 0) || !ColorIsEqual(pixels[(z - 1) * cubicmap.width + x], WHITE)) {
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
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 1;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Right face (+X)
                if ((x == cubicmap.width - 1) || !ColorIsEqual(pixels[z * cubicmap.width + (x + 1)], WHITE)) {
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
                if ((x == 0) || !ColorIsEqual(pixels[z * cubicmap.width + (x - 1)], WHITE)) {
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
            else if (ColorIsEqual(pixel, BLACK)) {
                // Black pixel - generate only the floor and ceiling
                minY = fminf(minY, 0.0f);
                maxY = fmaxf(maxY, cubeSize.y);

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

    // Final meshData allocation
    meshData.vertexCount = vertexCount;
    meshData.indexCount = indexCount;
    meshData.vertices = vertices;
    meshData.indices = indices;

    // Copy of final data
    memcpy(meshData.vertices, vertices, vertexCount * sizeof(R3D_Vertex));
    memcpy(meshData.indices, indices, indexCount * sizeof(uint32_t));

    // Cleaning
    UnloadImageColors(pixels);

    return meshData;
}

R3D_MeshData R3D_DuplicateMeshData(const R3D_MeshData* meshData)
{
    R3D_MeshData duplicate = {0};

    if (meshData == NULL || meshData->vertices == NULL) {
        TraceLog(LOG_ERROR, "R3D: Cannot duplicate null mesh data");
        return duplicate;
    }

    duplicate = R3D_CreateMeshData(meshData->vertexCount, meshData->indexCount);
    if (duplicate.vertices == NULL) {
        return duplicate;
    }

    memcpy(duplicate.vertices, meshData->vertices, meshData->vertexCount * sizeof(*meshData->vertices));

    if (meshData->indexCount > 0 && meshData->indices != NULL && duplicate.indices != NULL) {
        memcpy(duplicate.indices, meshData->indices, meshData->indexCount * sizeof(*meshData->indices));
    }

    return duplicate;
}

R3D_MeshData R3D_MergeMeshData(const R3D_MeshData* a, const R3D_MeshData* b)
{
    R3D_MeshData merged = {0};

    if (a == NULL || b == NULL || a->vertices == NULL || b->vertices == NULL) {
        TraceLog(LOG_ERROR, "R3D: Cannot merge null mesh data");
        return merged;
    }

    int totalVertices = a->vertexCount + b->vertexCount;
    int totalIndices = a->indexCount + b->indexCount;

    merged = R3D_CreateMeshData(totalVertices, totalIndices);

    if (merged.vertices == NULL) {
        return merged;
    }

    memcpy(merged.vertices, a->vertices, a->vertexCount * sizeof(*merged.vertices));
    memcpy(merged.vertices + a->vertexCount, b->vertices, b->vertexCount * sizeof(*merged.vertices));

    if (a->indexCount > 0 && a->indices != NULL) {
        memcpy(merged.indices, a->indices, a->indexCount * sizeof(*merged.indices));
    }

    if (b->indexCount > 0 && b->indices != NULL) {
        for (int i = 0; i < b->indexCount; i++) {
            merged.indices[a->indexCount + i] = b->indices[i] + a->vertexCount;
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
        TraceLog(LOG_ERROR, "R3D: Failed to allocate memory for tangent calculation");
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

BoundingBox R3D_CalculateMeshDataBoundingBox(const R3D_MeshData* meshData)
{
    BoundingBox bounds = {0};

    if (meshData == NULL || meshData->vertices == NULL || meshData->vertexCount == 0) {
        return bounds;
    }

    bounds.min = meshData->vertices[0].position;
    bounds.max = meshData->vertices[0].position;

    for (int i = 1; i < meshData->vertexCount; i++) {
        Vector3 pos = meshData->vertices[i].position;
        bounds.min = Vector3Min(bounds.min, pos);
        bounds.max = Vector3Max(bounds.max, pos);
    }

    return bounds;
}
