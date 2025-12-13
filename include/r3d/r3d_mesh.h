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
    unsigned int vao, vbo, ebo;          ///< OpenGL objects handles.
    R3D_ShadowCastMode shadowCastMode;   ///< Shadow casting mode for the mesh.
    R3D_PrimitiveType primitiveType;     ///< Type of primitive that constitutes the vertices.
    R3D_Layer layerMask;                 ///< Bitfield indicating the rendering layer(s) of this mesh.
    BoundingBox aabb;                    ///< Axis-Aligned Bounding Box in local space.
} R3D_Mesh;

// ========================================
// PUBLIC API
// ========================================

/**
 * @brief Creates a 3D mesh from CPU-side mesh data.
 * @param type Primitive type used to interpret vertex data.
 * @param meshData Pointer to the R3D_MeshData containing vertices and indices (cannot be NULL).
 * @param aabb Optional pointer to a bounding box. If NULL, it will be computed automatically.
 * @return Pointer to a newly created R3D_Mesh.
 * @note The function copies all vertex and index data into GPU buffers.
 */
R3DAPI R3D_Mesh* R3D_CreateMesh(R3D_PrimitiveType type, const R3D_MeshData* meshData, const BoundingBox* aabb);

/**
 * @brief Destroys a 3D mesh and frees its resources.
 * @param mesh Pointer to the R3D_Mesh to destroy.
 */
R3DAPI void R3D_DestroyMesh(R3D_Mesh* mesh);

/**
 * @brief Generate a polygon mesh with specified number of sides.
 *
 * Creates a regular polygon mesh centered at the origin in the XY plane.
 * The polygon is generated with vertices evenly distributed around a circle.
 *
 * @param sides Number of sides for the polygon (minimum 3).
 * @param radius Radius of the circumscribed circle.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated polygon mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshPoly(int sides, float radius, bool upload);

/**
 * @brief Generate a plane mesh with specified dimensions and resolution.
 *
 * Creates a flat plane mesh in the XZ plane, centered at the origin.
 * The mesh can be subdivided for higher resolution or displacement mapping.
 *
 * @param width Width of the plane along the X axis.
 * @param length Length of the plane along the Z axis.
 * @param resX Number of subdivisions along the X axis.
 * @param resZ Number of subdivisions along the Z axis.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated plane mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshPlane(float width, float length, int resX, int resZ, bool upload);

/**
 * @brief Generate a cube mesh with specified dimensions.
 *
 * Creates a cube mesh centered at the origin with the specified width, height, and length.
 * Each face consists of two triangles with proper normals and texture coordinates.
 *
 * @param width Width of the cube along the X axis.
 * @param height Height of the cube along the Y axis.
 * @param length Length of the cube along the Z axis.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cube mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCube(float width, float height, float length, bool upload);

/**
 * @brief Generate a sphere mesh with specified parameters.
 *
 * Creates a UV sphere mesh centered at the origin using latitude-longitude subdivision.
 * Higher ring and slice counts produce smoother spheres but with more vertices.
 *
 * @param radius Radius of the sphere.
 * @param rings Number of horizontal rings (latitude divisions).
 * @param slices Number of vertical slices (longitude divisions).
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated sphere mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshSphere(float radius, int rings, int slices, bool upload);

/**
 * @brief Generate a hemisphere mesh with specified parameters.
 *
 * Creates a half-sphere mesh (dome) centered at the origin, extending upward in the Y axis.
 * Uses the same UV sphere generation technique as R3D_GenMeshSphere but only the upper half.
 *
 * @param radius Radius of the hemisphere.
 * @param rings Number of horizontal rings (latitude divisions).
 * @param slices Number of vertical slices (longitude divisions).
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated hemisphere mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshHemiSphere(float radius, int rings, int slices, bool upload);

/**
 * @brief Generate a cylinder mesh with specified parameters.
 *
 * Creates a cylinder mesh centered at the origin, extending along the Y axis.
 * The cylinder includes both top and bottom caps and smooth side surfaces.
 *
 * @param radius Radius of the cylinder base.
 * @param height Height of the cylinder along the Y axis.
 * @param slices Number of radial subdivisions around the cylinder.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cylinder mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCylinder(float radius, float height, int slices, bool upload);

/**
 * @brief Generate a cone mesh with specified parameters.
 *
 * Creates a cone mesh with its base centered at the origin and apex pointing upward along the Y axis.
 * The cone includes a circular base and smooth tapered sides.
 *
 * @param radius Radius of the cone base.
 * @param height Height of the cone along the Y axis.
 * @param slices Number of radial subdivisions around the cone base.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cone mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCone(float radius, float height, int slices, bool upload);

/**
 * @brief Generate a torus mesh with specified parameters.
 *
 * Creates a torus (donut shape) mesh centered at the origin in the XZ plane.
 * The torus is defined by a major radius (distance from center to tube center)
 * and a minor radius (tube thickness).
 *
 * @param radius Major radius of the torus (distance from center to tube center).
 * @param size Minor radius of the torus (tube thickness/radius).
 * @param radSeg Number of segments around the major radius.
 * @param sides Number of sides around the tube cross-section.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated torus mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshTorus(float radius, float size, int radSeg, int sides, bool upload);

/**
 * @brief Generate a trefoil knot mesh with specified parameters.
 *
 * Creates a trefoil knot mesh, which is a mathematical knot shape.
 * Similar to a torus but with a twisted, knotted topology.
 *
 * @param radius Major radius of the knot.
 * @param size Minor radius (tube thickness) of the knot.
 * @param radSeg Number of segments around the major radius.
 * @param sides Number of sides around the tube cross-section.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated trefoil knot mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshKnot(float radius, float size, int radSeg, int sides, bool upload);

/**
 * @brief Generate a terrain mesh from a heightmap image.
 *
 * Creates a terrain mesh by interpreting the brightness values of a heightmap image
 * as height values. The resulting mesh represents a 3D terrain surface.
 *
 * @param heightmap Image containing height data (grayscale values represent elevation).
 * @param size 3D vector defining the terrain dimensions (width, max height, depth).
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated heightmap terrain mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshHeightmap(Image heightmap, Vector3 size, bool upload);

/**
 * @brief Generate a voxel-style mesh from a cubicmap image.
 *
 * Creates a mesh composed of cubes based on a cubicmap image, where each pixel
 * represents the presence or absence of a cube at that position. Useful for
 * creating voxel-based or block-based geometry.
 *
 * @param cubicmap Image where pixel values determine cube placement.
 * @param cubeSize 3D vector defining the size of each individual cube.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cubicmap mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCubicmap(Image cubicmap, Vector3 cubeSize, bool upload);

/**
 * @brief Free mesh data from both RAM and VRAM.
 *
 * Releases all memory associated with a mesh, including vertex data in RAM
 * and GPU buffers (VAO, VBO, EBO) if the mesh was uploaded to VRAM.
 * After calling this function, the mesh should not be used.
 *
 * @param mesh Pointer to the mesh structure to be freed.
 */
R3DAPI void R3D_UnloadMesh(const R3D_Mesh* mesh);

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
 */
R3DAPI void R3D_UpdateMesh(R3D_Mesh* mesh, const R3D_MeshData* data, const BoundingBox* aabb);

/** @} */ // end of Mesh

#endif // R3D_MESH_H
