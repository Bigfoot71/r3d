#include "./r3d_image.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

Image r3d_compose_images_rgb(const Image* sources[3], Color defaultColor)
{
    Image image = { 0 };

    /* --- Determine dimensions --- */

    int w = 0, h = 0;
    for (int i = 0; i < 3; i++) {
        if (sources[i]) {
            w = (w < sources[i]->width) ? sources[i]->width : w;
            h = (h < sources[i]->height) ? sources[i]->height : h;
        }
    }

    if (w == 0 || h == 0) {
        return image;
    }

    /* --- Allocation --- */

    uint8_t* pixels = RL_MALLOC(3 * w * h);
    if (pixels == NULL) {
        return image;
    }

    /* --- Calculate scales --- */

    int scaleX[3] = { 0 };
    int scaleY[3] = { 0 };
    for (int i = 0; i < 3; i++) {
        if (sources[i] && sources[i]->width > 0 && sources[i]->height > 0) {
            scaleX[i] = (sources[i]->width << 16) / w;
            scaleY[i] = (sources[i]->height << 16) / h;
        }
    }

    /* --- Calculate bytes per pixels --- */

    int bpp[3] = {
        sources[0] ? GetPixelDataSize(1, 1, sources[0]->format) : 0,
        sources[1] ? GetPixelDataSize(1, 1, sources[1]->format) : 0,
        sources[2] ? GetPixelDataSize(1, 1, sources[2]->format) : 0,
    };

    /* --- Generate optimized loop based on source combination --- */

    int mask = (sources[0] ? 1 : 0) | (sources[1] ? 2 : 0) | (sources[2] ? 4 : 0);

    #define SAMPLE_TEMPLATE(R_CODE, G_CODE, B_CODE) \
        for (int y = 0; y < h; y++) { \
            for (int x = 0; x < w; x++) { \
                Color color = defaultColor; \
                R_CODE; G_CODE; B_CODE; \
                int offset = 3 * (y * w + x); \
                pixels[offset + 0] = color.r; \
                pixels[offset + 1] = color.g; \
                pixels[offset + 2] = color.b; \
            } \
        }

    #define SAMPLE_R do { \
        int srcX = (x * scaleX[0]) >> 16; \
        int srcY = (y * scaleY[0]) >> 16; \
        if (srcX >= sources[0]->width) srcX = sources[0]->width - 1; \
        if (srcY >= sources[0]->height) srcY = sources[0]->height - 1; \
        Color src = GetPixelColor(((uint8_t*)sources[0]->data) + bpp[0] * (srcY * sources[0]->width + srcX), sources[0]->format); \
        color.r = src.r; \
    } while(0)

    #define SAMPLE_G do { \
        int srcX = (x * scaleX[1]) >> 16; \
        int srcY = (y * scaleY[1]) >> 16; \
        if (srcX >= sources[1]->width) srcX = sources[1]->width - 1; \
        if (srcY >= sources[1]->height) srcY = sources[1]->height - 1; \
        Color src = GetPixelColor(((uint8_t*)sources[1]->data) + bpp[1] * (srcY * sources[1]->width + srcX), sources[1]->format); \
        color.g = src.g; \
    } while(0)

    #define SAMPLE_B do { \
        int srcX = (x * scaleX[2]) >> 16; \
        int srcY = (y * scaleY[2]) >> 16; \
        if (srcX >= sources[2]->width) srcX = sources[2]->width - 1; \
        if (srcY >= sources[2]->height) srcY = sources[2]->height - 1; \
        Color src = GetPixelColor(((uint8_t*)sources[2]->data) + bpp[2] * (srcY * sources[2]->width + srcX), sources[2]->format); \
        color.b = src.b; \
    } while(0)

    #define NOOP do {} while(0)

    switch (mask) {
        case 0: break; // No sources
        case 1: SAMPLE_TEMPLATE(SAMPLE_R, NOOP, NOOP); break;
        case 2: SAMPLE_TEMPLATE(NOOP, SAMPLE_G, NOOP); break;
        case 3: SAMPLE_TEMPLATE(SAMPLE_R, SAMPLE_G, NOOP); break;
        case 4: SAMPLE_TEMPLATE(NOOP, NOOP, SAMPLE_B); break;
        case 5: SAMPLE_TEMPLATE(SAMPLE_R, NOOP, SAMPLE_B); break;
        case 6: SAMPLE_TEMPLATE(NOOP, SAMPLE_G, SAMPLE_B); break;
        case 7: SAMPLE_TEMPLATE(SAMPLE_R, SAMPLE_G, SAMPLE_B); break;
    }

    #undef SAMPLE_TEMPLATE
    #undef SAMPLE_R
    #undef SAMPLE_G  
    #undef SAMPLE_B
    #undef NOOP

    image.data = pixels;
    image.width = w;
    image.height = h;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    image.mipmaps = 1;

    return image;
}
