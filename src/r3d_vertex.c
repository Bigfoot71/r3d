/* r3d_vertex.c -- R3D Vertex Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_vertex.h>
#include <r3d/r3d_pack.h>

// ========================================
// PUBLIC API
// ========================================

R3D_Vertex R3D_MakeVertex(Vector3 position, Vector2 texcoord, Vector3 normal, Vector4 tangent, Color color)
{
    R3D_Vertex v = {0};

    v.position = position;
    R3D_PackTexCoord(v.texcoord, texcoord);
    R3D_PackNormal((int8_t*)v.normal, normal);
    R3D_PackTangent((int8_t*)v.tangent, tangent);
    v.color = color;

    return v;
}

void R3D_PackTexCoord(uint16_t* dst, Vector2 src)
{
    dst[0] = R3D_PackFloat16(src.x);
    dst[1] = R3D_PackFloat16(src.y);
}

Vector2 R3D_UnpackTexCoord(const uint16_t* src)
{
    return (Vector2) {
        R3D_UnpackFloat16(src[0]),
        R3D_UnpackFloat16(src[1])
    };
}

void R3D_PackNormal(int8_t* dst, Vector3 src)
{
    dst[0] = R3D_PackSnorm8(src.x);
    dst[1] = R3D_PackSnorm8(src.y);
    dst[2] = R3D_PackSnorm8(src.z);
    dst[3] = 0;
}

Vector3 R3D_UnpackNormal(const int8_t* src)
{
    return (Vector3) {
        R3D_UnpackSnorm8(src[0]),
        R3D_UnpackSnorm8(src[1]),
        R3D_UnpackSnorm8(src[2])
    };
}

void R3D_PackTangent(int8_t* dst, Vector4 src)
{
    dst[0] = R3D_PackSnorm8(src.x);
    dst[1] = R3D_PackSnorm8(src.y);
    dst[2] = R3D_PackSnorm8(src.z);
    dst[3] = (src.w >= 0.0f) ? 127 : -127;
}

Vector4 R3D_UnpackTangent(const int8_t* src)
{
    return (Vector4) {
        R3D_UnpackSnorm8(src[0]),
        R3D_UnpackSnorm8(src[1]),
        R3D_UnpackSnorm8(src[2]),
        (src[3] >= 0) ? 1.0f : -1.0f
    };
}
