#ifndef R3D_COMMON_INTERNAL_H
#define R3D_COMMON_INTERNAL_H

#include <raylib.h>
#include <glad.h>

// ========================================
// COMMON ENVIRONMENT GENERATION
// ========================================

void r3d_pass_prepare_irradiance(int layerMap, GLuint srcCubemap, int srcSize);
void r3d_pass_prepare_prefilter(int layerMap, GLuint srcCubemap, int srcSize);

#endif // R3D_COMMON_INERNAL_H
