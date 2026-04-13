/* r3d_vertex.c -- R3D Vertex Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_vertex.h>
#include <math.h>

#include "./common/r3d_half.h"

// ========================================
// PUBLIC API
// ========================================

R3D_Vertex R3D_MakeVertex(Vector3 position, Vector2 texcoord, Vector3 normal, Vector4 tangent, Color color)
{
    R3D_Vertex v = {0};
    v.position = position;
    R3D_EncodeTexCoord(v.texcoord, texcoord);
    R3D_EncodeNormal((int8_t*)v.normal, normal);
    R3D_EncodeTangent((int8_t*)v.tangent, tangent);
    v.color = color;
    return v;
}

void R3D_EncodeTexCoord(uint16_t* dst, Vector2 src)
{
    dst[0] = r3d_half_from_float(src.x);
    dst[1] = r3d_half_from_float(src.y);
}

Vector2 R3D_DecodeTexCoord(const uint16_t* src)
{
    Vector2 result;
    result.x = r3d_half_from_float(src[0]);
    result.y = r3d_half_from_float(src[1]);
    return result;
}

void R3D_EncodeNormal(int8_t* dst, Vector3 src)
{
    dst[0] = (int8_t)roundf(src.x * 127.0f);
    dst[1] = (int8_t)roundf(src.y * 127.0f);
    dst[2] = (int8_t)roundf(src.z * 127.0f);
    dst[3] = 0;
}

Vector3 R3D_DecodeNormal(const int8_t* src)
{
    return (Vector3){
        src[0] / 127.0f,
        src[1] / 127.0f,
        src[2] / 127.0f
    };
}

void R3D_EncodeTangent(int8_t* dst, Vector4 src)
{
    dst[0] = (int8_t)roundf(src.x * 127.0f);
    dst[1] = (int8_t)roundf(src.y * 127.0f);
    dst[2] = (int8_t)roundf(src.z * 127.0f);
    dst[3] = (src.w >= 0.0f) ? 127 : -127;
}

Vector4 R3D_DecodeTangent(const int8_t* src)
{
    return (Vector4){
        src[0] / 127.0f,
        src[1] / 127.0f,
        src[2] / 127.0f,
        (src[3] >= 0) ? 1.0f : -1.0f
    };
}
