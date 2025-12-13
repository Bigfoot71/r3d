/* r3d_mesh_data.h -- R3D Mesh Data Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MESH_DATA_H
#define R3D_MESH_DATA_H

#include "./r3d_api.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup MeshData
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a vertex and all its attributes for a mesh.
 */
typedef struct R3D_Vertex {
    Vector3 position;       /**< The 3D position of the vertex in object space. */
    Vector2 texcoord;       /**< The 2D texture coordinates (UV) for mapping textures. */
    Vector3 normal;         /**< The normal vector used for lighting calculations. */
    Vector4 color;          /**< Vertex color, typically RGBA. */
    Vector4 tangent;        /**< The tangent vector, used in normal mapping (often with a handedness in w). */
    int boneIds[4];         /**< Indices of up to 4 bones that influence this vertex (for GPU skinning). */
    float weights[4];       /**< Corresponding bone weights (should sum to 1.0). Defines the influence of each bone. */
} R3D_Vertex;

/**
 * @brief Represents a mesh stored in CPU memory.
 *
 * R3D_MeshData is the CPU-side container of a mesh. It stores vertex and index data,
 * and provides utility functions to generate, transform, and process geometry before
 * uploading it to the GPU as an R3D_Mesh.
 *
 * Think of it as a toolbox for procedural or dynamic mesh generation on the CPU.
 */
typedef struct R3D_MeshData {
    R3D_Vertex* vertices;       ///< Pointer to vertex data in CPU memory.
    uint32_t* indices;          ///< Pointer to index data in CPU memory.
    int vertexCount;            ///< Number of vertices.
    int indexCount;             ///< Number of indices.
} R3D_MeshData;

// ========================================
// PUBLIC API
// ========================================

/**
 * @brief Creates an empty mesh data container.
 * @param vertexCount Number of vertices to allocate.
 * @param indexCount Number of indices to allocate.
 * @return A new R3D_MeshData instance with allocated memory.
 */
R3DAPI R3D_MeshData R3D_CreateMeshData(int vertexCount, int indexCount);

/**
 * @brief Releases memory used by a mesh data container.
 * @param meshData Pointer to the R3D_MeshData to destroy.
 */
R3DAPI void R3D_DestroyMeshData(R3D_MeshData* meshData);

/**
 * @brief Creates a deep copy of an existing mesh data container.
 * @param meshData Source mesh data to duplicate.
 * @return A new R3D_MeshData containing a copy of the source data.
 */
R3DAPI R3D_MeshData R3D_DuplicateMeshData(const R3D_MeshData* meshData);

/**
 * @brief Merges two mesh data containers into a single one.
 * @param a First mesh data.
 * @param b Second mesh data.
 * @return A new R3D_MeshData containing the merged geometry.
 */
R3DAPI R3D_MeshData R3D_MergeMeshData(const R3D_MeshData* a, const R3D_MeshData* b);

/**
 * @brief Translates all vertices by a given offset.
 * @param meshData Mesh data to modify.
 * @param translation Offset to apply to all vertex positions.
 */
R3DAPI void R3D_TranslateMeshData(R3D_MeshData* meshData, Vector3 translation);

/**
 * @brief Rotates all vertices using a quaternion.
 * @param meshData Mesh data to modify.
 * @param rotation Quaternion representing the rotation.
 */
R3DAPI void R3D_RotateMeshData(R3D_MeshData* meshData, Quaternion rotation);

/**
 * @brief Scales all vertices by given factors.
 * @param meshData Mesh data to modify.
 * @param scale Scaling factors for each axis.
 */
R3DAPI void R3D_ScaleMeshData(R3D_MeshData* meshData, Vector3 scale);

/**
 * @brief Generates planar UV coordinates.
 * @param meshData Mesh data to modify.
 * @param uvScale Scaling factors for UV coordinates.
 * @param axis Axis along which to project the planar mapping.
 */
R3DAPI void R3D_GenMeshDataUVsPlanar(R3D_MeshData* meshData, Vector2 uvScale, Vector3 axis);

/**
 * @brief Generates spherical UV coordinates.
 * @param meshData Mesh data to modify.
 */
R3DAPI void R3D_GenMeshDataUVsSpherical(R3D_MeshData* meshData);

/**
 * @brief Generates cylindrical UV coordinates.
 * @param meshData Mesh data to modify.
 */
R3DAPI void R3D_GenMeshDataUVsCylindrical(R3D_MeshData* meshData);

/**
 * @brief Computes vertex normals from triangle geometry.
 * @param meshData Mesh data to modify.
 */
R3DAPI void R3D_GenMeshDataNormals(R3D_MeshData* meshData);

/**
 * @brief Computes vertex tangents based on existing normals and UVs.
 * @param meshData Mesh data to modify.
 */
R3DAPI void R3D_GenMeshDataTangents(R3D_MeshData* meshData);

/**
 * @brief Calculates the axis-aligned bounding box of the mesh.
 * @param meshData Mesh data to analyze.
 * @return The computed bounding box.
 */
R3DAPI BoundingBox R3D_CalculateMeshDataBoundingBox(const R3D_MeshData* meshData);

/** @} */ // end of MeshData

#endif // R3D_MESH_DATA_H
