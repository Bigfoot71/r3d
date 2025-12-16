/* r3d_sprite.h -- R3D Sprite Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SPRITE_H
#define R3D_SPRITE_H

#include "./r3d_material.h"
#include "./r3d_mesh.h"
#include "./r3d_core.h"

/**
 * @defgroup Sprite
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a 3D sprite with billboard rendering and animation support.
 *
 * This structure defines a 3D sprite, which by default is rendered as a billboard around the Y-axis.
 * The sprite supports frame-based animations and can be configured to use various billboard modes.
 *
 * @warning The shadow mode does not handle semi-transparency. Use alpha-cutoff for transparency.
 */
typedef struct R3D_Sprite {
    R3D_Material material;                 ///< The material used for rendering the sprite, including its texture and shading properties.
    R3D_ShadowCastMode shadowCastMode;     ///< The shadow casting mode for the sprite.
    float currentFrame;                    ///< The current animation frame, represented as a floating-point value to allow smooth interpolation.
    Vector2 frameSize;                     ///< The size of a single animation frame, in texture coordinates (width and height).
    int xFrameCount;                       ///< The number of frames along the horizontal (X) axis of the texture.
    int yFrameCount;                       ///< The number of frames along the vertical (Y) axis of the texture.
    R3D_Layer layerMask;                   /**< Bitfield indicating the rendering layer(s) this object belongs to. 
                                                A value of 0 means the object is always rendered. */
} R3D_Sprite;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------
// SPRITE: Sprite Functions
// ----------------------------------------

/**
 * @brief Load a sprite from a texture.
 *
 * This function creates a `R3D_Sprite` using the provided texture.
 * The texture will be used as the albedo of the sprite's material.
 * The default billboard mode applied to the material is `R3D_BILLBOARD_Y_AXIS`.
 *
 * @warning The lifetime of the provided texture is managed by the caller.
 *
 * @param texture The `Texture2D` to be used for the sprite.
 * @param xFrameCount The number of frames in the horizontal direction.
 * @param yFrameCount The number of frames in the vertical direction.
 *
 * @return A `R3D_Sprite` object initialized with the given texture and frame configuration.
 */
R3DAPI R3D_Sprite R3D_LoadSprite(Texture2D texture, int xFrameCount, int yFrameCount);

/**
 * @brief Unload a sprite and free associated resources.
 *
 * This function releases the resources allocated for a `R3D_Sprite`.
 * It should be called when the sprite is no longer needed.
 *
 * @warning This function only unloads non-default textures from the sprite's material,
 * so make sure these textures are not shared with other material instances elsewhere.
 *
 * @param sprite A pointer to the `R3D_Sprite` to be unloaded.
 */
R3DAPI void R3D_UnloadSprite(const R3D_Sprite* sprite);

/**
 * @brief Updates the animation of a sprite.
 *
 * This function updates the current frame of the sprite's animation based on the provided speed. The animation frames are read from
 * right to left, advancing the cursor to the next row after completing a line.
 *
 * @note The `speed` parameter can be calculated as the number of frames per second multiplied by `GetFrameTime()`.
 *
 * @param sprite A pointer to the `R3D_Sprite` to update.
 * @param speed The speed at which the animation progresses, in frames per second.
 */
R3DAPI void R3D_UpdateSprite(R3D_Sprite* sprite, float speed);

/**
 * @brief Updates the animation of a sprite with specified frame boundaries.
 *
 * This function updates the current frame of the sprite's animation while restricting it between `firstFrame` and `lastFrame`.
 * This is useful for spritesheets containing multiple animations.
 *
 * @note The frames are read from right to left, and the cursor moves to the next row after completing a line.
 * @note The `speed` parameter can be calculated as the number of frames per second multiplied by `GetFrameTime()`.
 *
 * @param sprite A pointer to the `R3D_Sprite` to update.
 * @param firstFrame The first frame of the animation loop.
 * @param lastFrame The last frame of the animation loop.
 * @param speed The speed at which the animation progresses, in frames per second.
 */
R3DAPI void R3D_UpdateSpriteEx(R3D_Sprite* sprite, int firstFrame, int lastFrame, float speed);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Sprite

#endif // R3D_SPRITE_H
