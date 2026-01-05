/* r3d_cubemap.c -- R3D Cubemap Module.
 *
 * Copyright (c) 2025 Le Juez Victor
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

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_shader.h"
#include "./r3d_core_state.h"

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static R3D_CubemapLayout detect_cubemap_layout(Image image)
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

static int get_cubemap_size_from_layout(Image image, R3D_CubemapLayout layout)
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

static R3D_Cubemap load_cubemap_from_panorama(Image image, int size)
{
    R3D_Cubemap cubemap = R3D_LoadCubemapEmpty(size);
    Texture2D panorama = LoadTextureFromImage(image);
    SetTextureFilter(panorama, TEXTURE_FILTER_BILINEAR);
    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE(prepare.cubemapFromEquirectangular);
    R3D_SHADER_SET_MAT4(prepare.cubemapFromEquirectangular, uMatProj, matProj);
    R3D_SHADER_BIND_SAMPLER_2D(prepare.cubemapFromEquirectangular, uTexEquirectangular, panorama.id);

    glBindFramebuffer(GL_FRAMEBUFFER, cubemap.fbo);
    glViewport(0, 0, size, size);
    glDisable(GL_CULL_FACE);

    for (int i = 0; i < 6; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap.texture, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        R3D_SHADER_SET_MAT4(prepare.cubemapFromEquirectangular, uMatView, R3D.matCubeViews[i]);
        R3D_PRIMITIVE_DRAW_CUBE();
    }

    R3D_SHADER_UNBIND_SAMPLER_2D(prepare.cubemapFromEquirectangular, uTexEquirectangular);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    UnloadTexture(panorama);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_CULL_FACE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.texture);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return cubemap;
}

static R3D_Cubemap load_cubemap_from_line_vertical(Image image, int size)
{
    Image workImage = image;

    if (image.format != PIXELFORMAT_UNCOMPRESSED_R16G16B16) {
        workImage = ImageCopy(image);
        ImageFormat(&workImage, PIXELFORMAT_UNCOMPRESSED_R16G16B16);
    }

    int faceSize = size * size * 3 * sizeof(uint16_t);
    R3D_Cubemap cubemap = R3D_LoadCubemapEmpty(size);

    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.texture);
    for (int i = 0; i < 6; i++) {
        glTexSubImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
            0, 0, size, size, GL_RGB, GL_HALF_FLOAT,
            (uint8_t*)workImage.data + i * faceSize
        );
    }

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    if (workImage.data != image.data) {
        UnloadImage(workImage);
    }

    return cubemap;
}

static Image alloc_work_faces_image(Image source, int size)
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

static R3D_Cubemap load_cubemap_from_line_horizontal(Image image, int size)
{
    Image faces = alloc_work_faces_image(image, size);  // ← Passe l'image source
    
    for (int i = 0; i < 6; i++) {
        Rectangle srcRect = {(float)i * size, 0, (float)size, (float)size};
        Rectangle dstRect = {0, (float)i * size, (float)size, (float)size};
        ImageDraw(&faces, image, srcRect, dstRect, WHITE);
    }

    R3D_Cubemap cubemap = load_cubemap_from_line_vertical(faces, size);
    UnloadImage(faces);

    return cubemap;
}

static R3D_Cubemap load_cubemap_from_cross_three_by_four(Image image, int size)
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

static R3D_Cubemap load_cubemap_from_cross_four_by_three(Image image, int size)
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

// ========================================
// PUBLIC API
// ========================================

R3D_Cubemap R3D_LoadCubemapEmpty(int size)
{
    R3D_Cubemap cubemap = {0};
    cubemap.size = size;

    glGenFramebuffers(1, &cubemap.fbo);

    glGenTextures(1, &cubemap.texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.texture);

    int mipLevels = 1 + (int)floor(log2(size));

    for (int level = 0; level < mipLevels; level++) {
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

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return cubemap;
}

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
            TraceLog(LOG_WARNING, "R3D: Failed to detect cubemap image layout");
            return cubemap;
        }
    }

    int size = get_cubemap_size_from_layout(image, layout);
    if (size == 0) {
        TraceLog(LOG_WARNING, "R3D: Cubemap layout not recognized (layout: %i)", layout);
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

    return cubemap;
}

void R3D_UnloadCubemap(R3D_Cubemap cubemap)
{
    glDeleteFramebuffers(1, &cubemap.fbo);
    glDeleteTextures(1, &cubemap.texture);
}
