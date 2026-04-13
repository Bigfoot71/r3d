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
 * @defgroup Vertex
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a vertex and all its attributes for a mesh.
 */
typedef struct R3D_Vertex {
    Vector3 position;           ///< The 3D position of the vertex in object space.
    uint16_t texcoord[2];       ///< The 2D texture coordinates (UV) for mapping textures.
    int8_t normal[4];           ///< The normal vector used for lighting calculations.
    int8_t tangent[4];          ///< The tangent vector, used in normal mapping (often with a handedness in w).
    Color color;                ///< Vertex color, in RGBA32.
    uint8_t boneIndices[4];     ///< Indices of up to 4 bones that influence this vertex (for skinning).
    uint8_t boneWeights[4];     ///< Corresponding bone weights (should sum to 255). Defines the influence of each bone.
} R3D_Vertex;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Constructs a fully encoded @ref R3D_Vertex from unpacked attribute data.
 * @param position Vertex position in object space.
 * @param texcoord UV texture coordinates (float, any range).
 * @param normal Unit normal vector.
 * @param tangent Tangent vector with handedness in w (+1 or -1).
 * @param color Vertex color in RGBA8.
 * @return Encoded vertex ready for GPU upload.
 */
R3DAPI R3D_Vertex R3D_MakeVertex(Vector3 position, Vector2 texcoord, Vector3 normal, Vector4 tangent, Color color);

/**
 * @brief Encodes a UV coordinate pair from float32 to float16.
 * @param dst Output buffer of 2 uint16_t (float16). Must not be NULL.
 * @param src UV coordinates in float32. Supports any range (tiling included).
 */
R3DAPI void R3D_EncodeTexCoord(uint16_t* dst, Vector2 src);

/**
 * @brief Decodes a float16 UV coordinate pair back to float32.
 * @param src Input buffer of 2 uint16_t (float16). Must not be NULL.
 * @return Decoded UV coordinates in float32.
 */
R3DAPI Vector2 R3D_DecodeTexCoord(const uint16_t* src);

/**
 * @brief Encodes a unit normal vector from float32 to snorm8 (XYZ).
 * @param dst Output buffer of 4 int8_t. W is set to 0. Must not be NULL.
 * @param src Unit normal vector. Components must be in [-1, 1].
 */
R3DAPI void R3D_EncodeNormal(int8_t* dst, Vector3 src);

/**
 * @brief Decodes a snorm8 normal back to float32.
 * @param src Input buffer of 4 int8_t (only XYZ are read). Must not be NULL.
 * @return Decoded normal vector. Not guaranteed to be unit length.
 */
R3DAPI Vector3 R3D_DecodeNormal(const int8_t* src);

/**
 * @brief Encodes a tangent vector from float32 to snorm8, preserving handedness in W.
 * @param dst Output buffer of 4 int8_t. Must not be NULL.
 * @param src Tangent vector. XYZ must be in [-1, 1]; W encodes handedness (+1 or -1).
 */
R3DAPI void R3D_EncodeTangent(int8_t* dst, Vector4 src);

/**
 * @brief Decodes a snorm8 tangent back to float32.
 * @param src Input buffer of 4 int8_t. Must not be NULL.
 * @return Decoded tangent. W is exactly +1.0 or -1.0 (handedness).
 */
R3DAPI Vector4 R3D_DecodeTangent(const int8_t* src);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of MeshData

#endif // R3D_VERTEX_H
