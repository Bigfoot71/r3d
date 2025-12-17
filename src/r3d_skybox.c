/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#include <r3d/r3d_skybox.h>
#include <raymath.h>
#include <assert.h>
#include <rlgl.h>
#include <glad.h>

#include "./modules/r3d_shader.h"
#include "./r3d_state.h"

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static TextureCubemap r3d_skybox_load_cubemap_from_layout(const Image* image, CubemapLayout layout)
{
    TextureCubemap cubemap = { 0 };

    /* --- Try to automatically guess layout type --- */

    if (layout == CUBEMAP_LAYOUT_AUTO_DETECT) {
        // Check image width/height to determine the type of cubemap provided
        if (image->width > image->height) {
            if (image->width / 6 == image->height) {
                layout = CUBEMAP_LAYOUT_LINE_HORIZONTAL;
                cubemap.width = image->width / 6;
            }
            else if (image->width / 4 == image->height / 3) {
                layout = CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE;
                cubemap.width = image->width / 4;
            }
        }
        else if (image->height > image->width) {
            if (image->height / 6 == image->width) {
                layout = CUBEMAP_LAYOUT_LINE_VERTICAL;
                cubemap.width = image->height / 6;
            }
            else if (image->width / 3 == image->height/4) {
                layout = CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR;
                cubemap.width = image->width / 3;
            }
        }
    }
    else {
        if (layout == CUBEMAP_LAYOUT_LINE_VERTICAL) cubemap.width = image->height / 6;
        if (layout == CUBEMAP_LAYOUT_LINE_HORIZONTAL) cubemap.width = image->width / 6;
        if (layout == CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR) cubemap.width = image->width / 3;
        if (layout == CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE) cubemap.width = image->width / 4;
    }

    /* --- Checks if the layout could be detected --- */

    if (layout == CUBEMAP_LAYOUT_AUTO_DETECT) {
        TraceLog(LOG_WARNING, "R3D: Failed to detect cubemap image layout");
        return cubemap;
    }

    cubemap.height = cubemap.width;

    /* --- Layout provided or already auto-detected --- */

    int size = cubemap.width;

    bool facesAllocatedHere = false;    //< Indicates whether we are even allocated the faces...
    Image faces = { 0 };                //< Vertical column image

    if (layout == CUBEMAP_LAYOUT_LINE_VERTICAL) {
        faces = *image; // Image data already follows expected convention
    }
    //else if (layout == CUBEMAP_LAYOUT_PANORAMA) {
    //    // REVIEW: CUBEMAP_LAYOUT_PANORAMA does not yet exist in raylib...
    //    //         We currently manage it in a separate function...
    //}
    else
    {
        Rectangle srcRecs[6] = { 0 };

        for (int i = 0; i < 6; i++) {
            srcRecs[i] = (Rectangle){ 0, 0, (float)size, (float)size };
        }

        if (layout == CUBEMAP_LAYOUT_LINE_HORIZONTAL) {
            for (int i = 0; i < 6; i++) {
                srcRecs[i].x = (float)i * size;
            }
        }
        else if (layout == CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR) {
            srcRecs[0].x = (float)size; srcRecs[0].y = (float)size;
            srcRecs[1].x = (float)size; srcRecs[1].y = 3.0f * size;
            srcRecs[2].x = (float)size; srcRecs[2].y = 0;
            srcRecs[3].x = (float)size; srcRecs[3].y = 2.0f * size;
            srcRecs[4].x = 0;           srcRecs[4].y = (float)size;
            srcRecs[5].x = 2.0f * size; srcRecs[5].y = (float)size;
        }
        else if (layout == CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE) {
            srcRecs[0].x = 2.0f * size; srcRecs[0].y = (float)size;
            srcRecs[1].x = 0;           srcRecs[1].y = (float)size;
            srcRecs[2].x = (float)size; srcRecs[2].y = 0;
            srcRecs[3].x = (float)size; srcRecs[3].y = 2.0f * size;
            srcRecs[4].x = (float)size; srcRecs[4].y = (float)size;
            srcRecs[5].x = 3.0f * size; srcRecs[5].y = (float)size;
        }

        /* --- Convert image data to 6 faces in a vertical column, that's the optimum layout for loading --- */

        faces.width = size;
        faces.height = 6 * size;
        faces.format = image->format;
        faces.mipmaps = 1;

        int bytesPerPixel = GetPixelDataSize(1, 1, image->format);
        faces.data = RL_CALLOC(size * size * 6 * bytesPerPixel, 1);
        facesAllocatedHere = true;

        // NOTE: Image formatting does not work with compressed textures

        for (int i = 0; i < 6; i++) {
            Rectangle dstRec = (Rectangle){ 0, (float)i * size, (float)size, (float)size };
            ImageDraw(&faces, *image, srcRecs[i], dstRec, WHITE);
        }
    }

    // NOTE: Cubemap data is expected to be provided as 6 images in a single data array,
    // one after the other (that's a vertical image), following convention: +X, -X, +Y, -Y, +Z, -Z

    cubemap.id = rlLoadTextureCubemap(faces.data, size, faces.format, faces.mipmaps);
    if (cubemap.id == 0) {
        TraceLog(LOG_WARNING, "R3D: Failed to load cubemap image");
        goto cleanup;
    }
    cubemap.format = faces.format;
    cubemap.mipmaps = faces.mipmaps;

    // Generate mipmaps and set texture parameters
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.id);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

cleanup:
    if (facesAllocatedHere) {
        UnloadImage(faces);
    }

    return cubemap;
}

static TextureCubemap r3d_skybox_load_cubemap_from_panorama(Image image, int size)
{
    // Temporarily loads the panorama
    Texture2D panorama = LoadTextureFromImage(image);
    SetTextureFilter(panorama, TEXTURE_FILTER_BILINEAR);

    // Choose the best HDR format available
    GLenum format = r3d_support_get_internal_format(GL_RGB16F, true);

    // Create the skybox cubemap texture
    unsigned int cubemapId = 0;
    glGenTextures(1, &cubemapId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapId);
    for (int i = 0; i < 6; i++) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format,
            size, size, 0, GL_RGB, GL_FLOAT, NULL
        );
    }

    // Create and configure the working framebuffer
    unsigned int fbo = rlLoadFramebuffer();

    // Calculate projection matrix
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    // Enable and configure the shader for converting panorama to cubemap
    r3d_shader_enable(prepare.cubemapFromEquirectangular);
    r3d_shader_set_mat4(prepare.cubemapFromEquirectangular, uMatProj, matProj);
    r3d_shader_bind_sampler2D(prepare.cubemapFromEquirectangular, uTexEquirectangular, panorama.id);

    // Set viewport to framebuffer dimensions
    glViewport(0, 0, size, size);
    glDisable(GL_CULL_FACE);

    // Bind the working framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Loop through and render each cubemap face
    for (int i = 0; i < 6; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemapId, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
        r3d_shader_set_mat4(prepare.cubemapFromEquirectangular, uMatView, R3D.misc.matCubeViews[i]);
        r3d_primitive_draw_cube();
    }

    // Unbind texture and disable the shader
    r3d_shader_unbind_sampler2D(prepare.cubemapFromEquirectangular, uTexEquirectangular);
    r3d_shader_disable();

    // Cleanup the working framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    // Reset viewport and re-enable culling
    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_CULL_FACE);

    // Generate mipmaps and set texture parameters
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapId);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    // Return cubemap texture
    TextureCubemap cubemap = {
        .id = cubemapId,
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = panorama.format
    };

    // Download the temporary panorama
    UnloadTexture(panorama);

    return cubemap;
}

static TextureCubemap r3d_skybox_generate_irradiance(TextureCubemap sky)
{
    // Compute irradiance resolution
    int size = sky.width / 16;
    if (size < 32) size = 32;

    // Choose the best HDR format available
    GLenum format = r3d_support_get_internal_format(GL_RGB16F, true);

    // Create the irradiance cubemap texture
    unsigned int irradianceId = 0;
    glGenTextures(1, &irradianceId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceId);
    for (int i = 0; i < 6; i++) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format,
            size, size, 0, GL_RGB, GL_FLOAT, NULL
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create and configure the working framebuffer
    unsigned int fbo = rlLoadFramebuffer();

    // Calculate projection matrix
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    // Enable and configure irradiance convolution shader
    r3d_shader_enable(prepare.cubemapIrradiance);
    r3d_shader_set_mat4(prepare.cubemapIrradiance, uMatProj, matProj);
    r3d_shader_bind_samplerCube(prepare.cubemapIrradiance, uCubemap, sky.id);

    // Set viewport to framebuffer dimensions
    glViewport(0, 0, size, size);
    glDisable(GL_CULL_FACE);

    // Bind the working framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Render irradiance to cubemap faces
    for (int i = 0; i < 6; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceId, 0);
        glClear(GL_DEPTH_BUFFER_BIT);
        r3d_shader_set_mat4(prepare.cubemapIrradiance, uMatView, R3D.misc.matCubeViews[i]);
        r3d_primitive_draw_cube();
    }

    // Unbind texture and disable the shader
    r3d_shader_unbind_samplerCube(prepare.cubemapIrradiance, uCubemap);
    r3d_shader_disable();

    // Cleanup the working framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    // Reset viewport and re-enable culling
    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_CULL_FACE);

    // Return irradiance cubemap
    TextureCubemap irradiance = {
        .id = irradianceId,
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32
    };

    return irradiance;
}

static TextureCubemap r3d_skybox_generate_prefilter(TextureCubemap sky)
{
    static const int PREFILTER_SIZE = 128;
    static const int MAX_MIP_LEVELS = 8;    //< 1 + (int)floor(log2(PREFILTER_SIZE))

    // Choose the best HDR format available
    GLenum format = r3d_support_get_internal_format(GL_RGB16F, true);

    // Create the prefilter cubemap texture
    unsigned int prefilterId = 0;
    glGenTextures(1, &prefilterId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterId);
    for (int face = 0; face < 6; face++) {
        for (int level = 0; level < MAX_MIP_LEVELS; level++) {
            int size = PREFILTER_SIZE >> level;
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, format,
                size, size, 0, GL_RGB, GL_FLOAT, NULL
            );
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create a working framebuffer
    // It will be configured for each mipmap
    unsigned int fbo = rlLoadFramebuffer();

    // Calculate projection matrix
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    // Enable shader for prefiltering
    r3d_shader_enable(prepare.cubemapPrefilter);
    r3d_shader_set_mat4(prepare.cubemapPrefilter, uMatProj, matProj);
    r3d_shader_set_float(prepare.cubemapPrefilter, uResolution, (float)sky.width);
    r3d_shader_bind_samplerCube(prepare.cubemapPrefilter, uCubemap, sky.id);

    // Configure framebuffer and rendering parameters
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDisable(GL_CULL_FACE);

    // Process each mipmap level
    for (int mip = 0; mip < MAX_MIP_LEVELS; mip++)
    {
        int mipWidth = (int)(PREFILTER_SIZE * powf(0.5, (float)mip));
        int mipHeight = (int)(PREFILTER_SIZE * powf(0.5, (float)mip));

        glViewport(0, 0, mipWidth, mipHeight);
        float roughness = (float)mip / (float)(MAX_MIP_LEVELS - 1);
        r3d_shader_set_float(prepare.cubemapPrefilter, uRoughness, roughness);

        // Render all faces of the cubemap
        for (int i = 0; i < 6; i++) {
            r3d_shader_set_mat4(prepare.cubemapPrefilter, uMatView, R3D.misc.matCubeViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterId, mip);
            glClear(GL_DEPTH_BUFFER_BIT);
            r3d_primitive_draw_cube();
        }
    }

    // Unbind texture and disable the shader
    r3d_shader_unbind_samplerCube(prepare.cubemapPrefilter, uCubemap);
    r3d_shader_disable();

    // Cleanup the working framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    
    // Reset viewport and re-enable culling
    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_CULL_FACE);

    // Return prefiltered cubemap
    TextureCubemap prefilter = {
        .id = prefilterId,
        .width = PREFILTER_SIZE,
        .height = PREFILTER_SIZE,
        .mipmaps = MAX_MIP_LEVELS,
        .format = RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16
    };

    return prefilter;
}

// ========================================
// PUBLIC API
// ========================================

R3D_Skybox R3D_LoadSkybox(const char* filePath, CubemapLayout layout)
{
    Image image = LoadImage(filePath);
    R3D_Skybox skybox = R3D_LoadSkyboxFromMemory(image, layout);
    UnloadImage(image);
    return skybox;
}

R3D_Skybox R3D_LoadSkyboxFromMemory(Image image, CubemapLayout layout)
{
    R3D_Skybox skybox = { 0 };
    skybox.cubemap = r3d_skybox_load_cubemap_from_layout(&image, layout);
    skybox.irradiance = r3d_skybox_generate_irradiance(skybox.cubemap);
    skybox.prefilter = r3d_skybox_generate_prefilter(skybox.cubemap);
    return skybox;
}

R3D_Skybox R3D_LoadSkyboxPanorama(const char* filePath, int size)
{
    Image image = LoadImage(filePath);
    R3D_Skybox skybox = R3D_LoadSkyboxPanoramaFromMemory(image, size);
    UnloadImage(image);
    return skybox;
}

R3D_Skybox R3D_LoadSkyboxPanoramaFromMemory(Image image, int size)
{
    R3D_Skybox skybox = { 0 };
    skybox.cubemap = r3d_skybox_load_cubemap_from_panorama(image, size);
    skybox.irradiance = r3d_skybox_generate_irradiance(skybox.cubemap);
    skybox.prefilter = r3d_skybox_generate_prefilter(skybox.cubemap);
    return skybox;
}

void R3D_UnloadSkybox(R3D_Skybox sky)
{
    UnloadTexture(sky.cubemap);
    UnloadTexture(sky.irradiance);
    UnloadTexture(sky.prefilter);
}
