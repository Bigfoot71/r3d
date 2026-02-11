/* r3d_cubemap.c -- R3D Cubemap Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_cubemap.h>
#include <raymath.h>
#include <stdlib.h>
#include <stddef.h>
#include <rlgl.h>
#include <glad.h>

#include "./modules/r3d_driver.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_render.h"
#include "./r3d_core_state.h"

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static const char* get_layout_name(R3D_CubemapLayout layout);
static R3D_CubemapLayout detect_cubemap_layout(Image image);
static int get_cubemap_size_from_layout(Image image, R3D_CubemapLayout layout);
static R3D_Cubemap allocate_cubemap(int size);
static R3D_Cubemap load_cubemap_from_panorama(Image image, int size);
static R3D_Cubemap load_cubemap_from_line_vertical(Image image, int size);
static Image alloc_work_faces_image(Image source, int size);
static R3D_Cubemap load_cubemap_from_line_horizontal(Image image, int size);
static R3D_Cubemap load_cubemap_from_cross_three_by_four(Image image, int size);
static R3D_Cubemap load_cubemap_from_cross_four_by_three(Image image, int size);
static void generate_mipmap(const R3D_Cubemap* cubemap);

// ========================================
// PUBLIC API
// ========================================

R3D_Cubemap R3D_LoadCubemap(const char* fileName, R3D_CubemapLayout layout)
{
    Image image = LoadImage(fileName);
    R3D_Cubemap cubemap = R3D_LoadCubemapFromImage(image, layout);
    UnloadImage(image);
    return cubemap;
}

R3D_Cubemap R3D_LoadCubemapFromImage(Image image, R3D_CubemapLayout layout)
{
    R3D_Cubemap cubemap = {0};

    if (layout == R3D_CUBEMAP_LAYOUT_AUTO_DETECT) {
        layout = detect_cubemap_layout(image);
        if (layout == R3D_CUBEMAP_LAYOUT_AUTO_DETECT) {
            R3D_TRACELOG(LOG_WARNING, "Failed to detect cubemap image layout");
            return cubemap;
        }
    }

    int size = get_cubemap_size_from_layout(image, layout);
    if (size == 0) {
        R3D_TRACELOG(LOG_WARNING, "Cubemap layout not recognized (layout: %i)", layout);
        return cubemap;
    }

    switch (layout) {
    case R3D_CUBEMAP_LAYOUT_LINE_VERTICAL:
        cubemap = load_cubemap_from_line_vertical(image, size);
        break;
    case R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL:
        cubemap = load_cubemap_from_line_horizontal(image, size);
        break;
    case R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR:
        cubemap = load_cubemap_from_cross_three_by_four(image, size);
        break;
    case R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE:
        cubemap = load_cubemap_from_cross_four_by_three(image, size);
        break;
    case R3D_CUBEMAP_LAYOUT_PANORAMA:
        cubemap = load_cubemap_from_panorama(image, size);
        break;
    case R3D_CUBEMAP_LAYOUT_AUTO_DETECT:
        break;
    }

    generate_mipmap(&cubemap);

    R3D_TRACELOG(LOG_INFO, "Cubemap loaded from '%s' successfully (%dx%d)", get_layout_name(layout), size, size);

    return cubemap;
}

R3D_Cubemap R3D_GenCubemapSky(int size, R3D_CubemapSky params)
{
    R3D_Cubemap cubemap = allocate_cubemap(size);
    R3D_UpdateCubemapSky(&cubemap, params);
    return cubemap;
}

void R3D_UnloadCubemap(R3D_Cubemap cubemap)
{
    glDeleteFramebuffers(1, &cubemap.fbo);
    glDeleteTextures(1, &cubemap.texture);
}

void R3D_UpdateCubemapSky(R3D_Cubemap* cubemap, R3D_CubemapSky params)
{
    r3d_driver_invalidate_cache();

    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    glBindFramebuffer(GL_FRAMEBUFFER, cubemap->fbo);
    glViewport(0, 0, cubemap->size, cubemap->size);

    R3D_SHADER_USE_BLT(prepare.cubemapSkybox);
    r3d_driver_disable(GL_CULL_FACE);

    R3D_SHADER_SET_MAT4_BLT(prepare.cubemapSkybox, uMatProj, matProj);
    R3D_SHADER_SET_COL3_BLT(prepare.cubemapSkybox, uSkyTopColor, R3D.colorSpace, params.skyTopColor);
    R3D_SHADER_SET_COL3_BLT(prepare.cubemapSkybox, uSkyHorizonColor, R3D.colorSpace, params.skyHorizonColor);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uSkyHorizonCurve, params.skyHorizonCurve);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uSkyEnergy, params.skyEnergy);
    R3D_SHADER_SET_COL3_BLT(prepare.cubemapSkybox, uGroundBottomColor, R3D.colorSpace, params.groundBottomColor);
    R3D_SHADER_SET_COL3_BLT(prepare.cubemapSkybox, uGroundHorizonColor, R3D.colorSpace, params.groundHorizonColor);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uGroundHorizonCurve, params.groundHorizonCurve);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uGroundEnergy, params.groundEnergy);
    R3D_SHADER_SET_VEC3_BLT(prepare.cubemapSkybox, uSunDirection, Vector3Normalize(Vector3Negate(params.sunDirection)));
    R3D_SHADER_SET_COL3_BLT(prepare.cubemapSkybox, uSunColor, R3D.colorSpace, params.sunColor);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uSunSize, params.sunSize);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uSunCurve, params.sunCurve);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapSkybox, uSunEnergy, params.sunEnergy);

    for (int i = 0; i < 6; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap->texture, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        R3D_SHADER_SET_MAT4_BLT(prepare.cubemapSkybox, uMatView, R3D.matCubeViews[i]);
        R3D_RENDER_CUBE();
    }

    generate_mipmap(cubemap);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    r3d_driver_enable(GL_CULL_FACE);
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

const char* get_layout_name(R3D_CubemapLayout layout)
{
    switch (layout) {
    case R3D_CUBEMAP_LAYOUT_AUTO_DETECT: return "Auto";
    case R3D_CUBEMAP_LAYOUT_LINE_VERTICAL: return "Line Vertical";
    case R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL: return "Line Horizontal";
    case R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR: return "Cross 3/4";
    case R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE: return "Cross 4/3";
    case R3D_CUBEMAP_LAYOUT_PANORAMA: return "Panorama";
    default: break;
    }
    return "Unknown";
}

R3D_CubemapLayout detect_cubemap_layout(Image image)
{
    R3D_CubemapLayout layout = R3D_CUBEMAP_LAYOUT_AUTO_DETECT;

    if (image.width > image.height) {
        if (image.width / 6 == image.height) {
            layout = R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL;
        }
        else if (image.width / 4 == image.height / 3) {
            layout = R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE;
        }
        else if (image.width / 2 == image.height) {
            layout = R3D_CUBEMAP_LAYOUT_PANORAMA;
        }
    }
    else if (image.height > image.width) {
        if (image.height / 6 == image.width) {
            layout = R3D_CUBEMAP_LAYOUT_LINE_VERTICAL;
        }
        else if (image.width / 3 == image.height/4) {
            layout = R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR;
        }
    }
    // Checking for cases where the ratio is not exactly 2:1 but close
    else if (abs(image.width - 2 * image.height) < image.height / 10) {
        layout = R3D_CUBEMAP_LAYOUT_PANORAMA;
    }

    return layout;
}

int get_cubemap_size_from_layout(Image image, R3D_CubemapLayout layout)
{
    int size = 0;

    switch (layout) {
    case R3D_CUBEMAP_LAYOUT_LINE_VERTICAL:
        size = image.height / 6;
        break;
    case R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL:
        size = image.width / 6;
        break;
    case R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR:
        size = image.width / 3;
        break;
    case R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE:
        size = image.width / 4;
        break;
    case R3D_CUBEMAP_LAYOUT_PANORAMA:
        size = image.height;
        break;
    default:
        break;
    }

    return size;
}

R3D_Cubemap allocate_cubemap(int size)
{
    R3D_Cubemap cubemap = {0};
    cubemap.size = size;

    glGenFramebuffers(1, &cubemap.fbo);

    glGenTextures(1, &cubemap.texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.texture);

    int mipCount = r3d_get_mip_levels_1d(size);
    for (int level = 0; level < mipCount; level++) {
        int mipSize = size >> level;
        for (int i = 0; i < 6; i++) {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, level, GL_RGB16F,
                mipSize, mipSize, 0, GL_RGB, GL_HALF_FLOAT, NULL
            );
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, mipCount - 1);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return cubemap;
}

R3D_Cubemap load_cubemap_from_panorama(Image image, int size)
{
    r3d_driver_invalidate_cache();

    R3D_Cubemap cubemap = allocate_cubemap(size);
    Texture2D panorama = LoadTextureFromImage(image);
    SetTextureFilter(panorama, TEXTURE_FILTER_BILINEAR);
    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE_BLT(prepare.cubemapFromEquirectangular);
    R3D_SHADER_SET_MAT4_BLT(prepare.cubemapFromEquirectangular, uMatProj, matProj);
    R3D_SHADER_BIND_SAMPLER_BLT(prepare.cubemapFromEquirectangular, uPanoramaTex, panorama.id);

    glBindFramebuffer(GL_FRAMEBUFFER, cubemap.fbo);
    glViewport(0, 0, size, size);
    r3d_driver_disable(GL_CULL_FACE);

    for (int i = 0; i < 6; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap.texture, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        R3D_SHADER_SET_MAT4_BLT(prepare.cubemapFromEquirectangular, uMatView, R3D.matCubeViews[i]);
        R3D_RENDER_CUBE();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    UnloadTexture(panorama);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    r3d_driver_enable(GL_CULL_FACE);

    return cubemap;
}

R3D_Cubemap load_cubemap_from_line_vertical(Image image, int size)
{
    Image workImage = image;

    if (image.format != PIXELFORMAT_UNCOMPRESSED_R16G16B16) {
        workImage = ImageCopy(image);
        ImageFormat(&workImage, PIXELFORMAT_UNCOMPRESSED_R16G16B16);
    }

    int faceSize = size * size * 3 * sizeof(uint16_t);
    R3D_Cubemap cubemap = allocate_cubemap(size);

    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.texture);
    for (int i = 0; i < 6; i++) {
        glTexSubImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
            0, 0, size, size, GL_RGB, GL_HALF_FLOAT,
            (uint8_t*)workImage.data + i * faceSize
        );
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    if (workImage.data != image.data) {
        UnloadImage(workImage);
    }

    return cubemap;
}

Image alloc_work_faces_image(Image source, int size)
{
    Image image = {0};
    
    int faceSize = GetPixelDataSize(size, size, source.format);

    image.data = RL_MALLOC(6 * faceSize);
    image.width = size;
    image.height = 6 * size;
    image.format = source.format;
    image.mipmaps = 1;

    return image;
}

R3D_Cubemap load_cubemap_from_line_horizontal(Image image, int size)
{
    Image faces = alloc_work_faces_image(image, size);

    for (int i = 0; i < 6; i++) {
        Rectangle srcRect = {(float)i * size, 0, (float)size, (float)size};
        Rectangle dstRect = {0, (float)i * size, (float)size, (float)size};
        ImageDraw(&faces, image, srcRect, dstRect, WHITE);
    }

    R3D_Cubemap cubemap = load_cubemap_from_line_vertical(faces, size);
    UnloadImage(faces);

    return cubemap;
}

R3D_Cubemap load_cubemap_from_cross_three_by_four(Image image, int size)
{
    Rectangle srcRecs[6] = {0};

    for (int i = 0; i < 6; i++) {
        srcRecs[i] = (Rectangle) {0, 0, (float)size, (float)size};
    }

    srcRecs[0].x = (float)size; srcRecs[0].y = (float)size;
    srcRecs[1].x = (float)size; srcRecs[1].y = 3.0f * size;
    srcRecs[2].x = (float)size; srcRecs[2].y = 0;
    srcRecs[3].x = (float)size; srcRecs[3].y = 2.0f * size;
    srcRecs[4].x = 0;           srcRecs[4].y = (float)size;
    srcRecs[5].x = 2.0f * size; srcRecs[5].y = (float)size;

    Image faces = alloc_work_faces_image(image, size);

    for (int i = 0; i < 6; i++) {
        Rectangle dstRec = {0, (float)i * size, (float)size, (float)size};
        ImageDraw(&faces, image, srcRecs[i], dstRec, WHITE);
    }

    R3D_Cubemap cubemap = load_cubemap_from_line_vertical(faces, size);
    UnloadImage(faces);

    return cubemap;
}

R3D_Cubemap load_cubemap_from_cross_four_by_three(Image image, int size)
{
    Rectangle srcRecs[6] = {0};

    for (int i = 0; i < 6; i++) {
        srcRecs[i] = (Rectangle) {0, 0, (float)size, (float)size};
    }

    srcRecs[0].x = 2.0f * size; srcRecs[0].y = (float)size;
    srcRecs[1].x = 0;           srcRecs[1].y = (float)size;
    srcRecs[2].x = (float)size; srcRecs[2].y = 0;
    srcRecs[3].x = (float)size; srcRecs[3].y = 2.0f * size;
    srcRecs[4].x = (float)size; srcRecs[4].y = (float)size;
    srcRecs[5].x = 3.0f * size; srcRecs[5].y = (float)size;

    Image faces = alloc_work_faces_image(image, size);

    for (int i = 0; i < 6; i++) {
        Rectangle dstRec = {0, (float)i * size, (float)size, (float)size};
        ImageDraw(&faces, image, srcRecs[i], dstRec, WHITE);
    }

    R3D_Cubemap cubemap = load_cubemap_from_line_vertical(faces, size);
    UnloadImage(faces);

    return cubemap;
}

void generate_mipmap(const R3D_Cubemap* cubemap)
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap->texture);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}
