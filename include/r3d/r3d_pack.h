/* r3d_pack.h -- R3D Pack Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_PACK_H
#define R3D_PACK_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Pack Pack
 * @{
 */

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Packs a 32-bit float into a 16-bit floating-point value.
 *
 * @param x Value to pack.
 *
 * @return IEEE 754 half-precision representation.
 */
R3DAPI uint16_t R3D_PackFloat16(float x);

/**
 * @brief Packs a float into a 16-bit unsigned normalized value.
 *
 * The input value is clamped to the [0, 1] range before packing.
 *
 * @param x Value to pack.
 *
 * @return 16-bit UNORM representation.
 */
R3DAPI uint16_t R3D_PackUnorm16(float x);

/**
 * @brief Packs a float into a 16-bit signed normalized value.
 *
 * The input value is clamped to the [-1, 1] range before packing.
 *
 * @param x Value to pack.
 *
 * @return 16-bit SNORM representation.
 */
R3DAPI int16_t R3D_PackSnorm16(float x);

/**
 * @brief Packs a float into an 8-bit unsigned normalized value.
 *
 * The input value is clamped to the [0, 1] range before packing.
 *
 * @param x Value to pack.
 *
 * @return 8-bit UNORM representation.
 */
R3DAPI uint8_t R3D_PackUnorm8(float x);

/**
 * @brief Packs a float into an 8-bit signed normalized value.
 *
 * The input value is clamped to the [-1, 1] range before packing.
 *
 * @param x Value to pack.
 *
 * @return 8-bit SNORM representation.
 */
R3DAPI int8_t R3D_PackSnorm8(float x);

/**
 * @brief Unpacks a 16-bit floating-point value into a 32-bit float.
 *
 * @param x IEEE 754 half-precision representation.
 *
 * @return Unpacked float value.
 */
R3DAPI float R3D_UnpackFloat16(uint16_t x);

/**
 * @brief Unpacks a 16-bit unsigned normalized value.
 *
 * The returned value is in the [0, 1] range.
 *
 * @param x 16-bit UNORM representation.
 *
 * @return Unpacked float value.
 */
R3DAPI float R3D_UnpackUnorm16(uint16_t x);

/**
 * @brief Unpacks a 16-bit signed normalized value.
 *
 * The returned value is clamped to the [-1, 1] range.
 *
 * @param x 16-bit SNORM representation.
 *
 * @return Unpacked float value.
 */
R3DAPI float R3D_UnpackSnorm16(int16_t x);

/**
 * @brief Unpacks an 8-bit unsigned normalized value.
 *
 * The returned value is in the [0, 1] range.
 *
 * @param x 8-bit UNORM representation.
 *
 * @return Unpacked float value.
 */
R3DAPI float R3D_UnpackUnorm8(uint8_t x);

/**
 * @brief Unpacks an 8-bit signed normalized value.
 *
 * The returned value is clamped to the [-1, 1] range.
 *
 * @param x 8-bit SNORM representation.
 *
 * @return Unpacked float value.
 */
R3DAPI float R3D_UnpackSnorm8(int8_t x);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Pack

#endif // R3D_PACK_H
