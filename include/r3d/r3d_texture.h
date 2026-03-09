/* r3d_texture.h -- R3D Texture Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_TEXTURE_H
#define R3D_TEXTURE_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup Texture
 * @{
 */

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a texture from the specified file path.
 *
 * If 'isColor' is true, the texture is loaded using the currently defined color space (sRGB by default).
 * The wrap and filter modes are taken from the current global default state.
 *
 * @param fileName The path to the texture file.
 * @param isColor Whether the texture should be treated as color data.
 * @return The loaded texture.
 */
R3DAPI Texture2D R3D_LoadTexture(const char* fileName, bool isColor);

/**
 * @brief Loads a texture from the specified file path.
 *
 * If 'isColor' is true, the texture is loaded using the currently defined color space (sRGB by default).
 *
 * @param fileName The path to the texture file.
 * @param wrap The texture wrap mode.
 * @param filter The texture filter mode.
 * @param isColor Whether the texture should be treated as color data.
 * @return The loaded texture.
 */
R3DAPI Texture2D R3D_LoadTextureEx(const char* fileName, TextureWrap wrap, TextureFilter filter, bool isColor);

/**
 * @brief Loads a texture from the specified image.
 *
 * If 'isColor' is true, the texture is loaded using the currently defined color space (sRGB by default).
 * The wrap and filter modes are taken from the current global default state.
 *
 * @param image The source image.
 * @param isColor Whether the texture should be treated as color data.
 * @return The loaded texture.
 */
R3DAPI Texture2D R3D_LoadTextureFromImage(Image image, bool isColor);

/**
 * @brief Loads a texture from the specified image.
 *
 * If 'isColor' is true, the texture is loaded using the currently defined color space (sRGB by default).
 *
 * @param image The source image.
 * @param wrap The texture wrap mode.
 * @param filter The texture filter mode.
 * @param isColor Whether the texture should be treated as color data.
 * @return The loaded texture.
 */
R3DAPI Texture2D R3D_LoadTextureFromImageEx(Image image, TextureWrap wrap, TextureFilter filter, bool isColor);

/**
 * @brief Loads a texture directly from memory.
 *
 * If 'isColor' is true, the texture is loaded using the currently defined color space (sRGB by default).
 * The wrap and filter modes are taken from the current global default state.
 *
 * @param fileType The file type/extension used to interpret the data.
 * @param fileData A pointer to the file data in memory.
 * @param dataSize The size of the file data in bytes.
 * @param isColor Whether the texture should be treated as color data.
 * @return The loaded texture.
 */
R3DAPI Texture2D R3D_LoadTextureFromMemory(const char* fileType, const void* fileData, int dataSize, bool isColor);

/**
 * @brief Loads a texture directly from memory.
 *
 * If 'isColor' is true, the texture is loaded using the currently defined color space (sRGB by default).
 *
 * @param fileType The file type/extension used to interpret the data.
 * @param fileData A pointer to the file data in memory.
 * @param dataSize The size of the file data in bytes.
 * @param wrap The texture wrap mode.
 * @param filter The texture filter mode.
 * @param isColor Whether the texture should be treated as color data.
 * @return The loaded texture.
 */
R3DAPI Texture2D R3D_LoadTextureFromMemoryEx(const char* fileType, const void* fileData, int dataSize, TextureWrap wrap, TextureFilter filter, bool isColor);

/**
 * @brief Unloads a texture.
 *
 * This function is equivalent to raylib's `UnloadTexture` and is provided for consistency.
 *
 * @param texture The texture to unload.
 */
R3DAPI void R3D_UnloadTexture(Texture2D texture);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Texture

#endif // R3D_TEXTURE_H
