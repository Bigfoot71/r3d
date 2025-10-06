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

#ifndef R3D_STATE_H
#define R3D_STATE_H

#include "r3d.h"
#include "glad.h"

#include "./details/r3d_shaders.h"
#include "./details/r3d_frustum.h"
#include "./details/r3d_primitives.h"
#include "./details/containers/r3d_array.h"
#include "./details/containers/r3d_registry.h"

/* === Defines === */

#define R3D_STORAGE_MATRIX_CAPACITY  256

#define R3D_STENCIL_GEOMETRY_BIT     0x80                               // Bit 7 (MSB) for geometry
#define R3D_STENCIL_GEOMETRY_MASK    0x80                               // Mask for geometry bit only
#define R3D_STENCIL_EFFECT_MASK      0x7F                               // Mask for effect bits (bits 0-6)
#define R3D_STENCIL_EFFECT_ID(n)     ((n) & R3D_STENCIL_EFFECT_MASK)    // Extract effect ID (7 bits - 127 effects)

/* === Internal Strucs === */

struct r3d_support_internal_format {
    bool internal, attachment;
};

/* === Global R3D State === */

extern struct R3D_State {

    // GPU Supports
    struct {

        // Single Channel Formats
        struct r3d_support_internal_format R8;               // 8-bit normalized red channel
        struct r3d_support_internal_format R16F;             // 16-bit half-precision floating point red channel
        struct r3d_support_internal_format R32F;             // 32-bit full-precision floating point red channel

        // Dual Channel Formats
        struct r3d_support_internal_format RG8;              // 8-bit normalized red-green channels
        struct r3d_support_internal_format RG16F;            // 16-bit half-precision floating point red-green channels
        struct r3d_support_internal_format RG32F;            // 32-bit full-precision floating point red-green channels

        // Triple Channel Formats (RGB)
        struct r3d_support_internal_format RGB565;           // 5-6-5 bits RGB (packed, legacy)
        struct r3d_support_internal_format RGB8;             // 8-bit normalized RGB channels
        struct r3d_support_internal_format SRGB8;            // 8-bit sRGB color space RGB channels
        struct r3d_support_internal_format RGB12;            // 12-bit normalized RGB channels
        struct r3d_support_internal_format RGB16;            // 16-bit normalized RGB channels
        struct r3d_support_internal_format RGB9_E5;          // RGB with shared 5-bit exponent (compact HDR format)
        struct r3d_support_internal_format R11F_G11F_B10F;   // 11-bit red, 11-bit green, 10-bit blue floating point (packed HDR)
        struct r3d_support_internal_format RGB16F;           // 16-bit half-precision floating point RGB channels
        struct r3d_support_internal_format RGB32F;           // 32-bit full-precision floating point RGB channels

        // Quad Channel Formats (RGBA)
        struct r3d_support_internal_format RGBA4;            // 4-4-4-4 bits RGBA (packed, legacy)
        struct r3d_support_internal_format RGB5_A1;          // 5-5-5-1 bits RGBA (packed, legacy)
        struct r3d_support_internal_format RGBA8;            // 8-bit normalized RGBA channels
        struct r3d_support_internal_format SRGB8_ALPHA8;     // 8-bit sRGB RGB + 8-bit linear alpha channel
        struct r3d_support_internal_format RGB10_A2;         // 10-bit RGB + 2-bit alpha (HDR color with minimal alpha)
        struct r3d_support_internal_format RGBA12;           // 12-bit normalized RGBA channels
        struct r3d_support_internal_format RGBA16;           // 16-bit normalized RGBA channels
        struct r3d_support_internal_format RGBA16F;          // 16-bit half-precision floating point RGBA channels
        struct r3d_support_internal_format RGBA32F;          // 32-bit full-precision floating point RGBA channels

    } support;

    // Targets
    struct {

        /**
         * @suffix: 'Pp' indicates that the target is used for ping-pong rendering, where [0] is the target and [1] is the source
         * @suffix: 'Hs' indicates that the target is half-sized
         */

        GLuint albedo;              ///< RGB[8|8|8]
        GLuint emission;            ///< RGB[11|11|10] (or fallbacks)
        GLuint normal;              ///< RG[16|16] (8-bit if R3D_FLAGS_8_BIT_NORMALS or 16F not supported)
        GLuint orm;                 ///< RGB[8|8|8]
        GLuint depthStencil;        ///< DS[24|8] -> Stencil: Last bit is a true/false geometry and others bits are for the rest
        GLuint ambient;             ///< RGB[16|16|16] (or R11G11B10 in low precision) (or fallbacks) -> Ambient contribution
        GLuint diffuse;             ///< RGB[16|16|16] (or R11G11B10 in low precision) (or fallbacks) -> Diffuse contribution
        GLuint specular;            ///< RGB[16|16|16] (or R11G11B10 in low precision) (or fallbacks) -> Specular contribution
        GLuint ssaoPpHs[2];         ///< R[8] -> Used for initial SSAO rendering + blur effect
        GLuint scenePp[2];          ///< RGB[16|16|16] (or R11G11B10 in low precision) (or fallbacks)

        struct r3d_mip_chain {
            struct r3d_mip {
                unsigned int id;    //< RGB[16|16|16] (or R11G11B10 in low precision) (or fallbacks)
                uint32_t w, h;      //< Dimensions
                float tx, ty;       //< Texel size
            } *chain;
            int count;
        } mipChainHs;

    } target;

    // Framebuffers
    struct {

        GLuint gBuffer;     /**< [0] = albedo
                             *   [1] = emission
                             *   [2] = normal
                             *   [3] = orm
                             *   [_] = depthStencil
                             */

        GLuint ssao;        /**< [0] = ssaoPpHs
                             *   [_] = depthStencil (use stencil)
                             */

        GLuint deferred;    /**< [0] = diffuse
                             *   [1] = specular
                             *   [2] = ambient
                             *   [_] = depthStencil
                             */

        GLuint bloom;       /**< [0] = mipChainHs
                             */

        GLuint scene;       /**< [0] = scenePp
                              *  [1] = albedo
                              *  [2] = normal
                              *  [3] = orm
                              *  [4] = ambient
                              *  [_] = depthStencil
                              */

        // Custom target (optional)
        RenderTexture customTarget;

    } framebuffer;

    // Containers
    struct {

        r3d_array_t aDrawDeferred;          //< Contains all deferred draw calls
        r3d_array_t aDrawDeferredInst;      //< Contains all deferred instanced draw calls

        r3d_array_t aDrawForward;           //< Contains all forward draw calls
        r3d_array_t aDrawForwardInst;       //< Contains all forward instanced draw calls

        r3d_registry_t rLights;             //< Contains all created lights
        r3d_array_t aLightBatch;            //< Contains all lights visible on screen

    } container;

    // Internal shaders
    struct {

        // Generation shaders
        struct {
            r3d_shader_generate_gaussian_blur_dual_pass_t gaussianBlurDualPass;
            r3d_shader_generate_downsampling_t downsampling;
            r3d_shader_generate_upsampling_t upsampling;
            r3d_shader_generate_cubemap_from_equirectangular_t cubemapFromEquirectangular;
            r3d_shader_generate_irradiance_convolution_t irradianceConvolution;
            r3d_shader_generate_prefilter_t prefilter;
        } generate;

        // Raster shaders
        struct {
            r3d_shader_raster_geometry_t geometry;
            r3d_shader_raster_forward_t forward;
            r3d_shader_raster_skybox_t skybox;
            r3d_shader_raster_depth_volume_t depthVolume;
            r3d_shader_raster_depth_t depth;
            r3d_shader_raster_depth_cube_t depthCube;
        } raster;

        // Screen shaders
        struct {
            r3d_shader_screen_ssao_t ssao;
            r3d_shader_screen_ssao_apply_t ssao_apply;
            r3d_shader_screen_ambient_ibl_t ambientIbl;
            r3d_shader_screen_ambient_t ambient;
            r3d_shader_screen_lighting_t lighting;
            r3d_shader_screen_scene_t scene;
            r3d_shader_screen_bloom_t bloom;
            r3d_shader_screen_ssr_t ssr;
            r3d_shader_screen_fog_t fog;
            r3d_shader_screen_output_t output[R3D_TONEMAP_COUNT];
            r3d_shader_screen_fxaa_t fxaa;
            r3d_shader_screen_dof_t dof;
        } screen;

    } shader;

    // Environment data
    struct {

        Vector3 backgroundColor;        // Used as default albedo color when skybox is disabled (raster pass)
        Vector3 ambientColor;           // Used as default ambient light when skybox is disabled (light pass)
                                        
        Quaternion quatSky;             // Rotation of the skybox (raster / light passes)
        R3D_Skybox sky;                 // Skybox textures (raster / light passes)
        bool useSky;                    // Flag to indicate if skybox is enabled (light pass)
        float skyBackgroundIntensity;   // Intensity of the visible background from the skybox (raster / light passes) 
        float skyAmbientIntensity;      // Intensity of the ambient light from the skybox (light pass)
        float skyReflectIntensity;      // Intensity of reflections from the skybox (light pass)
                                        
        bool ssaoEnabled;               // (pre-light pass)
        float ssaoRadius;               // (pre-light pass)
        float ssaoBias;                 // (pre-light pass)
        int ssaoIterations;             // (pre-light pass)
        float ssaoIntensity;            // (pre-light pass)
        float ssaoPower;                // (light pass)
        float ssaoLightAffect;          // (scene pass)
                                        
        R3D_Bloom bloomMode;            // (post pass)
        float bloomIntensity;           // (post pass)
        int bloomLevels;                // (gen pass)
        int bloomFilterRadius;          // (gen pass)
        float bloomThreshold;           // (gen pass)
        float bloomSoftThreshold;       // (gen pass)
        Vector4 bloomPrefilter;         // (gen pass)

        bool ssrEnabled;                // (post pass)
        int ssrMaxRaySteps;             // (post pass)
        int ssrBinarySearchSteps;       // (post pass)
        float ssrRayMarchLength;        // (post pass)
        float ssrDepthThickness;        // (post pass)
        float ssrDepthTolerance;        // (post pass)
        float ssrEdgeFadeStart;         // (post pass)
        float ssrEdgeFadeEnd;           // (post pass)
                                        
        R3D_Fog fogMode;                // (post pass)
        Vector3 fogColor;               // (post pass)
        float fogStart;                 // (post pass)
        float fogEnd;                   // (post pass)
        float fogDensity;               // (post pass)
        float fogSkyAffect;             // (post pass)
                                        
        R3D_Tonemap tonemapMode;        // (post pass)
        float tonemapExposure;          // (post pass)
        float tonemapWhite;             // (post pass)
                                        
        float brightness;               // (post pass)
        float contrast;                 // (post pass)
        float saturation;               // (post pass)

        R3D_Dof dofMode;                // (post pass)
        float dofFocusPoint;            // (post pass)
        float dofFocusScale;            // (post pass)
        float dofMaxBlurSize;           // (post pass)
        bool dofDebugMode;              // (post pass)

    } env;

    // Default textures
    struct {
        GLuint white;
        GLuint black;
        GLuint normal;
        GLuint ssaoNoise;
        GLuint ssaoKernel;
        GLuint iblBrdfLut;
    } texture;

    // Primitives
    struct {
        GLuint dummyVAO;        //< VAO with no buffers, used when the vertex shader takes care of geometry
        r3d_primitive_t quad;
        r3d_primitive_t cube;
    } primitive;

    // Storages
    struct {
        GLuint texMatrices[3];  // Stores 4x4 matrices for GPU skinning (triple-buffered to avoid GPU stalls)
    } storage;

    // State data
    struct {

        // Camera transformations
        struct {
            Matrix view, invView;
            Matrix proj, invProj;
            Matrix viewProj;
            Vector3 viewPos;
        } transform;

        // Frustum data
        struct {
            r3d_frustum_t shape;
            BoundingBox aabb;
        } frustum;

        // Scene data
        struct {
            BoundingBox bounds;
        } scene;

        // Resolution
        struct {
            int width;
            int height;
            int maxLevel;   //< Maximum mipmap level
            Vector2 texel;  //< Texel size
        } resolution;

        // Loading param
        struct {
            struct aiPropertyStore* aiProps;   //< Assimp import properties (scale, etc.)
            TextureFilter textureFilter;       //< Texture filter used by R3D during model loading
        } loading;

        // Active layers
        R3D_Layer layers;

        // Miscellaneous flags
        R3D_Flags flags;

    } state;

    // Misc data
    struct {
        Matrix matCubeViews[6];
    } misc;

} R3D;

/* === Helper functions === */

bool r3d_texture_is_default(GLuint id);
void r3d_calculate_bloom_prefilter_data(void);

/* === Support functions === */

GLenum r3d_support_get_internal_format(GLenum internalFormat, bool asAttachment);

/* === Storage functions === */

void r3d_storage_bind_and_upload_matrices(const Matrix* matrices, int count, int slot);

/* === Main loading functions === */

void r3d_supports_check(void);

void r3d_framebuffers_load(int width, int height);
void r3d_framebuffers_unload(void);

void r3d_textures_load(void);
void r3d_textures_unload(void);

void r3d_storages_load(void);
void r3d_storages_unload(void);

void r3d_shaders_load(void);
void r3d_shaders_unload(void);

/* === Target loading functions === */

void r3d_target_load_mip_chain_hs(int width, int height, int count);
void r3d_target_unload_mip_chain_hs(void);

/* === Framebuffer loading functions === */

void r3d_framebuffer_load_gbuffer(int width, int height);
void r3d_framebuffer_load_ssao(int width, int height);
void r3d_framebuffer_load_deferred(int width, int height);
void r3d_framebuffer_load_bloom(int width, int height);
void r3d_framebuffer_load_scene(int width, int height);

/* === Shader loading functions === */

void r3d_shader_load_generate_gaussian_blur_dual_pass(void);
void r3d_shader_load_generate_downsampling(void);
void r3d_shader_load_generate_upsampling(void);
void r3d_shader_load_generate_cubemap_from_equirectangular(void);
void r3d_shader_load_generate_irradiance_convolution(void);
void r3d_shader_load_generate_prefilter(void);
void r3d_shader_load_raster_geometry(void);
void r3d_shader_load_raster_forward(void);
void r3d_shader_load_raster_skybox(void);
void r3d_shader_load_raster_depth_volume(void);
void r3d_shader_load_raster_depth(void);
void r3d_shader_load_raster_depth_cube(void);
void r3d_shader_load_screen_ssao(void);
void r3d_shader_load_screen_ssao_apply(void);
void r3d_shader_load_screen_ambient_ibl(void);
void r3d_shader_load_screen_ambient(void);
void r3d_shader_load_screen_lighting(void);
void r3d_shader_load_screen_scene(void);
void r3d_shader_load_screen_bloom(void);
void r3d_shader_load_screen_ssr(void);
void r3d_shader_load_screen_fog(void);
void r3d_shader_load_screen_dof(void);
void r3d_shader_load_screen_output(R3D_Tonemap tonemap);
void r3d_shader_load_screen_fxaa(void);

/* === Texture loading functions === */

void r3d_texture_load_white(void);
void r3d_texture_load_black(void);
void r3d_texture_load_normal(void);
void r3d_texture_load_ssao_noise(void);
void r3d_texture_load_ssao_kernel(void);
void r3d_texture_load_ibl_brdf_lut(void);

/* === Storage loading functions === */

void r3d_storage_load_tex_matrices(void);

/* === Framebuffer helper macros === */

#define r3d_target_swap_pingpong(fb)            \
{                                               \
    unsigned int tmp = (fb)[0];                 \
    (fb)[0] = (fb)[1];                          \
    (fb)[1] = tmp;                              \
    glFramebufferTexture2D(                     \
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,   \
        GL_TEXTURE_2D, (fb)[0], 0               \
    );                                          \
}

/* === Shader helper macros === */

#define r3d_shader_enable(shader_name)                                                          \
do {                                                                                            \
    glUseProgram(R3D.shader.shader_name.id);                                                    \
} while(0)

#define r3d_shader_disable()                                                                    \
do {                                                                                            \
    glUseProgram(0);                                                                            \
} while(0)

#define r3d_shader_get_location(shader_name, uniform)                                           \
do {                                                                                            \
    R3D.shader.shader_name.uniform.loc = glGetUniformLocation(                                  \
        R3D.shader.shader_name.id, #uniform                                                     \
    );                                                                                          \
} while(0)

#define r3d_shader_set_sampler1D_slot(shader_name, uniform, value)                              \
do {                                                                                            \
    if (R3D.shader.shader_name.uniform.slot1D != (value)) {                                     \
        R3D.shader.shader_name.uniform.slot1D = (value);                                        \
        glUniform1i(                                                                            \
            R3D.shader.shader_name.uniform.loc,                                                 \
            R3D.shader.shader_name.uniform.slot1D                                               \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_sampler2D_slot(shader_name, uniform, value)                              \
do {                                                                                            \
    if (R3D.shader.shader_name.uniform.slot2D != (value)) {                                     \
        R3D.shader.shader_name.uniform.slot2D = (value);                                        \
        glUniform1i(                                                                            \
            R3D.shader.shader_name.uniform.loc,                                                 \
            R3D.shader.shader_name.uniform.slot2D                                               \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_samplerCube_slot(shader_name, uniform, value)                            \
do {                                                                                            \
    if (R3D.shader.shader_name.uniform.slotCube != (value)) {                                   \
        R3D.shader.shader_name.uniform.slotCube = (value);                                      \
        glUniform1i(                                                                            \
            R3D.shader.shader_name.uniform.loc,                                                 \
            R3D.shader.shader_name.uniform.slotCube                                             \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_bind_sampler1D(shader_name, uniform, texId)                                  \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slot1D);                       \
    glBindTexture(GL_TEXTURE_1D, (texId));                                                      \
} while(0)

#define r3d_shader_bind_sampler2D(shader_name, uniform, texId)                                  \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slot2D);                       \
    glBindTexture(GL_TEXTURE_2D, (texId));                                                      \
} while(0)

#define r3d_shader_bind_sampler2D_opt(shader_name, uniform, texId, altTex)                      \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slot2D);                       \
    if (texId != 0) glBindTexture(GL_TEXTURE_2D, (texId));                                      \
    else glBindTexture(GL_TEXTURE_2D, R3D.texture.altTex);                                      \
} while(0)

#define r3d_shader_bind_samplerCube(shader_name, uniform, texId)                                \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slotCube);                     \
    glBindTexture(GL_TEXTURE_CUBE_MAP, (texId));                                                \
} while(0)

#define r3d_shader_unbind_sampler1D(shader_name, uniform)                                       \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slot1D);                       \
    glBindTexture(GL_TEXTURE_1D, 0);                                                            \
} while(0)

#define r3d_shader_unbind_sampler2D(shader_name, uniform)                                       \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slot2D);                       \
    glBindTexture(GL_TEXTURE_2D, 0);                                                            \
} while(0)

#define r3d_shader_unbind_samplerCube(shader_name, uniform)                                     \
do {                                                                                            \
    glActiveTexture(GL_TEXTURE0 + R3D.shader.shader_name.uniform.slotCube);                     \
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);                                                      \
} while(0)

#define r3d_shader_set_int(shader_name, uniform, value)                                         \
do {                                                                                            \
    if (R3D.shader.shader_name.uniform.val != (value)) {                                        \
        R3D.shader.shader_name.uniform.val = (value);                                           \
        glUniform1i(                                                                            \
            R3D.shader.shader_name.uniform.loc,                                                 \
            R3D.shader.shader_name.uniform.val                                                  \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_float(shader_name, uniform, value)                                       \
do {                                                                                            \
    if (R3D.shader.shader_name.uniform.val != (value)) {                                        \
        R3D.shader.shader_name.uniform.val = (value);                                           \
        glUniform1f(                                                                            \
            R3D.shader.shader_name.uniform.loc,                                                 \
            R3D.shader.shader_name.uniform.val                                                  \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_vec2(shader_name, uniform, ...)                                          \
do {                                                                                            \
    const Vector2 tmp = (__VA_ARGS__);                                                          \
    if (!Vector2Equals(R3D.shader.shader_name.uniform.val, tmp)) {                              \
        R3D.shader.shader_name.uniform.val = tmp;                                               \
        glUniform2fv(                                                                           \
            R3D.shader.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D.shader.shader_name.uniform.val)                                    \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_vec3(shader_name, uniform, ...)                                          \
do {                                                                                            \
    const Vector3 tmp = (__VA_ARGS__);                                                          \
    if (!Vector3Equals(R3D.shader.shader_name.uniform.val, tmp)) {                              \
        R3D.shader.shader_name.uniform.val = tmp;                                               \
        glUniform3fv(                                                                           \
            R3D.shader.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D.shader.shader_name.uniform.val)                                    \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_vec4(shader_name, uniform, ...)                                          \
do {                                                                                            \
    const Vector4 tmp = (__VA_ARGS__);                                                          \
    if (!Vector4Equals(R3D.shader.shader_name.uniform.val, tmp)) {                              \
        R3D.shader.shader_name.uniform.val = tmp;                                               \
        glUniform4fv(                                                                           \
            R3D.shader.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D.shader.shader_name.uniform.val)                                    \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_col3(shader_name, uniform, value)                                        \
do {                                                                                            \
    const Vector3 v = {                                                                         \
        (value).r / 255.0f,                                                                     \
        (value).g / 255.0f,                                                                     \
        (value).b / 255.0f                                                                      \
    };                                                                                          \
    if (!Vector3Equals(R3D.shader.shader_name.uniform.val, v)) {                                \
        R3D.shader.shader_name.uniform.val = v;                                                 \
        glUniform3fv(                                                                           \
            R3D.shader.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D.shader.shader_name.uniform.val)                                    \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_col4(shader_name, uniform, value)                                        \
do {                                                                                            \
    const Vector4 v = {                                                                         \
        (value).r / 255.0f,                                                                     \
        (value).g / 255.0f,                                                                     \
        (value).b / 255.0f,                                                                     \
        (value).a / 255.0f                                                                      \
    };                                                                                          \
    if (!Vector4Equals(R3D.shader.shader_name.uniform.val, v)) {                                \
        R3D.shader.shader_name.uniform.val = v;                                                 \
        glUniform4fv(                                                                           \
            R3D.shader.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D.shader.shader_name.uniform.val)                                    \
        );                                                                                      \
    }                                                                                           \
} while(0)

#define r3d_shader_set_mat4(shader_name, uniform, value)                                        \
do {                                                                                            \
    glUniformMatrix4fv(R3D.shader.shader_name.uniform.loc, 1, GL_TRUE, (float*)(&(value)));     \
} while(0)

#define r3d_shader_set_mat4_v(shader_name, uniform, array, count)                               \
do {                                                                                            \
    glUniformMatrix4fv(R3D.shader.shader_name.uniform.loc, (count), GL_TRUE, (float*)(array));  \
} while(0)


/* === Primitive helper macros */

#define r3d_primitive_bind_and_draw_quad()                  \
{                                                           \
    r3d_primitive_bind_and_draw(&R3D.primitive.quad);       \
}

#define r3d_primitive_bind_and_draw_cube()                  \
{                                                           \
    r3d_primitive_bind_and_draw(&R3D.primitive.cube);       \
}

#define r3d_primitive_bind_and_draw_screen()                \
{                                                           \
    glBindVertexArray(R3D.primitive.dummyVAO);              \
    glDrawArrays(GL_TRIANGLES, 0, 3);                       \
    glBindVertexArray(0);                                   \
}

#endif // R3D_STATE_H
