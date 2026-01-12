/* r3d_target.h -- Internal R3D render target module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_TARGET_H
#define R3D_MODULE_TARGET_H

#include <raylib.h>
#include <glad.h>

// ========================================
// TARGET ENUM
// ========================================

/*
 * Enums for all internal render targets.
 * To add a new target, define a new enum before `R3D_TARGET_DEPTH`,
 * then add its creation parameters in `TARGET_CONFIG` in `r3d_target.c`.
 * Loading happens on-demand during bind operations.
 */
typedef enum {
    R3D_TARGET_INVALID = -1,
    R3D_TARGET_ALBEDO,          //< Full - Mip 1 - RGB[8|8|8]
    R3D_TARGET_NORMAL,          //< Full - Mip 1 - RGBA[16|16]
    R3D_TARGET_ORM,             //< Full - Mip 1 - RGB[8|8|8]
    R3D_TARGET_DIFFUSE,         //< Full - Mip 1 - RGB[16|16|16]
    R3D_TARGET_SPECULAR,        //< Full - Mip 1 - RGB[16|16|16]
    R3D_TARGET_GEOM_NORM_TAN,   //< Full - Mip 1 - RGBA[16|16|16|16]
    R3D_TARGET_SSAO_0,          //< Half - Mip 1 - R[8]
    R3D_TARGET_SSAO_1,          //< Half - Mip 1 - R[8]
    R3D_TARGET_SSIL_0,          //< Half - Mip 1 - RGBA[16|16|16|16]
    R3D_TARGET_SSIL_1,          //< Half - Mip 1 - RGBA[16|16|16|16]
    R3D_TARGET_SSIL_2,          //< Half - Mip 1 - RGBA[16|16|16|16]
    R3D_TARGET_SSR,             //< Half - Mip N - RGBA[16|16|16|16]
    R3D_TARGET_BLOOM,           //< Half - Mip N - RGB[16|16|16]
    R3D_TARGET_SCENE_0,         //< Full - Mip 1 - RGB[16|16|16]
    R3D_TARGET_SCENE_1,         //< Full - Mip 1 - RGB[16|16|16]
    R3D_TARGET_DEPTH,           //< Full - Mip 1 - D[24]
    R3D_TARGET_COUNT
} r3d_target_t;

// ========================================
// HELPER TARGET PACKS
// ========================================

#define R3D_TARGET_ALL_DEFERRED \
    R3D_TARGET_ALBEDO,          \
    R3D_TARGET_NORMAL,          \
    R3D_TARGET_ORM,             \
    R3D_TARGET_DIFFUSE,         \
    R3D_TARGET_SPECULAR,        \
    R3D_TARGET_GEOM_NORM_TAN,   \
    R3D_TARGET_DEPTH            \

#define R3D_TARGET_GBUFFER      \
    R3D_TARGET_ALBEDO,          \
    R3D_TARGET_DIFFUSE,         \
    R3D_TARGET_NORMAL,          \
    R3D_TARGET_ORM,             \
    R3D_TARGET_GEOM_NORM_TAN,   \
    R3D_TARGET_DEPTH            \

#define R3D_TARGET_LIGHTING     \
    R3D_TARGET_DIFFUSE,         \
    R3D_TARGET_SPECULAR,        \
    R3D_TARGET_DEPTH            \

#define R3D_TARGET_DECAL        \
    R3D_TARGET_ALBEDO,          \
    R3D_TARGET_DIFFUSE,         \
    R3D_TARGET_ORM,             \
    R3D_TARGET_NORMAL          \

// ========================================
// HELPER MACROS
// ========================================

#define R3D_TARGET_WIDTH        R3D_MOD_TARGET.resW
#define R3D_TARGET_HEIGHT       R3D_MOD_TARGET.resH
#define R3D_TARGET_RESOLUTION   R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH

#define R3D_TARGET_TEXEL_WIDTH  R3D_MOD_TARGET.txlW
#define R3D_TARGET_TEXEL_HEIGHT R3D_MOD_TARGET.txlH
#define R3D_TARGET_TEXEL_SIZE   R3D_MOD_TARGET.txlW, R3D_MOD_TARGET.txlH

#define R3D_TARGET_CLEAR(...)                                           \
    r3d_target_clear(                                                   \
        (r3d_target_t[]){ __VA_ARGS__ },                                \
        sizeof((r3d_target_t[]){ __VA_ARGS__ }) / sizeof(r3d_target_t)  \
    )

#define R3D_TARGET_BIND(...)                                            \
    r3d_target_bind(                                                    \
        (r3d_target_t[]){ __VA_ARGS__ },                                \
        sizeof((r3d_target_t[]){ __VA_ARGS__ }) / sizeof(r3d_target_t)  \
    )

/*
 * Binds the target, then swaps to the alternate SSAO target.
 * Modifies the target parameter to point to the other buffer.
 */
#define R3D_TARGET_BIND_AND_SWAP_SSAO(target) do {                      \
    R3D_TARGET_BIND(target);                                            \
    target = r3d_target_swap_ssao(target);                              \
} while(0)

/*
 * Binds the target, then swaps to the alternate scene target.
 * Modifies the target parameter to point to the other buffer.
 */
#define R3D_TARGET_BIND_AND_SWAP_SCENE(target) do {                     \
    R3D_TARGET_BIND(target);                                            \
    target = r3d_target_swap_scene(target);                             \
} while(0)

// ========================================
// FRAMEBUFFER STRUCTURE
// ========================================

#define R3D_TARGET_MAX_FRAMEBUFFERS 32
#define R3D_TARGET_MAX_ATTACHMENTS  8

typedef struct {
    GLuint id;
    r3d_target_t targets[R3D_TARGET_MAX_ATTACHMENTS];
    int count;
} r3d_target_fbo_t;

// ========================================
// MODULE STATE
// ========================================

extern struct r3d_target {

    r3d_target_fbo_t fbo[R3D_TARGET_MAX_FRAMEBUFFERS];  //< FBO combination cache. FBOs are automatically generated as needed during bind.
    int currentFbo;                                     //< Cache index of currently bound FBO, -1 if none bound. Reset via `r3d_target_reset()`.
    int fboCount;                                       //< Number of FBOs created

    bool targetLoaded[R3D_TARGET_COUNT];    //< Indicates whether the targets have been allocated.
    GLuint targets[R3D_TARGET_COUNT];       //< Table of targets (textures)

    uint32_t resW, resH;    //< Full internal resolution
    float txlW, txlH;       //< Size of a texel for full resolution

} R3D_MOD_TARGET;

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_target_init(int resW, int resH);

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_target_quit(void);

/*
 * Resizes the internal resolution.
 * Performs a reallocation of all targets already allocated.
 * Ignore the operation if the new resolution is identical to the one already defined.
 */
void r3d_target_resize(int resW, int resH);

/*
 * Returns the total number of mip levels of the internal buffers
 * based on their full resolution.
 */
int r3d_target_get_mip_count(r3d_target_t target);

/*
 * Returns the internal resolution for the specified mip level.
 */
void r3d_target_get_resolution(int* w, int* h, r3d_target_t target, int level);

/*
 * Returns the texel size for the specified mip level.
 */
void r3d_target_get_texel_size(float* w, float* h, r3d_target_t target, int level);

/*
 * Returns target '1' if target '0' is provided, otherwise returns target '0'.
 */
r3d_target_t r3d_target_swap_ssao(r3d_target_t ssao);

/*
 * Returns target '1' if target '0' is provided, otherwise returns target '0'.
 */
r3d_target_t r3d_target_swap_scene(r3d_target_t scene);

/*
 * Clears color targets with {0,0,0,1} and depth with 1.0.
 * Creates and binds the FBO with the requested combination.
 * Attachment locations correspond to the provided order.
 * Depth target must always be passed last in the list (verified by assert).
 */
void r3d_target_clear(const r3d_target_t* targets, int count);

/*
 * Creates and binds the FBO with the requested combination.
 * Attachment locations correspond to the provided order.
 * Depth target must always be passed last in the list (verified by assert).
 */
void r3d_target_bind(const r3d_target_t* targets, int count);

/*
 * Changes the mip level of the specified attachment.
 * Takes effect on the currently bound FBO.
 * The attachment index corresponds to the target's location in 'r3d_target_bind'.
 * Asserts that a valid FBO is currently bound.
 */
void r3d_target_set_mip_level(int attachment, int level);

/*
 * Generates mipmaps for the specified target.
 * Asserts that the target has already been created.
 */
void r3d_target_gen_mipmap(r3d_target_t target);

/*
 * Returns the texture ID corresponding to the requested target.
 * Asserts that the requested target has been created and if the target enum is valid.
 * If not created yet, it means we never bound this target, so it would be empty.
 */
GLuint r3d_target_get(r3d_target_t target);

/*
 * Returns the texture ID corresponding to the requested target.
 * Or returns 0 if the target has not been created or if the enum is invalid.
 */
GLuint r3d_target_get_or_null(r3d_target_t target);

/*
 * Blit les cibles fournies vers le FBO indiqué.
 * Supporte le blit de la cible de profondeur seulement.
 * La cible de profondeur doit toujours être spécifiée en dernier.
 */
void r3d_target_blit(r3d_target_t* targets, int count, GLuint dstFbo, int dstX, int dstY, int dstW, int dstH, bool linear);

/*
 * Reset the internal state cache as the FBO target currently binds.
 */
void r3d_target_reset(void);

#endif // R3D_MODULE_TARGET_H
