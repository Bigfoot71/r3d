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

#include <r3d/r3d_environment.h>
#include <r3d/r3d_material.h>
#include <r3d/r3d_core.h>
#include <r3d/r3d_mesh.h>
#include <glad.h>

#include "./details/r3d_frustum.h"
#include "./details/r3d_primitives.h"
#include "./details/containers/r3d_array.h"
#include "./details/containers/r3d_registry.h"

/* === Defines === */

#define R3D_STORAGE_MATRIX_CAPACITY  256

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
        GLuint depth;               ///< D[24]
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
                             *   [_] = depth
                             */

        GLuint ssao;        /**< [0] = ssaoPpHs
                             */

        GLuint deferred;    /**< [0] = diffuse
                             *   [1] = specular
                             *   [_] = depth
                             */

        GLuint bloom;       /**< [0] = mipChainHs
                             */

        GLuint scene;       /**< [0] = scenePp
                              *  [1] = albedo
                              *  [2] = normal
                              *  [3] = orm
                              *  [_] = depth
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

        r3d_array_t aDrawDecals;            //< Contains all decal draw calls
        r3d_array_t aDrawDecalsInst;        //< Contains all instanced decal draw calls

        r3d_registry_t rLights;             //< Contains all created lights
        r3d_array_t aLightBatch;            //< Contains all lights visible on screen

    } container;

    // Environment data
    struct {

        Vector4 backgroundColor;        // Used as default albedo color when skybox is disabled (scene pass)

        Color ambientColor;             // Base color of ambient light used when skybox is disabled (light pass)
        float ambientEnergy;            // Multiplicative energy for ambient light when skybox is disabled (light pass)
        Vector3 ambientLight;           // Total ambient light contribution when skybox is disabled (light pass)
                                        
        Quaternion quatSky;             // Rotation of the skybox (scene / light passes)
        R3D_Skybox sky;                 // Skybox textures (scene / light passes)
        bool useSky;                    // Flag to indicate if skybox is enabled (light pass)
        float skyBackgroundIntensity;   // Intensity of the visible background from the skybox (scene / light passes) 
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
        // REVIEW: We could use internal primitive rendering (?)
        R3D_Mesh meshDecalBounds;   //< Unit cube used for decal projection
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

/* === Target loading functions === */

void r3d_target_load_mip_chain_hs(int width, int height, int count);
void r3d_target_unload_mip_chain_hs(void);

/* === Framebuffer loading functions === */

void r3d_framebuffer_load_gbuffer(int width, int height);
void r3d_framebuffer_load_ssao(int width, int height);
void r3d_framebuffer_load_deferred(int width, int height);
void r3d_framebuffer_load_bloom(int width, int height);
void r3d_framebuffer_load_scene(int width, int height);

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

/* === Primitive helper macros */

#define r3d_primitive_draw_quad()               \
{                                               \
    r3d_primitive_draw(&R3D.primitive.quad);    \
}

#define r3d_primitive_draw_cube()               \
{                                               \
    r3d_primitive_draw(&R3D.primitive.cube);    \
}

#define r3d_primitive_bind_and_draw_screen()    \
{                                               \
    glBindVertexArray(R3D.primitive.dummyVAO);  \
    glDrawArrays(GL_TRIANGLES, 0, 3);           \
    glBindVertexArray(0);                       \
}

#endif // R3D_STATE_H
