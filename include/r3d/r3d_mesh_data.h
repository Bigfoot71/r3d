/* r3d_mesh_data.h -- R3D Mesh Data Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MESH_DATA_H
#define R3D_MESH_DATA_H

#include "./r3d_platform.h"
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
    Color color;            /**< Vertex color, in RGBA32. */
    Vector4 tangent;        /**< The tangent vector, used in normal mapping (often with a handedness in w). */
    int boneIds[4];         /**< Indices of up to 4 bones that influence this vertex (for skinning). */
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates an empty mesh data container.
 *
 * Allocates memory for vertex and index buffers. All allocated buffers
 * are zero-initialized.
 *
 * @param vertexCount Number of vertices to allocate. Must be non-zero.
 * @param indexCount Number of indices to allocate. May be zero.
 *                   If zero, no index buffer is allocated.
 *
 * @return A new R3D_MeshData instance with allocated memory.
 */
R3DAPI R3D_MeshData R3D_CreateMeshData(int vertexCount, int indexCount);

/**
 * @brief Releases memory used by a mesh data container.
 * @param meshData R3D_MeshData to destroy.
 */
R3DAPI void R3D_UnloadMeshData(R3D_MeshData meshData);

/**
 * @brief Check if mesh data is valid.
 * 
 * Returns true if the mesh data contains at least one vertex buffer
 * with a positive number of vertices.
 *
 * @param meshData Mesh data to check.
 * @return true if valid, false otherwise.
 */
R3DAPI bool R3D_IsMeshDataValid(R3D_MeshData meshData);

/**
 * @brief Generate a quad mesh with specified dimensions, resolution, and orientation.
 *
 * Creates a flat rectangular quad mesh with customizable facing direction.
 * The mesh can be subdivided for higher resolution or displacement mapping.
 * The quad is centered at the origin and oriented according to the frontDir parameter,
 * which defines both the face direction and the surface normal.
 *
 * @param width Width of the quad along its local X axis.
 * @param length Length of the quad along its local Z axis.
 * @param resX Number of subdivisions along the width.
 * @param resZ Number of subdivisions along the length.
 * @param frontDir Direction vector defining the quad's front face and normal.
 *                 This vector will be normalized internally.
 *
 * @return Generated quad mesh structure with proper normals, tangents, and UVs.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataQuad(float width, float length, int resX, int resZ, Vector3 frontDir);

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
 *
 * @return Generated plane mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataPlane(float width, float length, int resX, int resZ);

/**
 * @brief Generate a polygon mesh with specified number of sides.
 *
 * Creates a regular polygon mesh centered at the origin in the XY plane.
 * The polygon is generated with vertices evenly distributed around a circle.
 *
 * @param sides Number of sides for the polygon (minimum 3).
 * @param radius Radius of the circumscribed circle.
 * @param frontDir Direction vector defining the polygon's front face and normal.
 *                 This vector will be normalized internally.
 *
 * @return Generated polygon mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataPoly(int sides, float radius, Vector3 frontDir);

/**
 * @brief Generate a cube mesh with specified dimensions.
 *
 * Creates a cube mesh centered at the origin with the specified width, height, and length.
 * Each face consists of two triangles with proper normals and texture coordinates.
 *
 * @param width Width of the cube along the X axis.
 * @param height Height of the cube along the Y axis.
 * @param length Length of the cube along the Z axis.
 *
 * @return Generated cube mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataCube(float width, float height, float length);

/**
 * @brief Generate a subdivided cube mesh with specified dimensions.
 *
 * Extension of R3D_GenMeshDataCube() allowing per-axis subdivision.
 * Each face can be tessellated along the X, Y, and Z axes according
 * to the provided resolutions.
 *
 * @param width Width of the cube along the X axis.
 * @param height Height of the cube along the Y axis.
 * @param length Length of the cube along the Z axis.
 * @param resX Number of subdivisions along the X axis.
 * @param resY Number of subdivisions along the Y axis.
 * @param resZ Number of subdivisions along the Z axis.
 *
 * @return Generated cube mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataCubeEx(float width, float height, float length, int resX, int resY, int resZ);

/**
 * @brief Generate a slope mesh by cutting a cube with a plane.
 *
 * Creates a slope mesh by slicing a cube with a plane that passes through the origin.
 * The plane is defined by its normal vector, and the portion of the cube on the side
 * opposite to the normal direction is kept. This allows creating ramps, wedges, and
 * angled surfaces with arbitrary orientations.
 *
 * @param width Width of the base cube along the X axis.
 * @param height Height of the base cube along the Y axis.
 * @param length Length of the base cube along the Z axis.
 * @param slopeNormal Normal vector of the cutting plane. The mesh keeps the portion
 *                    of the cube in the direction opposite to this normal.
 *                    Example: {-1, 0, 0} creates a ramp rising towards +X.
 *                             {0, 1, 0} creates a wedge with the slope facing up.
 *                             {-1.0, 1.0, 0} creates a 45° diagonal slope.
 *
 * @return Generated slope mesh structure.
 *
 * @note The normal vector will be automatically normalized internally.
 * @note The cutting plane always passes through the center of the cube (origin).
 */
R3DAPI R3D_MeshData R3D_GenMeshDataSlope(float width, float height, float length, Vector3 slopeNormal);

/**
 * @brief Generate a sphere mesh with specified parameters.
 *
 * Creates a UV sphere mesh centered at the origin using latitude-longitude subdivision.
 * Higher ring and slice counts produce smoother spheres but with more vertices.
 *
 * @param radius Radius of the sphere.
 * @param rings Number of horizontal rings (latitude divisions).
 * @param slices Number of vertical slices (longitude divisions).
 *
 * @return Generated sphere mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataSphere(float radius, int rings, int slices);

/**
 * @brief Generate a hemisphere mesh with specified parameters.
 *
 * Creates a half-sphere mesh (dome) centered at the origin, extending upward in the Y axis.
 * Uses the same UV sphere generation technique as R3D_GenMeshSphere but only the upper half.
 *
 * @param radius Radius of the hemisphere.
 * @param rings Number of horizontal rings (latitude divisions).
 * @param slices Number of vertical slices (longitude divisions).
 *
 * @return Generated hemisphere mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataHemiSphere(float radius, int rings, int slices);

/**
 * @brief Generate a cylinder mesh with specified parameters.
 *
 * Creates a mesh centered at the origin, extending along the Y axis.
 * The mesh includes top and bottom caps and smooth side surfaces.
 * A cone is produced when bottomRadius and topRadius differ.
 *
 * @param bottomRadius Radius of the bottom cap.
 * @param topRadius Radius of the top cap.
 * @param height Height of the shape along the Y axis.
 * @param slices Number of radial subdivisions around the shape.
 *
 * @return Generated mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataCylinder(float bottomRadius, float topRadius, float height, int slices);

/**
 * @brief Generate a capsule mesh with specified parameters.
 *
 * Creates a capsule mesh centered at the origin, extending along the Y axis.
 * The capsule consists of a cylindrical body with hemispherical caps on both ends.
 * The total height of the capsule is height + 2 * radius.
 *
 * @param radius Radius of the capsule (both cylindrical body and hemispherical caps).
 * @param height Height of the cylindrical portion along the Y axis.
 * @param rings Total number of latitudinal subdivisions for both hemispheres combined.
 * @param slices Number of radial subdivisions around the shape.
 *
 * @return Generated mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataCapsule(float radius, float height, int rings, int slices);

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
 *
 * @return Generated torus mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataTorus(float radius, float size, int radSeg, int sides);

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
 *
 * @return Generated trefoil knot mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataKnot(float radius, float size, int radSeg, int sides);

/**
 * @brief Generate a terrain mesh from a heightmap image.
 *
 * Creates a terrain mesh by interpreting the brightness values of a heightmap image
 * as height values. The resulting mesh represents a 3D terrain surface.
 *
 * @param heightmap Image containing height data (grayscale values represent elevation).
 * @param size 3D vector defining the terrain dimensions (width, max height, depth).
 *
 * @return Generated heightmap terrain mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataHeightmap(Image heightmap, Vector3 size);

/**
 * @brief Generate a voxel-style mesh from a cubicmap image.
 *
 * Creates a mesh composed of cubes based on a cubicmap image, where each pixel
 * represents the presence or absence of a cube at that position. Useful for
 * creating voxel-based or block-based geometry.
 *
 * @param cubicmap Image where pixel values determine cube placement.
 * @param cubeSize 3D vector defining the size of each individual cube.
 *
 * @return Generated cubicmap mesh structure.
 */
R3DAPI R3D_MeshData R3D_GenMeshDataCubicmap(Image cubicmap, Vector3 cubeSize);

/**
 * @brief Creates a deep copy of an existing mesh data container.
 * @param meshData Source mesh data to duplicate.
 * @return A new R3D_MeshData containing a copy of the source data.
 */
R3DAPI R3D_MeshData R3D_DuplicateMeshData(R3D_MeshData meshData);

/**
 * @brief Merges two mesh data containers into a single one.
 * @param a First mesh data.
 * @param b Second mesh data.
 * @return A new R3D_MeshData containing the merged geometry.
 */
R3DAPI R3D_MeshData R3D_MergeMeshData(R3D_MeshData a, R3D_MeshData b);

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
R3DAPI BoundingBox R3D_CalculateMeshDataBoundingBox(R3D_MeshData meshData);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of MeshData

#endif // R3D_MESH_DATA_H
