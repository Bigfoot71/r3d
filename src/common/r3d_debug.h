/* r3d_debug.h -- Common R3D Debug Helpers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_DEBUG_H
#define R3D_COMMON_DEBUG_H

#include <raylib.h>
#include <glad.h>

static inline void r3d_debug_clear_opengl_errors(void)
{
    while (glGetError() != GL_NO_ERROR);
}

static inline void r3d_debug_check_opengl_error(const char* msg)
{
    int err = glGetError();
    if (err != GL_NO_ERROR) {
        TraceLog(LOG_ERROR, "R3D: OpenGL Error (%s): 0x%04x", msg, err);
    }
}

#endif // R3D_COMMON_DEBUG_H
