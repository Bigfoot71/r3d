/* r3d_instance.h -- R3D Instance Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_INSTANCE_H
#define R3D_INSTANCE_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Instance
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

#define R3D_INSTANCE_ATTRIBUTE_COUNT 5

// ========================================
// ENUM / FLAGS
// ========================================

/**
 * @brief Bitmask defining which instance attributes are present.
 *
 * Each bit enables one per-instance attribute stream. Attribute data is stored
 * in separate GPU buffers, one buffer per enabled attribute.
 *
 * The memory format of each enabled attribute is defined by `R3D_InstanceLayout`.
 * In shaders, these attributes are exposed as floating-point vectors, including
 * normalized integer formats such as UNORM/SNORM.
 */
typedef uint32_t R3D_InstanceFlags;

#define R3D_INSTANCE_POSITION   (1u << 0)   /*< Position attribute: 3 components */
#define R3D_INSTANCE_ROTATION   (1u << 1)   /*< Rotation attribute: 4 components (quaternion) */
#define R3D_INSTANCE_SCALE      (1u << 2)   /*< Scale attribute: 3 components */
#define R3D_INSTANCE_COLOR      (1u << 3)   /*< Color attribute: 4 components */
#define R3D_INSTANCE_CUSTOM     (1u << 4)   /*< Custom attribute: 4 components */

/**
 * @brief Storage format used by an instance attribute.
 *
 * The selected format controls how the attribute is stored in GPU memory.
 * All formats are read by the shader as floating-point values.
 *
 * UNORM formats are converted to the [0, 1] range.
 * SNORM formats are converted to the [-1, 1] range.
 */
typedef enum R3D_InstanceFormat {
    R3D_INSTANCE_FORMAT_FLOAT32,    ///< 32-bit floating-point component.
    R3D_INSTANCE_FORMAT_FLOAT16,    ///< 16-bit floating-point component.
    R3D_INSTANCE_FORMAT_UNORM16,    ///< 16-bit unsigned normalized component.
    R3D_INSTANCE_FORMAT_SNORM16,    ///< 16-bit signed normalized component.
    R3D_INSTANCE_FORMAT_UNORM8,     ///< 8-bit unsigned normalized component.
    R3D_INSTANCE_FORMAT_SNORM8,     ///< 8-bit signed normalized component.

    R3D_INSTANCE_FORMAT_COUNT       ///< Number of available instance formats.
} R3D_InstanceFormat;

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief Describes the layout of an instance buffer.
 *
 * `flags` defines which instance attributes are allocated.
 * `formats` defines the storage format used by each attribute.
 *
 * The `formats` array is indexed in the same order as the instance attribute
 * flags:
 *
 * - index 0: `R3D_INSTANCE_POSITION`
 * - index 1: `R3D_INSTANCE_ROTATION`
 * - index 2: `R3D_INSTANCE_SCALE`
 * - index 3: `R3D_INSTANCE_COLOR`
 * - index 4: `R3D_INSTANCE_CUSTOM`
 *
 * Data uploaded or mapped for an attribute must match the format selected for
 * that attribute.
 */
typedef struct R3D_InstanceLayout {
    R3D_InstanceFormat formats[R3D_INSTANCE_ATTRIBUTE_COUNT];   ///< Storage format for each instance attribute.
    R3D_InstanceFlags flags;                                    ///< Enabled instance attribute mask.
} R3D_InstanceLayout;

/**
 * @brief GPU buffers storing instance attribute streams.
 *
 * Each enabled attribute owns one GPU buffer. The enabled attributes and their
 * storage formats are described by `layout`.
 */
typedef struct R3D_InstanceBuffer {
    uint32_t buffers[R3D_INSTANCE_ATTRIBUTE_COUNT];     ///< One GPU buffer per attribute, indexed by attribute order.
    R3D_InstanceLayout layout;                          ///< Instance buffer layout.
    int capacity;                                       ///< Maximum number of instances.
} R3D_InstanceBuffer;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates instance buffers on the GPU using the default layout.
 *
 * This is the simple entry point. It allocates one GPU buffer for each enabled
 * attribute in `flags`.
 *
 * Default formats:
 *
 * - position: FLOAT32
 * - rotation: FLOAT32
 * - scale:    FLOAT32
 * - color:    UNORM8
 * - custom:   FLOAT32
 *
 * @param capacity Maximum number of instances.
 * @param flags    Attribute mask to allocate.
 *
 * @return Initialized instance buffer, or an empty buffer on failure.
 */
R3DAPI R3D_InstanceBuffer R3D_LoadInstanceBuffer(int capacity, R3D_InstanceFlags flags);

/**
 * @brief Creates instance buffers on the GPU using a custom layout.
 *
 * This is the advanced entry point. It allocates one GPU buffer for each
 * enabled attribute in `layout.flags`, using the corresponding storage format
 * from `layout.formats`.
 *
 * Data uploaded or mapped for each attribute must match the format selected in
 * the layout.
 *
 * @param capacity Maximum number of instances.
 * @param layout   Instance layout describing enabled attributes and formats.
 *
 * @return Initialized instance buffer, or an empty buffer on failure.
 */
R3DAPI R3D_InstanceBuffer R3D_LoadInstanceBufferEx(int capacity, R3D_InstanceLayout layout);

/**
 * @brief Destroy all GPU buffers owned by this instance buffer.
 */
R3DAPI void R3D_UnloadInstanceBuffer(R3D_InstanceBuffer buffer);

/**
 * @brief Grow the GPU buffers of an instance buffer to a new capacity.
 *
 * Only expands; if newCapacity <= buffer->capacity the call is a no-op.
 * All attribute buffers present in buffer->flags are reallocated and
 * if keepData is true, their existing content is copied to the new
 * buffers before the old ones are deleted.
 *
 * @param buffer      Instance buffer to resize (updated in place).
 * @param newCapacity Desired minimum capacity in number of instances.
 * @param keepData    If true, preserves existing instance data.
 */
R3DAPI void R3D_ResizeInstanceBuffer(R3D_InstanceBuffer* buffer, int newCapacity, bool keepData);

/**
 * @brief Upload a contiguous range of instance data to a GPU buffer.
 *
 * @param buffer  Instance buffer containing the target GPU buffer.
 * @param flag    Attribute to update (single bit).
 * @param offset  First instance index.
 * @param count   Number of instances to upload.
 * @param data    Source data pointer.
 * @param discard If true, the entire GPU buffer is orphaned before upload,
 *                avoiding a GPU/CPU sync at the cost of discarding existing data.
 *                Safe to use when rewriting the full buffer each frame.
 */
R3DAPI void R3D_UploadInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlags flag,
                                int offset, int count, const void* data, bool discard);

/**
 * @brief Map an entire attribute buffer for CPU write access.
 *
 * Call R3D_UnmapInstances when done writing.
 *
 * @param buffer  Instance buffer containing the target GPU buffer.
 * @param flag    Attribute to map (single bit).
 * @param discard If true, existing buffer contents are invalidated,
 *                allowing the driver to return a fresh memory region
 *                without stalling on in-flight GPU reads.
 * @return        Writable pointer to the full buffer, or NULL on error.
 */
R3DAPI void* R3D_MapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlags flag, bool discard);

/**
 * @brief Map a sub-range of an attribute buffer for CPU write access.
 *
 * Prefer this over R3D_MapInstances for partial updates to avoid
 * touching unrelated data. Call R3D_UnmapInstances when done writing.
 *
 * @param buffer  Instance buffer containing the target GPU buffer.
 * @param flag    Attribute to map (single bit).
 * @param offset  First instance index of the mapped range.
 * @param count   Number of instances to map.
 * @param discard If true, the mapped range is invalidated, allowing the
 *                driver to skip syncing that region with in-flight GPU reads.
 * @return        Writable pointer to the mapped range, or NULL on error.
 */
R3DAPI void* R3D_MapInstancesEx(R3D_InstanceBuffer buffer, R3D_InstanceFlags flag,
                                int offset, int count, bool discard);

/**
 * @brief Unmap one or more previously mapped attribute buffers.
 * @param flags Bitmask of attributes to unmap.
 */
R3DAPI void R3D_UnmapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlags flags);

/**
 * @brief Sets the storage format of an instance attribute in a layout.
 *
 * `attribute` must be a single instance attribute flag, such as
 * `R3D_INSTANCE_POSITION`, not a combination of multiple flags.
 *
 * This function only changes the format stored in the layout. It does not
 * enable the attribute in `layout.flags`.
 *
 * @param layout    Layout to modify.
 * @param attribute Single attribute flag to modify.
 * @param format    New storage format.
 */
R3DAPI void R3D_SetInstanceFormat(R3D_InstanceLayout* layout, R3D_InstanceFlags attribute, R3D_InstanceFormat format);

/**
 * @brief Gets the storage format of an instance attribute from a layout.
 *
 * `attribute` must be a single instance attribute flag, such as
 * `R3D_INSTANCE_POSITION`, not a combination of multiple flags.
 *
 * @param layout    Layout to read from.
 * @param attribute Single attribute flag to query.
 *
 * @return Storage format of the requested attribute, or FLOAT32 if the
 * attribute flag is invalid.
 */
R3DAPI R3D_InstanceFormat R3D_GetInstanceFormat(R3D_InstanceLayout layout, R3D_InstanceFlags attribute);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Instance

#endif // R3D_INSTANCE_H
