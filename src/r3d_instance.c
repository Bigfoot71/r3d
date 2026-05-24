/* r3d_instance.c -- R3D Instance Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_instance.h>
#include <r3d_config.h>
#include <raylib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <glad.h>

#include "./modules/r3d_driver.h"
#include "./common/r3d_helper.h"

// ========================================
// INTERNAL CONSTANTS
// ========================================

#define R3D_INSTANCE_VALID_FLAGS ((1u << R3D_INSTANCE_ATTRIBUTE_COUNT) - 1u)

static const size_t INSTANCE_ATTRIBUTE_COMPONENTS[R3D_INSTANCE_ATTRIBUTE_COUNT] = {
    /* POSITION */  3,
    /* ROTATION */  4,
    /* SCALE    */  3,
    /* COLOR    */  4,
    /* CUSTOM   */  4,
};

static const size_t INSTANCE_FORMAT_SIZE[R3D_INSTANCE_FORMAT_COUNT] = {
    [R3D_INSTANCE_FORMAT_FLOAT32] = 4,
    [R3D_INSTANCE_FORMAT_FLOAT16] = 2,
    [R3D_INSTANCE_FORMAT_UNORM16] = 2,
    [R3D_INSTANCE_FORMAT_SNORM16] = 2,
    [R3D_INSTANCE_FORMAT_UNORM8]  = 1,
    [R3D_INSTANCE_FORMAT_SNORM8]  = 1,
};

// ========================================
// HELPER FUNCTIONS
// ========================================

static bool r3d_instance_format_is_valid(R3D_InstanceFormat format)
{
    return format >= 0 && format < R3D_INSTANCE_FORMAT_COUNT;
}

static size_t get_attribute_size(int attrIndex, R3D_InstanceFormat format)
{
    if (attrIndex < 0 || attrIndex >= R3D_INSTANCE_ATTRIBUTE_COUNT) return 0;
    if (!r3d_instance_format_is_valid(format)) return 0;

    return INSTANCE_ATTRIBUTE_COMPONENTS[attrIndex] * INSTANCE_FORMAT_SIZE[format];
}

static int r3d_instance_attribute_index(R3D_InstanceFlags attribute)
{
    if (attribute == 0 || (attribute & (attribute - 1)) != 0) return -1;

    int index = r3d_lsb_index(attribute);
    if (index < 0 || index >= R3D_INSTANCE_ATTRIBUTE_COUNT) return -1;

    return index;
}

static bool get_buffer_size(int capacity, int attrIndex, R3D_InstanceFormat format, size_t* size)
{
    if (size == NULL) return false;

    size_t attrSize = get_attribute_size(attrIndex, format);
    if (capacity <= 0 || attrSize == 0) return false;

    if ((size_t)capacity > SIZE_MAX / attrSize) return false;

    *size = (size_t)capacity * attrSize;
    return true;
}

// ========================================
// PUBLIC API
// ========================================

R3D_InstanceBuffer R3D_LoadInstanceBuffer(int capacity, R3D_InstanceFlags flags)
{
    R3D_InstanceLayout layout = {
        .formats = {
            R3D_INSTANCE_FORMAT_FLOAT32,
            R3D_INSTANCE_FORMAT_FLOAT32,
            R3D_INSTANCE_FORMAT_FLOAT32,
            R3D_INSTANCE_FORMAT_UNORM8,
            R3D_INSTANCE_FORMAT_FLOAT32,
        },
        .flags = flags,
    };

    return R3D_LoadInstanceBufferEx(capacity, layout);
}

R3D_InstanceBuffer R3D_LoadInstanceBufferEx(int capacity, R3D_InstanceLayout layout)
{
    R3D_InstanceBuffer buffer = {0};

    if (capacity <= 0) {
        R3D_TRACELOG(LOG_WARNING, "InstanceBuffer -> invalid capacity (%d)", capacity);
        return buffer;
    }

    if ((layout.flags & ~R3D_INSTANCE_VALID_FLAGS) != 0) {
        R3D_TRACELOG(LOG_WARNING, "InstanceBuffer -> invalid attribute flags (0x%04x)", layout.flags);
        return buffer;
    }

    if (layout.flags == 0) {
        R3D_TRACELOG(LOG_WARNING, "InstanceBuffer -> no attributes requested");
        return buffer;
    }

    glGenBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer.buffers);

    for (int i = 0; i < R3D_INSTANCE_ATTRIBUTE_COUNT; i++) {
        if (!BIT_TEST(layout.flags, 1u << i)) continue;

        size_t size = 0;
        if (!get_buffer_size(capacity, i, layout.formats[i], &size)) {
            R3D_TRACELOG(LOG_WARNING, "InstanceBuffer -> invalid size or format (attribute=%d, format=%d)", i, layout.formats[i]);
            glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer.buffers);
            return (R3D_InstanceBuffer){0};
        }

        glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[i]);

        r3d_driver_clear_errors();
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, NULL, GL_DYNAMIC_DRAW);

        if (r3d_driver_check_error("InstanceBuffer -> glBufferData failed")) {
            R3D_TRACELOG(LOG_WARNING, "InstanceBuffer -> failed to allocate attribute %d (%.2f MiB)", i, (double)size / (1024.0 * 1024.0));
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer.buffers);
            return (R3D_InstanceBuffer){0};
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    buffer.capacity = capacity;
    buffer.layout = layout;

    R3D_TRACELOG(LOG_INFO, "Instance buffer created successfully (capacity=%d | flags=0x%04x)", capacity, layout.flags);

    return buffer;
}

void R3D_UnloadInstanceBuffer(R3D_InstanceBuffer buffer)
{
    glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer.buffers);
}

void R3D_ResizeInstanceBuffer(R3D_InstanceBuffer* buffer, int newCapacity, bool keepData)
{
    if (buffer == NULL) {
        R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> buffer is NULL");
        return;
    }

    if (newCapacity <= buffer->capacity) {
        return;
    }

    if ((buffer->layout.flags & ~R3D_INSTANCE_VALID_FLAGS) != 0) {
        R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> invalid attribute flags (0x%04x)", buffer->layout.flags);
        return;
    }

    GLuint newBuffers[R3D_INSTANCE_ATTRIBUTE_COUNT] = {0};
    glGenBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, newBuffers);

    for (int i = 0; i < R3D_INSTANCE_ATTRIBUTE_COUNT; i++) {
        if (!BIT_TEST(buffer->layout.flags, 1u << i)) continue;

        size_t newSize = 0;
        if (!get_buffer_size(newCapacity, i, buffer->layout.formats[i], &newSize)) {
            R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> invalid new size or format (attribute=%d, format=%d)", i, buffer->layout.formats[i]);
            glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, newBuffers);
            return;
        }

        glBindBuffer(GL_COPY_WRITE_BUFFER, newBuffers[i]);

        r3d_driver_clear_errors();
        glBufferData(GL_COPY_WRITE_BUFFER, (GLsizeiptr)newSize, NULL, GL_DYNAMIC_DRAW);

        if (r3d_driver_check_error("ResizeInstanceBuffer -> glBufferData failed")) {
            R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> failed to allocate attribute %d (%.2f MiB)", i, (double)newSize / (1024.0 * 1024.0));
            glBindBuffer(GL_COPY_READ_BUFFER, 0);
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
            glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, newBuffers);
            return;
        }

        if (keepData && buffer->capacity > 0) {
            size_t oldSize = 0;
            if (!get_buffer_size(buffer->capacity, i, buffer->layout.formats[i], &oldSize)) {
                R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> invalid old size (attribute=%d)", i);
                glBindBuffer(GL_COPY_READ_BUFFER, 0);
                glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
                glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, newBuffers);
                return;
            }

            if (buffer->buffers[i] == 0) {
                R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> invalid source buffer (attribute=%d)", i);
                glBindBuffer(GL_COPY_READ_BUFFER, 0);
                glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
                glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, newBuffers);
                return;
            }

            glBindBuffer(GL_COPY_READ_BUFFER, buffer->buffers[i]);

            r3d_driver_clear_errors();
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)oldSize);

            if (r3d_driver_check_error("ResizeInstanceBuffer -> glCopyBufferSubData failed")) {
                R3D_TRACELOG(LOG_WARNING, "ResizeInstanceBuffer -> failed to copy attribute %d", i);
                glBindBuffer(GL_COPY_READ_BUFFER, 0);
                glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
                glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, newBuffers);
                return;
            }
        }
    }

    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

    glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer->buffers);
    memcpy(buffer->buffers, newBuffers, R3D_INSTANCE_ATTRIBUTE_COUNT * sizeof(GLuint));
    buffer->capacity = newCapacity;

    R3D_TRACELOG(LOG_INFO, "Instance buffer resized successfully (capacity=%d)", newCapacity);
}

void R3D_UploadInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlags flag, int offset, int count, void* data)
{
    int index = r3d_instance_attribute_index(flag);
    if (index < 0) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> invalid attribute flag (0x%04x)", flag);
        return;
    }

    if (!BIT_TEST(buffer.layout.flags, flag)) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> attribute not allocated for this buffer (flag=0x%04x)", flag);
        return;
    }

    if (buffer.buffers[index] == 0) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> invalid GPU buffer (flag=0x%04x)", flag);
        return;
    }

    if (offset < 0 || count < 0 || offset > buffer.capacity || count > buffer.capacity - offset) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> range out of bounds (offset=%d, count=%d, capacity=%d)", offset, count, buffer.capacity);
        return;
    }

    if (count == 0) {
        return;
    }

    if (data == NULL) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> data is NULL");
        return;
    }

    size_t size = 0;
    if (!get_buffer_size(count, index, buffer.layout.formats[index], &size)) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> invalid upload size (flag=0x%04x)", flag);
        return;
    }

    size_t attrSize = get_attribute_size(index, buffer.layout.formats[index]);
    size_t offsetBytes = (size_t)offset * attrSize;

    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[index]);

    r3d_driver_clear_errors();
    glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offsetBytes, (GLsizeiptr)size, data);

    if (r3d_driver_check_error("UploadInstances -> glBufferSubData failed")) {
        R3D_TRACELOG(LOG_WARNING, "UploadInstances -> failed to upload attribute data (%.2f MiB)", (double)size / (1024.0 * 1024.0));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void* R3D_MapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlags flag)
{
    int index = r3d_instance_attribute_index(flag);
    if (index < 0) {
        R3D_TRACELOG(LOG_WARNING, "MapInstances -> invalid attribute flag (0x%04x)", flag);
        return NULL;
    }

    if (!BIT_TEST(buffer.layout.flags, flag)) {
        R3D_TRACELOG(LOG_WARNING, "MapInstances -> attribute not allocated for this buffer (flag=0x%04x)", flag);
        return NULL;
    }

    if (buffer.buffers[index] == 0) {
        R3D_TRACELOG(LOG_WARNING, "MapInstances -> invalid GPU buffer (flag=0x%04x)", flag);
        return NULL;
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[index]);

    r3d_driver_clear_errors();
    void* ptrMap = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

    if (r3d_driver_check_error("MapInstances -> glMapBuffer failed") || ptrMap == NULL) {
        R3D_TRACELOG(LOG_WARNING, "MapInstances -> failed to map GPU buffer (flag=0x%04x)", flag);
        ptrMap = NULL;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return ptrMap;
}

void R3D_UnmapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlags flags)
{
    if ((flags & ~R3D_INSTANCE_VALID_FLAGS) != 0) {
        R3D_TRACELOG(LOG_WARNING, "UnmapInstances -> invalid attribute flags (0x%04x)", flags);
        flags &= R3D_INSTANCE_VALID_FLAGS;
    }

    for (int i = 0; i < R3D_INSTANCE_ATTRIBUTE_COUNT; i++) {
        R3D_InstanceFlags flag = 1u << i;

        if (!BIT_TEST(flags, flag) || !BIT_TEST(buffer.layout.flags, flag)) {
            continue;
        }

        if (buffer.buffers[i] == 0) {
            R3D_TRACELOG(LOG_WARNING, "UnmapInstances -> invalid GPU buffer (flag=0x%04x)", flag);
            continue;
        }

        glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[i]);

        r3d_driver_clear_errors();
        if (!glUnmapBuffer(GL_ARRAY_BUFFER)) {
            R3D_TRACELOG(LOG_WARNING, "UnmapInstances -> GPU data may be corrupted (flag=0x%04x)", flag);
        }

        r3d_driver_check_error("UnmapInstances -> glUnmapBuffer failed");
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void R3D_SetInstanceFormat(R3D_InstanceLayout* layout, R3D_InstanceFlags attribute, R3D_InstanceFormat format)
{
    if (layout == NULL) {
        R3D_TRACELOG(LOG_WARNING, "SetInstanceFormat -> layout is NULL");
        return;
    }

    int index = r3d_instance_attribute_index(attribute);
    if (index < 0) {
        R3D_TRACELOG(LOG_WARNING, "SetInstanceFormat -> invalid attribute flag (0x%04x)", attribute);
        return;
    }

    if (!r3d_instance_format_is_valid(format)) {
        R3D_TRACELOG(LOG_WARNING, "SetInstanceFormat -> invalid format (%d)", format);
        return;
    }

    layout->formats[index] = format;
}

R3D_InstanceFormat R3D_GetInstanceFormat(R3D_InstanceLayout layout, R3D_InstanceFlags attribute)
{
    int index = r3d_instance_attribute_index(attribute);
    if (index < 0) {
        R3D_TRACELOG(LOG_WARNING, "GetInstanceFormat -> invalid attribute flag (0x%04x)", attribute);
        return R3D_INSTANCE_FORMAT_FLOAT32;
    }

    if (!r3d_instance_format_is_valid(layout.formats[index])) {
        R3D_TRACELOG(LOG_WARNING, "GetInstanceFormat -> invalid format for attribute flag (0x%04x)", attribute);
        return R3D_INSTANCE_FORMAT_FLOAT32;
    }

    return layout.formats[index];
}
