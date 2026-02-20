/* r3d_sky_shader.h -- R3D Sky Shader Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SKY_SHADER_H
#define R3D_SKY_SHADER_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup SkyShader
 * @{
 */

// ========================================
// OPAQUE TYPES
// ========================================

typedef struct R3D_SkyShader R3D_SkyShader;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a sky shader from a file.
 *
 * The shader must define a single entry point:
 * `void fragment()`. Any other entry point, such as `vertex()`,
 * or any varyings will be ignored.
 *
 * @param filePath Path to the shader source file.
 * @return Pointer to the loaded sky shader, or NULL on failure.
 */
R3DAPI R3D_SkyShader* R3D_LoadSkyShader(const char* filePath);

/**
 * @brief Loads a sky shader from a source code string in memory.
 *
 * The shader must define a single entry point:
 * `void fragment()`. Any other entry point, such as `vertex()`,
 * or any varyings will be ignored.
 *
 * @param code Null-terminated shader source code.
 * @return Pointer to the loaded sky shader, or NULL on failure.
 */
R3DAPI R3D_SkyShader* R3D_LoadSkyShaderFromMemory(const char* code);

/**
 * @brief Unloads and destroys a sky shader.
 *
 * @param shader Sky shader to unload.
 */
R3DAPI void R3D_UnloadSkyShader(R3D_SkyShader* shader);

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
 * @param shader Target sky shader.
 *               May be NULL. In that case, the call is ignored
 *               and a warning is logged.
 * @param name   Name of the uniform. Must not be NULL.
 * @param value  Pointer to the uniform value. Must not be NULL.
 */
R3DAPI void R3D_SetSkyShaderUniform(R3D_SkyShader* shader, const char* name, const void* value);

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
 * @param shader  Target sky shader.
 *                May be NULL. In that case, the call is ignored
 *                and a warning is logged.
 * @param name    Name of the sampler uniform. Must not be NULL.
 * @param texture Texture to bind to the sampler.
 */
R3DAPI void R3D_SetSkyShaderSampler(R3D_SkyShader* shader, const char* name, Texture texture);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of SkyShader

#endif // R3D_SKY_SHADER_H
