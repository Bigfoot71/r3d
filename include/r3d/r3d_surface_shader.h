/* r3d_surface_shader.h -- R3D Material Shader Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SURFACE_SHADER_H
#define R3D_SURFACE_SHADER_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup SurfaceShader
 * @{
 */

// ========================================
// OPAQUE TYPES
// ========================================

typedef struct R3D_SurfaceShader R3D_SurfaceShader;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a surface shader from a file.
 *
 * @param filePath Path to the shader source file.
 * @return Pointer to the loaded surface shader, or NULL on failure.
 */
R3DAPI R3D_SurfaceShader* R3D_LoadSurfaceShader(const char* filePath);

/**
 * @brief Loads a surface shader from a source code string in memory.
 *
 * @param code Null-terminated shader source code.
 * @return Pointer to the loaded surface shader, or NULL on failure.
 */
R3DAPI R3D_SurfaceShader* R3D_LoadSurfaceShaderFromMemory(const char* code);

/**
 * @brief Unloads and destroys a surface shader.
 *
 * @param shader Surface shader to unload.
 */
R3DAPI void R3D_UnloadSurfaceShader(R3D_SurfaceShader* shader);

/**
 * @brief Sets a uniform value for the current frame.
 *
 * Once a uniform is set, it remains valid for the all frames.
 * If an uniform is set multiple times during the same frame,
 * the last value defined before R3D_End() is used.
 *
 * Supported types:
 * bool, int, float,
 * ivec2, ivec3, ivec4,
 * vec2, vec3, vec4,
 * mat2, mat3, mat4
 *
 * @warning Boolean values are read as 4 bytes.
 *
 * @param shader Target surface shader.
 * @param name Name of the uniform.
 * @param value Pointer to the uniform value.
 */
R3DAPI void R3D_SetSurfaceShaderUniform(R3D_SurfaceShader* shader, const char* name, const void* value);

/**
 * @brief Sets a texture sampler for the current frame.
 *
 * Once a sampler is set, it remains valid for all frames.
 * If a sampler is set multiple times during the same frame,
 * the last value defined before R3D_End() is used.
 *
 * Supported samplers:
 * sampler1D, sampler2D, sampler3D, samplerCube
 *
 * @param shader Target surface shader.
 * @param name Name of the sampler uniform.
 * @param texture Texture to bind to the sampler.
 */
R3DAPI void R3D_SetSurfaceShaderSampler(R3D_SurfaceShader* shader, const char* name, Texture texture);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of SurfaceShader

#endif // R3D_SURFACE_SHADER_H
