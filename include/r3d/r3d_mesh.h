/* r3d_mesh.h -- R3D Mesh Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MESH_H
#define R3D_MESH_H

#include "./r3d_mesh_data.h"
#include "./r3d_core.h"
#include "./r3d_api.h"
#include <raylib.h>

/**
 * @defgroup Mesh
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Hint on how a mesh will be used.
 */
typedef enum R3D_MeshUsage {
    R3D_STATIC_MESH,            ///< Will never be updated.
    R3D_DYNAMIC_MESH,           ///< Will be updated occasionally.
    R3D_STREAMED_MESH           ///< Will be update on each frame.
} R3D_MeshUsage;

/**
 * @brief Defines the geometric primitive type.
 */
typedef enum R3D_PrimitiveType {
    R3D_PRIMITIVE_POINTS,           ///< Each vertex represents a single point.
    R3D_PRIMITIVE_LINES,            ///< Each pair of vertices forms an independent line segment.
    R3D_PRIMITIVE_LINE_STRIP,       ///< Connected series of line segments sharing vertices.
    R3D_PRIMITIVE_LINE_LOOP,        ///< Closed loop of connected line segments.
    R3D_PRIMITIVE_TRIANGLES,        ///< Each set of three vertices forms an independent triangle.
    R3D_PRIMITIVE_TRIANGLE_STRIP,   ///< Connected strip of triangles sharing vertices.
    R3D_PRIMITIVE_TRIANGLE_FAN      ///< Fan of triangles sharing the first vertex.
} R3D_PrimitiveType;

/**
 * @brief Defines the depth mode used to render the mesh.
 */
typedef enum R3D_DepthMode {
    R3D_DEPTH_READ_WRITE,       ///< Enable depth testing and writing to the depth buffer.
    R3D_DEPTH_READ_ONLY,        ///< Enable depth testing but disable writing to the depth buffer.
    R3D_DEPTH_DISABLED          ///< Disable depth testing and writing to the depth buffer.
} R3D_DepthMode;

/**
 * @brief Shadow casting modes for objects.
 *
 * Controls how an object interacts with the shadow mapping system.
 * These modes determine whether the object contributes to shadows,
 * and if so, whether it is also rendered in the main pass.
 */
typedef enum R3D_ShadowCastMode {
    R3D_SHADOW_CAST_ON_AUTO,            ///< The object casts shadows; the faces used are determined by the material's culling mode.
    R3D_SHADOW_CAST_ON_DOUBLE_SIDED,    ///< The object casts shadows with both front and back faces, ignoring face culling.
    R3D_SHADOW_CAST_ON_FRONT_SIDE,      ///< The object casts shadows with only front faces, culling back faces.
    R3D_SHADOW_CAST_ON_BACK_SIDE,       ///< The object casts shadows with only back faces, culling front faces.
    R3D_SHADOW_CAST_ONLY_AUTO,          ///< The object only casts shadows; the faces used are determined by the material's culling mode.
    R3D_SHADOW_CAST_ONLY_DOUBLE_SIDED,  ///< The object only casts shadows with both front and back faces, ignoring face culling.
    R3D_SHADOW_CAST_ONLY_FRONT_SIDE,    ///< The object only casts shadows with only front faces, culling back faces.
    R3D_SHADOW_CAST_ONLY_BACK_SIDE,     ///< The object only casts shadows with only back faces, culling front faces.
    R3D_SHADOW_CAST_DISABLED            ///< The object does not cast shadows at all.
} R3D_ShadowCastMode;

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a 3D mesh.
 *
 * Stores vertex and index data, shadow casting settings, bounding box, and layer information.
 * Can represent a static or skinned mesh.
 */
typedef struct R3D_Mesh {
    uint32_t vao, vbo, ebo;                 ///< OpenGL objects handles.
    int vertexCount, indexCount;            ///< Number of vertices and indices currently in use.
    int allocVertexCount, allocIndexCount;  ///< Number of vertices and indices allocated in GPU buffers.
    R3D_ShadowCastMode shadowCastMode;      ///< Shadow casting mode for the mesh.
    R3D_PrimitiveType primitiveType;        ///< Type of primitive that constitutes the vertices.
    R3D_MeshUsage usage;                    ///< Hint about the usage of the mesh, retained in case of update if there is a reallocation.
    R3D_Layer layerMask;                    ///< Bitfield indicating the rendering layer(s) of this mesh.
    BoundingBox aabb;                       ///< Axis-Aligned Bounding Box in local space.
} R3D_Mesh;

// ========================================
// PUBLIC API
// ========================================

/**
 * @brief Creates a 3D mesh from CPU-side mesh data.
 * @param type Primitive type used to interpret vertex data.
 * @param data Pointer to the R3D_MeshData containing vertices and indices (cannot be NULL).
 * @param aabb Optional pointer to a bounding box. If NULL, it will be computed automatically.
 * @param usage Hint on how the mesh will be used.
 * @return Created R3D_Mesh.
 * @note The function copies all vertex and index data into GPU buffers.
 */
R3DAPI R3D_Mesh R3D_LoadMesh(R3D_PrimitiveType type, const R3D_MeshData* data, const BoundingBox* aabb, R3D_MeshUsage usage);

/**
 * @brief Destroys a 3D mesh and frees its resources.
 * @param mesh Pointer to the R3D_Mesh to destroy.
 */
R3DAPI void R3D_UnloadMesh(R3D_Mesh* mesh);

/**
 * @brief Upload a mesh data on the GPU.
 *
 * This function uploads a mesh's vertex and optional index data to the GPU.
 *
 * If `aabb` is provided, it will be used as the mesh's bounding box; if null,
 * the bounding box is automatically recalculated from the vertex data.
 *
 * @param mesh Pointer to the mesh structure to upload or update.
 * @param data Pointer to the mesh data (vertices and indices) to upload.
 * @param aabb Optional bounding box; if null, it is recalculated automatically.
 * @return Returns true if the update is successful, false otherwise.
 */
R3DAPI bool R3D_UpdateMesh(R3D_Mesh* mesh, const R3D_MeshData* data, const BoundingBox* aabb);

/** @} */ // end of Mesh

#endif // R3D_MESH_H
