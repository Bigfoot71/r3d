/* r3d_draw.h -- Internal R3D draw module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_DRAW_H
#define R3D_MODULE_DRAW_H

#include <r3d/r3d_animation.h>
#include <r3d/r3d_material.h>
#include <r3d/r3d_skeleton.h>
#include <r3d/r3d_mesh.h>

#include "../details/r3d_frustum.h"

// ========================================
// HELPER MACROS
// ========================================

/*
 * Check whether a draw call has valid instancing data.
 * Returns true if the draw call contains a non-null instance transform array
 * and a positive instance count.
 */
#define R3D_DRAW_HAS_INSTANCES(call) \
    (call->instanced.transforms && call->instanced.count > 0)

/*
 * Check whether there are any deferred draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
#define R3D_DRAW_HAS_DEFERRED                                   \
    (R3D_MOD_DRAW.list[R3D_DRAW_DEFERRED].numDrawCalls > 0 ||   \
    R3D_MOD_DRAW.list[R3D_DRAW_DEFERRED_INST].numDrawCalls > 0)

/*
 * Check whether there are any prepass draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
#define R3D_DRAW_HAS_PREPASS                                    \
    (R3D_MOD_DRAW.list[R3D_DRAW_PREPASS].numDrawCalls > 0 ||    \
    R3D_MOD_DRAW.list[R3D_DRAW_PREPASS_INST].numDrawCalls > 0)

/*
 * Check whether there are any forward draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
#define R3D_DRAW_HAS_FORWARD                                    \
    (R3D_MOD_DRAW.list[R3D_DRAW_FORWARD].numDrawCalls > 0 ||    \
    R3D_MOD_DRAW.list[R3D_DRAW_FORWARD_INST].numDrawCalls > 0)

/*
 * Check whether there are any decal draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
#define R3D_DRAW_HAS_DECAL                                      \
    (R3D_MOD_DRAW.list[R3D_DRAW_DECAL].numDrawCalls > 0 ||      \
    R3D_MOD_DRAW.list[R3D_DRAW_DECAL_INST].numDrawCalls > 0)

/*
 * Iterate over multiple draw lists in the order specified by the variadic arguments.
 * Provides a pointer to each r3d_draw_call_t in sequence.
 *
 * Intended for internal rendering passes only.
 */
#define R3D_DRAW_FOR_EACH(call, ...)                                            \
    for (int _lists[] = {__VA_ARGS__}, _list_idx = 0, _i = 0, _keep = 1;        \
         _list_idx < (int)(sizeof(_lists)/sizeof(_lists[0]));                   \
         (_i >= R3D_MOD_DRAW.list[_lists[_list_idx]].numDrawCalls ?             \
          (_list_idx++, _i = 0) : 0))                                           \
        for (; _list_idx < (int)(sizeof(_lists)/sizeof(_lists[0])) &&           \
               _i < R3D_MOD_DRAW.list[_lists[_list_idx]].numDrawCalls;          \
             _i++, _keep = 1)                                                   \
            for (const r3d_draw_call_t* call =                                  \
                 &R3D_MOD_DRAW.drawCalls[R3D_MOD_DRAW.list[_lists[_list_idx]].drawCalls[_i]]; \
                 _keep; _keep = 0)

// ========================================
// DRAW OP ENUMS
// ========================================

/*
 * Sorting modes applied to draw lists.
 * Used to control draw order for depth testing efficiency or visual correctness.
 */
typedef enum {
    R3D_DRAW_SORT_FRONT_TO_BACK,    //< Typically used for opaque geometry
    R3D_DRAW_SORT_BACK_TO_FRONT,    //< Typically used for transparent geometry
} r3d_draw_sort_enum_t;

// ========================================
// DRAW CALL STRUCTURE
// ========================================

/*
 * Internal representation of a single draw call.
 * Contains all data required to issue a draw, including geometry, material,
 * transform, and optional animation or instancing data.
 */
typedef struct {

    R3D_Mesh mesh;                      //< Mesh geometry and GPU buffers
    Matrix transform;                   //< World transform matrix
    R3D_Material material;              //< Material used for rendering
    R3D_Skeleton skeleton;              //< Skeleton containing the bind pose (if any)
    const R3D_AnimationPlayer* player;  //< Animation player (may be NULL)

    struct {
        const Matrix* transforms;       //< Per-instance model matrices
        const Color* colors;            //< Optional per-instance colors
        BoundingBox allAabb;            //< World-space AABB covering all instances
        int transStride;                //< Byte stride between instance transforms (0 = sizeof(Matrix))
        int colStride;                  //< Byte stride between instance colors (0 = sizeof(Color))
        int count;                      //< Number of instances
    } instanced;

} r3d_draw_call_t;

// ========================================
// MODULE STATE
// ========================================

/*
 * Enumeration of internal draw lists.
 * Lists are separated by rendering path and instancing mode.
 */
typedef enum {

    R3D_DRAW_DEFERRED,          //< Fully opaque
    R3D_DRAW_PREPASS,           //< Forward but with depth prepass
    R3D_DRAW_FORWARD,           //< Forward only, without prepass
    R3D_DRAW_DECAL,

    R3D_DRAW_DEFERRED_INST,
    R3D_DRAW_PREPASS_INST,
    R3D_DRAW_FORWARD_INST,
    R3D_DRAW_DECAL_INST,

    R3D_DRAW_LIST_COUNT

} r3d_draw_list_enum_t;

/*
 * A draw list stores indices into the global draw call array.
 */
typedef struct {
    int* drawCalls;     //< Indices of draw calls
    int numDrawCalls;   //< Number of active entries
} r3d_draw_list_t;

/*
 * Global internal state of the draw module.
 * Owns the draw call storage and per-pass draw lists.
 */
extern struct r3d_draw {
    r3d_draw_list_t list[R3D_DRAW_LIST_COUNT];
    r3d_draw_call_t* drawCalls;
    int capDrawCalls;
    int numDrawCalls;
} R3D_MOD_DRAW;

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_draw_init(void);

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_draw_quit(void);

/*
 * Clear all draw lists and reset the draw call buffer for the next frame.
 */
void r3d_draw_clear(void);

/*
 * Push a new draw call to the right draw call list.
 * The draw call data is copied internally.
 */
void r3d_draw_push(const r3d_draw_call_t* call, bool decal);

/*
 * Indicates if the draw call is visible within the given frustum.
 * Useful for shadow maps, but for final scene culling, use `r3d_draw_cull_list`,
 * which generates a complete list of visible draw calls. Can only be called once.
 */
bool r3d_draw_is_visible(const r3d_draw_call_t* call, const r3d_frustum_t* frustum);

/*
 * Performs frustum culling on a draw list.
 * Invisible draw calls are removed, leaving only those visible within the frustum.
 * Should be called once per list/frame for final scene rendering.
 */
void r3d_draw_cull_list(r3d_draw_list_enum_t list, const r3d_frustum_t* frustum);

/*
 * Sort a draw list according to the given mode and camera position.
 */
void r3d_draw_sort_list(r3d_draw_list_enum_t list, Vector3 viewPosition, r3d_draw_sort_enum_t mode);

/*
 * Apply the OpenGL face culling state for a draw call.
 */
void r3d_draw_apply_cull_mode(R3D_CullMode mode);

/*
 * Applies the OpenGL blend function for the current draw call.
 * Assumes GL_BLEND is already enabled and only sets the blend equations.
 * The MIX blend mode always uses standard alpha blending.
 */
void r3d_draw_apply_blend_mode(R3D_BlendMode blend, R3D_TransparencyMode transparency);

/*
 * Configure face culling for shadow rendering depending on shadow casting mode.
 */
void r3d_draw_apply_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode);

/*
 * Issue a non-instanced draw call.
 */
void r3d_draw(const r3d_draw_call_t* call);

/*
 * Issue an instanced draw call.
 * Instance data is uploaded and bound internally.
 */
void r3d_draw_instanced(const r3d_draw_call_t* call, int locInstanceModel, int locInstanceColor);

#endif // R3D_MODULE_DRAW_H
