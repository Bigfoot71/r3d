/* r3d_image.c -- Common R3D Image Functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_image.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <rlgl.h>
#include <glad.h>

#include "../modules/r3d_driver.h"

// ========================================
// IMAGE FUNCTIONS
// ========================================

Image r3d_image_compose_rgb(const Image* sources[3], Color defaultColor)
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

// ========================================
// TEXTURE FUNCTIONS
// ========================================

static void get_texture_format(int format, bool srgb, GLenum* glInternalFormat, GLenum* glFormat, GLenum* glType)
{
    // TODO: Add checks for support of compressed formats (consider WebGL 2 for later)

    *glInternalFormat = 0;
    *glFormat = 0;
    *glType = 0;

    switch (format) {
    case RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: *glInternalFormat = GL_R8; *glFormat = GL_RED; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: *glInternalFormat = GL_RG8; *glFormat = GL_RG; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R5G6B5: *glInternalFormat = GL_RGB565; *glFormat = GL_RGB; *glType = GL_UNSIGNED_SHORT_5_6_5; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8: *glInternalFormat = GL_RGB8; *glFormat = GL_RGB; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R5G5B5A1: *glInternalFormat = GL_RGB5_A1; *glFormat = GL_RGBA; *glType = GL_UNSIGNED_SHORT_5_5_5_1; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: *glInternalFormat = GL_RGBA4; *glFormat = GL_RGBA; *glType = GL_UNSIGNED_SHORT_4_4_4_4; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: *glInternalFormat = GL_RGBA8; *glFormat = GL_RGBA; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R32: *glInternalFormat = GL_R32F; *glFormat = GL_RED; *glType = GL_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32: *glInternalFormat = GL_RGB32F; *glFormat = GL_RGB; *glType = GL_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: *glInternalFormat = GL_RGBA32F; *glFormat = GL_RGBA; *glType = GL_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R16: *glInternalFormat = GL_R16F; *glFormat = GL_RED; *glType = GL_HALF_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16: *glInternalFormat = GL_RGB16F; *glFormat = GL_RGB; *glType = GL_HALF_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16: *glInternalFormat = GL_RGBA16F; *glFormat = GL_RGBA; *glType = GL_HALF_FLOAT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT1_RGB: *glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT1_RGBA: *glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT3_RGBA: *glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT5_RGBA: *glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
    default: TRACELOG(RL_LOG_WARNING, "R3D: Current format not supported (%i)", format); break;
    }

    if (srgb) {
        switch (*glInternalFormat) {
        case GL_RGBA8: *glInternalFormat = GL_SRGB8_ALPHA8; break;
        case GL_RGB8: *glInternalFormat = GL_SRGB8; break;
        // NOT SUPPORTED FOR NOW
        case GL_COMPRESSED_RGBA_BPTC_UNORM: *glInternalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM; break;
        case GL_COMPRESSED_RGBA_ASTC_4x4_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_5x4_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_5x5_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_6x5_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_6x6_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_8x5_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_8x6_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_8x8_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_10x5_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_10x6_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_10x8_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_10x10_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_12x10_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR; break;
        case GL_COMPRESSED_RGBA_ASTC_12x12_KHR: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR; break;
        case GL_COMPRESSED_RGB8_ETC2: *glInternalFormat = GL_COMPRESSED_SRGB8_ETC2; break;
        case GL_COMPRESSED_RGBA8_ETC2_EAC: *glInternalFormat = GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC; break;
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2: *glInternalFormat = GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2; break;
        }
    }
}

static void upload_texture_mipmap(const uint8_t *data, int width, int height, int level, int format, bool srgb)
{
    GLenum glInternalFormat, glFormat, glType;
    get_texture_format(format, srgb, &glInternalFormat, &glFormat, &glType);
    
    if (glInternalFormat == 0) return;
    
    if (format < RL_PIXELFORMAT_COMPRESSED_DXT1_RGB) {
        glTexImage2D(
            GL_TEXTURE_2D, level, glInternalFormat, width, height, 
            0, glFormat, glType, data
        );
    }
    else {
        int size = GetPixelDataSize(width, height, format);
        glCompressedTexImage2D(
            GL_TEXTURE_2D, level, glInternalFormat,
            width, height, 0, size, data
        );
    }
}

static void set_texture_swizzle(int format)
{
    static const GLint grayscale[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
    static const GLint gray_alpha[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
    
    const GLint *mask = NULL;
    if (format == RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) mask = grayscale;
    else if (format == RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) mask = gray_alpha;
    
    if (mask) {
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
    }
}

static void set_texture_wrap(TextureWrap wrap)
{
    static const GLenum wrap_modes[] = {
        [TEXTURE_WRAP_REPEAT] = GL_REPEAT,
        [TEXTURE_WRAP_CLAMP] = GL_CLAMP_TO_EDGE,
        [TEXTURE_WRAP_MIRROR_REPEAT] = GL_MIRRORED_REPEAT,
        [TEXTURE_WRAP_MIRROR_CLAMP] = GL_MIRROR_CLAMP_TO_EDGE
    };
    
    if (wrap < sizeof(wrap_modes) / sizeof(wrap_modes[0]) && wrap_modes[wrap]) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_modes[wrap]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_modes[wrap]);
    }
}

static void set_texture_filter(TextureFilter filter)
{
    typedef struct {
        GLenum mag, min;
        float aniso;
    } FilterMode;
    
    static const FilterMode modes[] = {
        [TEXTURE_FILTER_POINT] = {GL_NEAREST, GL_NEAREST, 0},
        [TEXTURE_FILTER_BILINEAR] = {GL_LINEAR, GL_LINEAR, 0},
        [TEXTURE_FILTER_TRILINEAR] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 0},
        [TEXTURE_FILTER_ANISOTROPIC_4X] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 4.0f},
        [TEXTURE_FILTER_ANISOTROPIC_8X] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 8.0f},
        [TEXTURE_FILTER_ANISOTROPIC_16X] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 16.0f}
    };
    
    if (filter < sizeof(modes) / sizeof(modes[0]) && modes[filter].mag) {
        const FilterMode *m = &modes[filter];
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m->mag);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m->min);
        if (m->aniso > 0) {
            float maxAniso = 1.0f;
            if (r3d_driver_has_anisotropy(&maxAniso)) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fminf(m->aniso, maxAniso));
            }
        }
    }
}

Texture2D r3d_image_upload(const Image* image, TextureWrap wrap, TextureFilter filter, bool srgb)
{
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const uint8_t *dataPtr = image->data;
    int mipW = image->width, mipH = image->height;

    for (int i = 0; i < image->mipmaps; i++) {
        upload_texture_mipmap(dataPtr, mipW, mipH, i, image->format, srgb);
        if (i == 0) set_texture_swizzle(image->format);

        int mipSize = GetPixelDataSize(mipW, mipH, image->format);
        if (dataPtr) dataPtr += mipSize;

        mipW = (mipW > 1) ? mipW / 2 : 1;
        mipH = (mipH > 1) ? mipH / 2 : 1;
    }

    if (image->mipmaps == 1 && filter >= TEXTURE_FILTER_TRILINEAR) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    set_texture_wrap(wrap);
    set_texture_filter(filter);
    glBindTexture(GL_TEXTURE_2D, 0);

    return (Texture2D) {
        .id = id,
        .width = image->width,
        .height = image->height,
        .mipmaps = image->mipmaps,
        .format = image->format
    };
}
