/* r3d_pass.h -- Common R3D Passes
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_PASS_H
#define R3D_COMMON_PASS_H

#include <raylib.h>
#include <glad.h>

// ========================================
// COMMON ENVIRONMENT GENERATION
// ========================================

void r3d_pass_prepare_irradiance(int layerMap, GLuint srcCubemap, int srcSize);
void r3d_pass_prepare_prefilter(int layerMap, GLuint srcCubemap, int srcSize);

#endif // R3D_COMMON_PASS_H
