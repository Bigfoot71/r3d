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
// ENUM TYPES
// ========================================

/**
 * @brief Screen shader execution stage.
 *
 * Screen shaders are custom fullscreen post-processing passes inserted at
 * specific points of the frame.
 *
 * The SCENE stage runs before built-in post-processing and receives
 * scene-referred HDR linear color. It is an advanced stage: effects
 * may affect bloom, auto exposure, and all later passes.
 *
 * The POST stage runs after built-in HDR post-processing, but before output
 * conversion. It still receives scene-referred HDR linear color.
 *
 * The OUTPUT stage runs after tonemapping/output conversion, but before
 * anti-aliasing. It receives display-referred LDR color and is suitable for
 * most artistic image effects.
 *
 * The FINAL stage runs after anti-aliasing, before the final blit. It receives
 * the final display-referred image and is suitable for overlays, grain,
 * scanlines, sharpening, fades, and debug visualization.
 */
typedef enum R3D_ScreenShaderStage {
    R3D_SCREEN_SHADER_STAGE_SCENE,  ///< Before built-in post-processing; advanced HDR scene stage.
    R3D_SCREEN_SHADER_STAGE_POST,   ///< After built-in HDR post-processing, before output conversion.
    R3D_SCREEN_SHADER_STAGE_OUTPUT, ///< After output conversion, before anti-aliasing.
    R3D_SCREEN_SHADER_STAGE_FINAL,  ///< After anti-aliasing, before final blit.
    R3D_SCREEN_SHADER_STAGE_COUNT,  ///< Number of screen shader stages.
} R3D_ScreenShaderStage;

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
 * @brief Sets the screen shader chain for a given stage.
 *
 * Screen shaders are executed in the order provided. The maximum number of
 * shaders per stage is `R3D_MAX_SCREEN_SHADERS`; extra entries are ignored
 * and a warning is emitted.
 *
 * Shader pointers are copied internally, so the original array may be modified
 * or freed after the call. NULL entries are allowed and skipped safely.
 *
 * Calling this function replaces the previous chain for the selected stage.
 * To clear a stage, pass `shaders = NULL` or `count = 0`.
 *
 * @param stage Screen shader stage to configure.
 * @param shaders Array of pointers to R3D_ScreenShader objects.
 * @param count Number of shaders in the array.
 */
R3DAPI void R3D_SetScreenShaderChain(R3D_ScreenShaderStage stage, R3D_ScreenShader** shaders, int count);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of ScreenShader

#endif // R3D_SCREEN_SHADER_H
