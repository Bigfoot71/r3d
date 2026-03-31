/* r3d_screen_shader.h -- R3D Screen Shader Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SCREEN_SHADER_H
#define R3D_SCREEN_SHADER_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup ScreenShader
 * @{
 */

// ========================================
// OPAQUE TYPES
// ========================================

typedef struct R3D_ShaderCustom R3D_ScreenShader;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a screen shader from a file.
 *
 * The shader must define a single entry point:
 * `void fragment()`. Any other entry point, such as `vertex()`,
 * or any varyings will be ignored.
 *
 * @param filePath Path to the shader source file.
 * @return Pointer to the loaded screen shader, or NULL on failure.
 */
R3DAPI R3D_ScreenShader* R3D_LoadScreenShader(const char* filePath);

/**
 * @brief Loads a screen shader from a source code string in memory.
 *
 * The shader must define a single entry point:
 * `void fragment()`. Any other entry point, such as `vertex()`,
 * or any varyings will be ignored.
 *
 * @param code Null-terminated shader source code.
 * @return Pointer to the loaded screen shader, or NULL on failure.
 */
R3DAPI R3D_ScreenShader* R3D_LoadScreenShaderFromMemory(const char* code);

/**
 * @brief Creates an alias of an existing screen shader.
 *
 * The alias shares the same compiled program as the original but holds its own
 * independent uniform and sampler state. Typical use cases include pre-configuring
 * aliases for distinct effects (e.g. different convolution kernels), or running
 * the same shader multiple times in a post-process chain with different parameters
 * at each pass.
 *
 * Uniform and sampler state is copied from the original at the moment this
 * function is called, not from the shader source defaults. Any values set
 * on the original after compilation but before this call will be reflected
 * in the alias; values set afterward will not.
 *
 * @note The alias does not own the program. Always unload all aliases before
 *       unloading the original, or the alias program references become dangling.
 *
 * @param shader The original screen shader to alias.
 * @return Pointer to the alias, or NULL on failure.
 */
R3DAPI R3D_ScreenShader* R3D_LoadScreenShaderAlias(R3D_ScreenShader* shader);

/**
 * @brief Unloads and destroys a screen shader.
 *
 * If the shader owns its program shaders (i.e. it was created with @ref R3D_LoadScreenShader
 * or @ref R3D_LoadScreenShaderFromMemory), they are deleted. Aliases created from this
 * shader via @ref R3D_LoadScreenShaderAlias must be unloaded beforehand, as they
 * share the same programs and will be left with dangling references.
 *
 * @param shader Screen shader to unload.
 */
R3DAPI void R3D_UnloadScreenShader(R3D_ScreenShader* shader);

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
 * @param shader Target screen shader.
 *               May be NULL. In that case, the call is ignored
 *               and a warning is logged.
 * @param name   Name of the uniform. Must not be NULL.
 * @param value  Pointer to the uniform value. Must not be NULL.
 */
R3DAPI void R3D_SetScreenShaderUniform(R3D_ScreenShader* shader, const char* name, const void* value);

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
 * @param shader  Target screen shader.
 *                May be NULL. In that case, the call is ignored
 *                and a warning is logged.
 * @param name    Name of the sampler uniform. Must not be NULL.
 * @param texture Texture to bind to the sampler.
 */
R3DAPI void R3D_SetScreenShaderSampler(R3D_ScreenShader* shader, const char* name, Texture texture);

/**
 * @brief Sets the list of screen shaders to execute at the end of the frame.
 *
 * The maximum number of shaders is defined by `R3D_MAX_SCREEN_SHADERS`. 
 * If the provided count exceeds this limit, a warning is emitted and only 
 * the first `R3D_MAX_SCREEN_SHADERS` shaders are used.
 *
 * Shader pointers are copied internally, so the original array can be modified or freed after the call.
 * NULL entries are allowed safely within the list.
 *
 * Calling this function resets all internal screen shaders before copying the new list.
 * To disable all screen shaders, call this function with `shaders = NULL` and/or `count = 0`.
 *
 * @param shaders Array of pointers to R3D_ScreenShader objects.
 * @param count Number of shaders in the array.
 */
R3DAPI void R3D_SetScreenShaderChain(R3D_ScreenShader** shaders, int count);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of ScreenShader

#endif // R3D_SCREEN_SHADER_H
