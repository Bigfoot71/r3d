#ifndef R3D_COMMON_IMAGE_H
#define R3D_COMMON_IMAGE_H

#include <raylib.h>

/**
 * @brief Composes an RGB image by mapping each source image to its corresponding color channel.
 * 
 * This function creates a new RGB image by sampling the red, green, and blue channels
 * from up to three source images and placing them in the corresponding channels of
 * the output image:
 *   - R channel from sources[0]
 *   - G channel from sources[1]
 *   - B channel from sources[2]
 * 
 * At least one source must be non-NULL to produce a valid output image.
 * 
 * Source images can have different dimensions; the output image will have a width and height
 * equal to the maximum width and height among all non-NULL sources. Nearest neighbor
 * sampling is used for rescaling each channel, using fixed-point 16.16 arithmetic.
 * 
 * @param sources Array of 3 source image pointers (can be NULL). Each index maps to the
 *                corresponding output channel (0 = red, 1 = green, 2 = blue).
 * @param defaultColor Default color used for channels when the corresponding source is NULL.
 * 
 * @return New composed RGB image, or an empty image if the composition fails.
 * 
 * @note This function is particularly useful for composing ORM (Occlusion/Roughness/Metalness)
 *       textures from separate grayscale sources.
 */
Image r3d_compose_images_rgb(const Image* sources[3], Color defaultColor);

#endif // R3D_COMMON_IMAGE_H
