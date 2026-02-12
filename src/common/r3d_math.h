/* r3d_math.h -- Common R3D Math
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_MATH_H
#define R3D_COMMON_MATH_H

#include <r3d/r3d_core.h>
#include <raymath.h>
#include <string.h>
#include <math.h>

#include "./r3d_helper.h"

// ========================================
// DEFINITIONS AND CONSTANTS
// ========================================

#ifndef R3D_RESTRICT
#   if defined(_MSC_VER)
#       define R3D_RESTRICT __restrict
#   else
#       define R3D_RESTRICT restrict
#   endif
#endif

#define R3D_SRGB_ALPHA                  (0.055f)
#define R3D_SRGB_INV_ALPHA              (1.0f / 1.055f)
#define R3D_SRGB_GAMMA                  (2.4f)
#define R3D_SRGB_INV_GAMMA              (1.0f / 2.4f)
#define R3D_SRGB_LINEAR_THRESHOLD       (0.04045f)
#define R3D_SRGB_NONLINEAR_THRESHOLD    (0.0031308f)
#define R3D_SRGB_LINEAR_FACTOR          (1.0f / 12.92f)

#define R3D_MATRIX_IDENTITY             \
    (Matrix) {                          \
        1.0f, 0.0f, 0.0f, 0.0f,         \
        0.0f, 1.0f, 0.0f, 0.0f,         \
        0.0f, 0.0f, 1.0f, 0.0f,         \
        0.0f, 0.0f, 0.0f, 1.0f,         \
    }

#define R3D_AABB_UNIT                   \
    (BoundingBox) {                     \
        .min = {-0.5f, -0.5f, -0.5f},   \
        .max = {+0.5f, +0.5f, +0.5f},   \
    }

// ========================================
// HELPER TYPES
// ========================================

typedef struct {
    int x, y;
    int w, h;
} r3d_rect_t;

typedef struct {
    Vector3 center;
    Vector3 axisX;
    Vector3 axisY;
    Vector3 axisZ;
    Vector3 halfExtents;
} r3d_oriented_box_t;

// ========================================
// COLOR FUNCTIONS
// ========================================

static inline Vector3 r3d_color_to_vec3(Color color)
{
    return (Vector3) {
        color.r * (1.0f / 255.0f),
        color.g * (1.0f / 255.0f),
        color.b * (1.0f / 255.0f)
    };
}

static inline Vector4 r3d_color_to_vec4(Color color)
{
    return (Vector4) {
        color.r * (1.0f / 255.0f),
        color.g * (1.0f / 255.0f),
        color.b * (1.0f / 255.0f),
        color.a * (1.0f / 255.0f)
    };
}

static inline float r3d_srgb8_to_linear(uint8_t srgb8)
{
    float srgb = srgb8 * (1.0f / 255.0f);
    
    return (srgb <= R3D_SRGB_LINEAR_THRESHOLD) 
        ? srgb * R3D_SRGB_LINEAR_FACTOR
        : powf((srgb + R3D_SRGB_ALPHA) * R3D_SRGB_INV_ALPHA, R3D_SRGB_GAMMA);
}

static inline uint8_t r3d_linear_to_srgb8(float linear)
{
    float srgb = (linear <= R3D_SRGB_NONLINEAR_THRESHOLD)
        ? 12.92f * linear
        : (1.0f + R3D_SRGB_ALPHA) * powf(linear, R3D_SRGB_INV_GAMMA) - R3D_SRGB_ALPHA;
    
    return (uint8_t)(SATURATE(srgb) * 255.0f + 0.5f);
}

static inline Vector3 r3d_color_srgb_to_linear_vec3(Color color)
{
    return (Vector3) {
        r3d_srgb8_to_linear(color.r),
        r3d_srgb8_to_linear(color.g),
        r3d_srgb8_to_linear(color.b)
    };
}

static inline Vector4 r3d_color_srgb_to_linear_vec4(Color color)
{
    return (Vector4) {
        r3d_srgb8_to_linear(color.r),
        r3d_srgb8_to_linear(color.g),
        r3d_srgb8_to_linear(color.b),
        color.a * (1.0f / 255.0f)
    };
}

static inline Color r3d_color_linear_to_srgb_vec3(Vector3 linear)
{
    return (Color) {
        r3d_linear_to_srgb8(linear.x),
        r3d_linear_to_srgb8(linear.y),
        r3d_linear_to_srgb8(linear.z),
        255
    };
}

static inline Color r3d_color_linear_to_srgb_vec4(Vector4 linear)
{
    return (Color) {
        r3d_linear_to_srgb8(linear.x),
        r3d_linear_to_srgb8(linear.y),
        r3d_linear_to_srgb8(linear.z),
        (uint8_t)(SATURATE(linear.w) * 255.0f + 0.5f)
    };
}

static inline Vector3 r3d_color_to_linear_vec3(Color color, R3D_ColorSpace space)
{
    switch (space) {
    case R3D_COLORSPACE_SRGB: return r3d_color_srgb_to_linear_vec3(color);
    default: break;
    }

    return r3d_color_to_vec3(color);
}

static inline Vector4 r3d_color_to_linear_vec4(Color color, R3D_ColorSpace space)
{
    switch (space) {
    case R3D_COLORSPACE_SRGB: return r3d_color_srgb_to_linear_vec4(color);
    default: break;
    }

    return r3d_color_to_vec4(color);
}

static inline Vector3 r3d_color_to_linear_scaled_vec3(Color color, R3D_ColorSpace space, float scale)
{
    Vector3 result = r3d_color_to_linear_vec3(color, space);
    result.x *= scale;
    result.y *= scale;
    result.z *= scale;
    return result;
}

static inline Vector4 r3d_color_to_linear_scaled_vec4(Color color, R3D_ColorSpace space, float scale)
{
    Vector4 result = r3d_color_to_linear_vec4(color, space);
    result.x *= scale;
    result.y *= scale;
    result.z *= scale;
    return result;
}

// ========================================
// VECTOR FUNCTIONS
// ========================================

static inline Vector3 r3d_vector3_transform(Vector3 v, const Matrix* m)
{
    float x = v.x, y = v.y, z = v.z;
    return (Vector3){
        m->m0 * x + m->m4 * y + m->m8  * z + m->m12,
        m->m1 * x + m->m5 * y + m->m9  * z + m->m13,
        m->m2 * x + m->m6 * y + m->m10 * z + m->m14
    };
}

static inline Vector3 r3d_vector3_transform_normal(Vector3 v, const Matrix* m)
{
    float x = v.x, y = v.y, z = v.z;
    return (Vector3){
        m->m0 * x + m->m4 * y + m->m8  * z,
        m->m1 * x + m->m5 * y + m->m9  * z,
        m->m2 * x + m->m6 * y + m->m10 * z
    };
}

static inline Vector3 r3d_vector3_transform_linear(Vector3 v, const Matrix* m)
{
    float x = v.x, y = v.y, z = v.z;
    return (Vector3){
        m->m0 * x + m->m4 * y + m->m8 * z,
        m->m1 * x + m->m5 * y + m->m9 * z,
        m->m2 * x + m->m6 * y + m->m10 * z
    };
}

static inline Vector4 r3d_vector4_transform(Vector4 v, const Matrix* m)
{
    float x = v.x, y = v.y, z = v.z, w = v.w;
    return (Vector4){
        m->m0 * x + m->m4 * y + m->m8 * z + m->m12 * w,
        m->m1 * x + m->m5 * y + m->m9 * z + m->m13 * w,
        m->m2 * x + m->m6 * y + m->m10 * z + m->m14 * w,
        m->m3 * x + m->m7 * y + m->m11 * z + m->m15 * w
    };
}

// ========================================
// MATRIX FUNCTIONS
// ========================================

static inline bool r3d_matrix_is_identity(Matrix matrix)
{
    return (0 == memcmp(&matrix, &R3D_MATRIX_IDENTITY, sizeof(Matrix)));
}

static inline Matrix r3d_matrix_st(Vector3 scale, Vector3 translate)
{
    return (Matrix) {
        scale.x, 0.0f,    0.0f,    translate.x,
        0.0f,    scale.y, 0.0f,    translate.y,
        0.0f,    0.0f,    scale.z, translate.z,
        0.0f,    0.0f,    0.0f,    1.0f
    };
}

static inline Matrix r3d_matrix_srt_axis(Vector3 scale, Vector4 axis, Vector3 translate)
{
    float axisLen = sqrtf(axis.x*axis.x + axis.y*axis.y + axis.z*axis.z);
    if (axisLen < 1e-6f) {
        return r3d_matrix_st(scale, translate);
    }

    float invLen = 1.0f / axisLen;
    float x = axis.x * invLen;
    float y = axis.y * invLen; 
    float z = axis.z * invLen;
    float angle = axis.w;

    float c = cosf(angle);
    float s = sinf(angle);
    float oneMinusC = 1.0f - c;

    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float xs = x * s, ys = y * s, zs = z * s;

    return (Matrix) {
        scale.x * (c + xx * oneMinusC),  scale.x * (xy * oneMinusC - zs), scale.x * (xz * oneMinusC + ys), translate.x,
        scale.y * (xy * oneMinusC + zs), scale.y * (c + yy * oneMinusC),  scale.y * (yz * oneMinusC - xs), translate.y,
        scale.z * (xz * oneMinusC - ys), scale.z * (yz * oneMinusC + xs), scale.z * (c + zz * oneMinusC),  translate.z,
        0,0,0,1
    };
}

static inline Matrix r3d_matrix_srt_euler(Vector3 scale, Vector3 euler, Vector3 translate)
{
    float cx = cosf(euler.x), sx = sinf(euler.x);
    float cy = cosf(euler.y), sy = sinf(euler.y); 
    float cz = cosf(euler.z), sz = sinf(euler.z);

    float czcx = cz * cx, czsx = cz * sx;
    float szcx = sz * cx, szsx = sz * sx;

    return (Matrix) {
        scale.x * (cy*cz),               scale.x * (-cy*sz),             scale.x * sy,      translate.x,
        scale.y * (sx*sy*cz + cx*sz),    scale.y * (-sx*sy*sz + cx*cz),  scale.y * (-sx*cy), translate.y,
        scale.z * (-cx*sy*cz + sx*sz),   scale.z * (cx*sy*sz + sx*cz),   scale.z * (cx*cy),  translate.z,
        0,0,0,1
    };
}

static inline Matrix r3d_matrix_srt_quat(Vector3 scale, Quaternion quat, Vector3 translate)
{
    float qlen = sqrtf(quat.x*quat.x + quat.y*quat.y + quat.z*quat.z + quat.w*quat.w);
    if (qlen < 1e-6f) {
        return r3d_matrix_st(scale, translate);
    }

    float invLen = 1.0f / qlen;
    float qx = quat.x * invLen;
    float qy = quat.y * invLen;
    float qz = quat.z * invLen;
    float qw = quat.w * invLen;

    float qx2  = qx * qx, qy2  = qy * qy, qz2  = qz * qz;
    float qxqy = qx * qy, qxqz = qx * qz, qxqw = qx * qw;
    float qyqz = qy * qz, qyqw = qy * qw, qzqw = qz * qw;

    float r00 = 1.0f - 2.0f * (qy2 + qz2);
    float r01 = 2.0f * (qxqy - qzqw);
    float r02 = 2.0f * (qxqz + qyqw);
    
    float r10 = 2.0f * (qxqy + qzqw);
    float r11 = 1.0f - 2.0f * (qx2 + qz2);
    float r12 = 2.0f * (qyqz - qxqw);
    
    float r20 = 2.0f * (qxqz - qyqw);
    float r21 = 2.0f * (qyqz + qxqw);
    float r22 = 1.0f - 2.0f * (qx2 + qy2);

    return (Matrix) {
        r00 * scale.x, r01 * scale.y, r02 * scale.z, translate.x,
        r10 * scale.x, r11 * scale.y, r12 * scale.z, translate.y,
        r20 * scale.x, r21 * scale.y, r22 * scale.z, translate.z,
        0, 0, 0, 1
    };
}

static inline Matrix r3d_matrix_normal(const Matrix* transform)
{
    Matrix result = {0};

    float a00 = transform->m0,  a01 = transform->m1,  a02 = transform->m2,  a03 = transform->m3;
    float a10 = transform->m4,  a11 = transform->m5,  a12 = transform->m6,  a13 = transform->m7;
    float a20 = transform->m8,  a21 = transform->m9,  a22 = transform->m10, a23 = transform->m11;
    float a30 = transform->m12, a31 = transform->m13, a32 = transform->m14, a33 = transform->m15;

    float b00 = a00*a11 - a01*a10;
    float b01 = a00*a12 - a02*a10;
    float b02 = a00*a13 - a03*a10;
    float b03 = a01*a12 - a02*a11;
    float b04 = a01*a13 - a03*a11;
    float b05 = a02*a13 - a03*a12;
    float b06 = a20*a31 - a21*a30;
    float b07 = a20*a32 - a22*a30;
    float b08 = a20*a33 - a23*a30;
    float b09 = a21*a32 - a22*a31;
    float b10 = a21*a33 - a23*a31;
    float b11 = a22*a33 - a23*a32;

    float invDet = 1.0f/(b00*b11 - b01*b10 + b02*b09 + b03*b08 - b04*b07 + b05*b06);

    result.m0 = (a11*b11 - a12*b10 + a13*b09)*invDet;
    result.m1 = (-a10*b11 + a12*b08 - a13*b07)*invDet;
    result.m2 = (a10*b10 - a11*b08 + a13*b06)*invDet;

    result.m4 = (-a01*b11 + a02*b10 - a03*b09)*invDet;
    result.m5 = (a00*b11 - a02*b08 + a03*b07)*invDet;
    result.m6 = (-a00*b10 + a01*b08 - a03*b06)*invDet;

    result.m8 = (a31*b05 - a32*b04 + a33*b03)*invDet;
    result.m9 = (-a30*b05 + a32*b02 - a33*b01)*invDet;
    result.m10 = (a30*b04 - a31*b02 + a33*b00)*invDet;

    result.m15 = 1.0f;

    return result;
}

// ========================================
// SHAPE FUNCTIONS
// ========================================

static inline r3d_oriented_box_t r3d_compute_obb(BoundingBox aabb, Matrix transform)
{
    r3d_oriented_box_t obb;

    obb.halfExtents.x = (aabb.max.x - aabb.min.x) * 0.5f;
    obb.halfExtents.y = (aabb.max.y - aabb.min.y) * 0.5f;
    obb.halfExtents.z = (aabb.max.z - aabb.min.z) * 0.5f;

    Vector3 localCenter = {
        (aabb.min.x + aabb.max.x) * 0.5f,
        (aabb.min.y + aabb.max.y) * 0.5f,
        (aabb.min.z + aabb.max.z) * 0.5f
    };

    obb.center.x = transform.m0*localCenter.x + transform.m4*localCenter.y + transform.m8*localCenter.z + transform.m12;
    obb.center.y = transform.m1*localCenter.x + transform.m5*localCenter.y + transform.m9*localCenter.z + transform.m13;
    obb.center.z = transform.m2*localCenter.x + transform.m6*localCenter.y + transform.m10*localCenter.z + transform.m14;

    obb.axisX = (Vector3){transform.m0, transform.m1, transform.m2};
    obb.axisY = (Vector3){transform.m4, transform.m5, transform.m6};
    obb.axisZ = (Vector3){transform.m8, transform.m9, transform.m10};

    return obb;
}

#endif // R3D_COMMON_MATH_H
