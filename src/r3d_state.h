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

/* === Global R3D State === */

extern struct R3D_State {

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

/* === Storage functions === */

void r3d_storage_bind_and_upload_matrices(const Matrix* matrices, int count, int slot);

/* === Main loading functions === */

void r3d_textures_load(void);
void r3d_textures_unload(void);

void r3d_storages_load(void);
void r3d_storages_unload(void);

/* === Texture loading functions === */

void r3d_texture_load_white(void);
void r3d_texture_load_black(void);
void r3d_texture_load_normal(void);
void r3d_texture_load_ssao_noise(void);
void r3d_texture_load_ssao_kernel(void);
void r3d_texture_load_ibl_brdf_lut(void);

/* === Storage loading functions === */

void r3d_storage_load_tex_matrices(void);

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
