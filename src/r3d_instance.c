/* r3d_instance.c -- R3D Instance Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_instance.h>
#include <raylib.h>
#include <stddef.h>
#include <glad.h>

#include "./common/r3d_helper.h"
#include "./r3d_config.h"

// ========================================
// INTERNAL CONSTANTS
// ========================================

static const size_t INSTANCE_ATTRIBUTE_SIZE[R3D_INSTANCE_ATTRIBUTE_COUNT] = {
    /* POSITION */  sizeof(Vector3),
    /* ROTATION */  sizeof(Quaternion),
    /* SCALE    */  sizeof(Vector3),
    /* COLOR    */  sizeof(Color)
};

// ========================================
// PUBLIC API
// ========================================

R3D_InstanceBuffer R3D_LoadInstanceBuffer(int capacity, R3D_InstanceFlag flags)
{
    R3D_InstanceBuffer buffer = {0};

    glGenBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer.buffers);

    for (int i = 0; i < R3D_INSTANCE_ATTRIBUTE_COUNT; i++) {
        if ((1 << i) & flags) {
            glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[i]);
            glBufferData(GL_ARRAY_BUFFER, capacity * INSTANCE_ATTRIBUTE_SIZE[i], NULL, GL_DYNAMIC_DRAW);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    buffer.capacity = capacity;
    buffer.flags = flags;

    return buffer;
}

void R3D_UnloadInstanceBuffer(R3D_InstanceBuffer buffer)
{
    glDeleteBuffers(R3D_INSTANCE_ATTRIBUTE_COUNT, buffer.buffers);
}

void R3D_UploadInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlag flag, int offset, int count, void* data)
{
    int index = r3d_lsb_index(flag);
    if (index < 0 || index >= R3D_INSTANCE_ATTRIBUTE_COUNT) {
        R3D_TRACELOG(LOG_WARNING, "R3D: UploadInstances -> invalid attribute flag (0x%X)", flag);
        return;
    }

    if ((flag & buffer.flags) == 0) {
        R3D_TRACELOG(LOG_WARNING, "R3D: UploadInstances -> attribute not allocated for this buffer (flag=0x%X)", flag);
        return;
    }

    if (offset + count > buffer.capacity) {
        R3D_TRACELOG(LOG_WARNING, "R3D: UploadInstances -> range out of bounds (offset=%d, count=%d, capacity=%d)", offset, count, buffer.capacity);
        return;
    }

    int attrSize = INSTANCE_ATTRIBUTE_SIZE[index];

    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[index]);
    glBufferSubData(GL_ARRAY_BUFFER, offset * attrSize, count * attrSize, data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void* R3D_MapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlag flag)
{
    int index = r3d_lsb_index(flag);
    if (index < 0 || index >= R3D_INSTANCE_ATTRIBUTE_COUNT) {
        R3D_TRACELOG(LOG_WARNING, "R3D: MapInstances -> invalid attribute flag (0x%X)", flag);
        return NULL;
    }

    if ((flag & buffer.flags) == 0) {
        R3D_TRACELOG(LOG_WARNING, "R3D: MapInstances -> attribute not allocated for this buffer (flag=0x%X)", flag);
        return NULL;
    }

    glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[index]);
    void* ptrMap = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (!ptrMap) {
        R3D_TRACELOG(LOG_WARNING, "R3D: MapInstances -> failed to map GPU buffer (flag=0x%X)", flag);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return ptrMap;
}

void R3D_UnmapInstances(R3D_InstanceBuffer buffer, R3D_InstanceFlag flags)
{
    for (int i = 0; i < R3D_INSTANCE_ATTRIBUTE_COUNT; i++) {
        if (((1 << i) & flags) & buffer.flags) {
            glBindBuffer(GL_ARRAY_BUFFER, buffer.buffers[i]);
            glUnmapBuffer(GL_ARRAY_BUFFER);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
