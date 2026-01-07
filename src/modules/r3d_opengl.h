/* r3d_opengl.h -- Internal R3D OpenGL cache module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_OPENGL_H
#define R3D_MODULE_OPENGL_H

#include <raylib.h>

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_opengl_init(void);
void r3d_opengl_quit(void);

bool r3d_opengl_check_ext(const char* name);
bool r3d_opengl_has_anisotropy(float* max);

void r3d_opengl_clear_errors(void);
bool r3d_opengl_check_error(const char* msg);

#endif // R3D_MODULE_OPENGL_H
