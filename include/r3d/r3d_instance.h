/* r3d_instance.h -- R3D Instance Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_INSTANCE_H
#define R3D_INSTANCE_H

#include "./r3d_platform.h"
#include <stdint.h>

/**
 * @defgroup Instance
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

#define R3D_INSTANCE_ATTRIBUTE_COUNT 4

// ========================================
// ENUM / FLAGS
// ========================================

/**
 * @brief Bitmask defining which instance attributes are present.
 */
typedef int R3D_InstanceFlag;

#define R3D_INSTANCE_POSITION   (1 << 0)    /*< Vector3     */
#define R3D_INSTANCE_ROTATION   (1 << 1)    /*< Quaternion  */
#define R3D_INSTANCE_SCALE      (1 << 2)    /*< Vector3     */
#define R3D_INSTANCE_COLOR      (1 << 3)    /*< Color       */

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief GPU buffers storing instance attribute streams.
 *
 * buffers: One VBO per attribute (indexed by flag order).
 * capcity: Maximum number of instances.
 * flags: Enabled attribute mask.
 */
typedef struct R3D_InstanceBuffer {
    uint32_t buffers[R3D_INSTANCE_ATTRIBUTE_COUNT];
    int capacity;
    int flags;
} R3D_InstanceBuffer;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create instance buffers on the GPU.
 * @param capacity Max instances.
 * @param flags Attribute mask to allocate.
 * @return Initialized instance buffer.
 */
R3DAPI R3D_InstanceBuffer R3D_LoadInstanceBuffer(int capacity, R3D_InstanceFlag flags);

/**
 * @brief Destroy all GPU buffers owned by this instance buffer.
 */
R3DAPI void R3D_UnloadInstanceBuffer(R3D_InstanceBuffer buffer);

/**
 * @brief Upload a contiguous range of instance data.
 * @param flag Attribute being updated (single bit).
 * @param offset First instance index.
 * @param count Number of instances.
 * @param data Source pointer.
 */
R3DAPI void R3D_UploadInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlag flag,
                                int offset, int count, void* data);

/**
 * @brief Map an attribute buffer for CPU write access.
 * @param flag Attribute to map (single bit).
 * @return Writable pointer, or NULL on error.
 */
R3DAPI void* R3D_MapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlag flag);

/**
 * @brief Unmap one or more previously mapped attribute buffers.
 * @param flags Bitmask of attributes to unmap.
 */
R3DAPI void R3D_UnmapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlag flags);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Instance

#endif // R3D_INSTANCE_H
