/* r3d_vertex.h -- R3D Vertex Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_VERTEX_H
#define R3D_VERTEX_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Vertex Vertex
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Compact vertex format used by R3D meshes.
 *
 * Texture coordinates are stored as float16 values.
 * Normals and tangents are stored as signed normalized 8-bit values.
 * Bone weights are stored as unsigned 8-bit values and should sum to 255.
 */
typedef struct R3D_Vertex {
    Vector3 position;           ///< Vertex position in object space.
    uint16_t texcoord[2];       ///< Texture coordinates stored as float16.
    int8_t normal[4];           ///< Normal vector stored as SNORM8. XYZ are used, W is unused.
    int8_t tangent[4];          ///< Tangent vector stored as SNORM8. XYZ are tangent, W stores handedness.
    Color color;                ///< Vertex color in RGBA8.
    uint8_t boneIndices[4];     ///< Indices of up to 4 bones influencing this vertex.
    uint8_t boneWeights[4];     ///< Bone weights in UNORM8. Values should sum to 255.
} R3D_Vertex;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Constructs a packed R3D vertex from unpacked attribute data.
 *
 * Texture coordinates are packed to float16.
 * Normals and tangents are packed to SNORM8.
 *
 * @param position Vertex position in object space.
 * @param texcoord Texture coordinates in float32. Any range is supported.
 * @param normal Normal vector. Components are clamped to the [-1, 1] range.
 * @param tangent Tangent vector. XYZ components are clamped to [-1, 1], W stores handedness.
 * @param color Vertex color in RGBA8.
 *
 * @return Packed vertex ready for GPU upload.
 */
R3DAPI R3D_Vertex R3D_MakeVertex(Vector3 position, Vector2 texcoord, Vector3 normal, Vector4 tangent, Color color);

/**
 * @brief Packs texture coordinates from float32 to float16.
 *
 * @param dst Output buffer of 2 uint16_t values. Must not be NULL.
 * @param src Texture coordinates in float32. Any range is supported.
 */
R3DAPI void R3D_PackTexCoord(uint16_t* dst, Vector2 src);

/**
 * @brief Unpacks texture coordinates from float16 to float32.
 *
 * @param src Input buffer of 2 uint16_t values. Must not be NULL.
 *
 * @return Unpacked texture coordinates in float32.
 */
R3DAPI Vector2 R3D_UnpackTexCoord(const uint16_t* src);

/**
 * @brief Packs a normal vector from float32 to SNORM8.
 *
 * XYZ components are clamped to the [-1, 1] range before packing.
 * The fourth component is set to 0.
 *
 * @param dst Output buffer of 4 int8_t values. Must not be NULL.
 * @param src Normal vector to pack.
 */
R3DAPI void R3D_PackNormal(int8_t* dst, Vector3 src);

/**
 * @brief Unpacks a normal vector from SNORM8 to float32.
 *
 * Only XYZ components are read.
 *
 * @param src Input buffer of 4 int8_t values. Must not be NULL.
 *
 * @return Unpacked normal vector. Not guaranteed to be unit length.
 */
R3DAPI Vector3 R3D_UnpackNormal(const int8_t* src);

/**
 * @brief Packs a tangent vector from float32 to SNORM8.
 *
 * XYZ components are clamped to the [-1, 1] range before packing.
 * W stores tangent handedness and is packed as either +1 or -1.
 *
 * @param dst Output buffer of 4 int8_t values. Must not be NULL.
 * @param src Tangent vector to pack. W is interpreted as handedness.
 */
R3DAPI void R3D_PackTangent(int8_t* dst, Vector4 src);

/**
 * @brief Unpacks a tangent vector from SNORM8 to float32.
 *
 * XYZ components are unpacked from SNORM8.
 * W is returned as exactly +1.0f or -1.0f.
 *
 * @param src Input buffer of 4 int8_t values. Must not be NULL.
 *
 * @return Unpacked tangent vector.
 */
R3DAPI Vector4 R3D_UnpackTangent(const int8_t* src);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Vertex

#endif // R3D_VERTEX_H
