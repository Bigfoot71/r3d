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
#include <r3d/r3d_instance.h>
#include <r3d/r3d_material.h>
#include <r3d/r3d_skeleton.h>
#include <r3d/r3d_mesh.h>

#include "../common/r3d_frustum.h"

// ========================================
// HELPER MACROS
// ========================================

/*
 * Iterate over multiple draw lists in the order specified by the variadic arguments,
 * yielding a pointer to each r3d_draw_call_t.
 *
 * The optional 'cond' expression filters calls before culling (use `true` if unused).
 * The optional 'frustum' pointer enables frustum culling; pass NULL to disable it.
 *
 * Intended for internal rendering passes only.
 */
#define R3D_DRAW_FOR_EACH(call, cond, frustum, ...)                             \
    for (int _lists[] = {__VA_ARGS__}, _list_idx = 0, _i = 0, _keep = 1;        \
         _list_idx < (int)(sizeof(_lists)/sizeof(_lists[0]));                   \
         (_i >= R3D_MOD_DRAW.list[_lists[_list_idx]].numCalls ?                 \
          (_list_idx++, _i = 0) : 0))                                           \
        for (; _list_idx < (int)(sizeof(_lists)/sizeof(_lists[0])) &&           \
               _i < R3D_MOD_DRAW.list[_lists[_list_idx]].numCalls;              \
             _i++, _keep = 1)                                                   \
            for (const r3d_draw_call_t* call =                                  \
                 &R3D_MOD_DRAW.calls[R3D_MOD_DRAW.list[_lists[_list_idx]].calls[_i]]; \
                 _keep && (cond) && (!frustum || r3d_draw_call_is_visible(call, frustum)); \
                 _keep = 0)

// ========================================
// DRAW CALL ENUMS
// ========================================

/*
 * Visibility state for a group or cluster.
 * Used by culling passes to indicate whether drawing is required.
 */
typedef enum {
    R3D_DRAW_VISBILITY_UNKNOWN = -1,    //< Visibility has not been evaluated yet
    R3D_DRAW_VISBILITY_FALSE = 0,       //< Determined to be not visible (culled)
    R3D_DRAW_VISBILITY_TRUE = 1,        //< Determined to be visible
} r3d_draw_visibility_enum_t;

/*
 * Sorting modes applied to draw lists.
 * Used to control draw order for depth testing efficiency or visual correctness.
 */
typedef enum {
    R3D_DRAW_SORT_FRONT_TO_BACK,    //< Typically used for opaque geometry
    R3D_DRAW_SORT_BACK_TO_FRONT,    //< Typically used for transparent geometry
} r3d_draw_sort_enum_t;

// ========================================
// DRAW CALL STRUCTS
// ========================================

/*
 * Cluster that may contain multiple draw groups.
 * Stores bounds and the evaluated visibility state.
 */
typedef struct {
    BoundingBox aabb;
    r3d_draw_visibility_enum_t visible;
} r3d_draw_cluster_t;

/*
 * Visibility metadata for a draw group.
 * Holds its cluster index (if assigned) and its own visibility state.
 * Note: a group is effectively visible only when its cluster is visible.
 */
typedef struct {
    int clusterIndex;
    r3d_draw_visibility_enum_t visible;
} r3d_draw_group_visibility_t;

/*
 * Mapping structure linking draw groups to their associated draw calls.
 * Stores the range of draw calls belonging to a specific group.
 * One entry is stored per draw group.
 */
typedef struct {
    int firstCall;                      //< Index of the first draw call in this group
    int numCall;                        //< Number of draw calls in this group
} r3d_draw_indices_t;

/*
 * Draw group containing shared state for multiple draw calls.
 * All draw calls pushed after a group inherit its transform, skeleton, and instancing data.
 */
typedef struct {
    BoundingBox aabb;                   //< AABB of the model
    Matrix transform;                   //< World transform matrix
    uint32_t texPose;                   //< Texture that contains the bone matrices (can be 0 for non-skinned)
    R3D_InstanceBuffer instances;       //< Instance buffer to use
    int instanceCount;                  //< Number of instances
} r3d_draw_group_t;

/*
 * Internal representation of a single draw call.
 * Contains all data required to issue a draw, including geometry and material.
 * Transform and animation data are stored in the parent draw group.
 */
typedef struct {
    R3D_Material material;              //< Material used for rendering
    R3D_Mesh mesh;                      //< Mesh geometry and GPU buffers
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
    int* calls;     //< Indices of draw calls
    int numCalls;   //< Number of active entries
} r3d_draw_list_t;

/*
 * Global internal state of the draw module.
 * Owns the draw call storage and per-pass draw lists.
 */
extern struct r3d_draw {

    r3d_draw_cluster_t* clusters;                   //< Array of draw clusters
    int activeCluster;                              //< Index of the active cluster for new draw groups (-1 if no active clusters)

    r3d_draw_group_visibility_t* groupVisibility;   //< Array containing visibility info for each draw group (generated during group culling)
    r3d_draw_indices_t* callIndices;                //< Array of draw call index ranges for each draw group (automatically managed)
    r3d_draw_group_t* groups;                       //< Array of draw groups (shared data across draw calls)

    r3d_draw_list_t list[R3D_DRAW_LIST_COUNT];      //< Lists of draw call indices organized by rendering category
    r3d_draw_call_t* calls;                         //< Array of draw calls
    int* groupIndices;                              //< Array of group indices for each draw call (automatically managed)
    float* cacheDists;                              //< Array of distances between draw calls and the camera for sorting

    int numClusters;                                //< Number of active draw clusters
    int numGroups;                                  //< Number of active draw groups
    int numCalls;                                   //< Number of active draw calls
    int capacity;                                   //< Allocated capacity for all arrays (in number of elements)

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
 * Begins a new draw cluster with the given bounds.
 * All subsequent draw-group pushes will belong to this cluster.
 * Returns false if a cluster is already active or allocation fails.
 */
bool r3d_draw_cluster_begin(BoundingBox aabb);

/*
 * Ends the currently active draw cluster.
 * Returns false if no cluster is currently active.
 */
bool r3d_draw_cluster_end(void);

/*
 * Push a new draw group. All subsequent draw calls will belong to this group
 * until a new group is pushed.
 */
void r3d_draw_group_push(const r3d_draw_group_t* group);

/*
 * Push a new draw call to the appropriate draw list.
 * The draw call data is copied internally.
 * Inherits the group previously pushed.
 */
void r3d_draw_call_push(const r3d_draw_call_t* call, bool decal);

/*
 * Retrieve the draw group associated with a given draw call.
 * Returns a pointer to the parent group containing shared transform and instancing data.
 */
r3d_draw_group_t* r3d_draw_get_call_group(const r3d_draw_call_t* call);

/*
 * Builds the list of groups that are visible inside the given frustum.
 * Must be called before issuing visibility tests with the same frustum.
 */
void r3d_draw_compute_visible_groups(const r3d_frustum_t* frustum);

/*
 * Returns true if the draw call is visible within the given frustum.
 * Uses both per-call culling and the results produced by `r3d_draw_compute_visible_groups()`
 * Make sure to compute visible groups with the same frustum before calling this function.
 */
bool r3d_draw_call_is_visible(const r3d_draw_call_t* call, const r3d_frustum_t* frustum);

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
void r3d_draw_instanced(const r3d_draw_call_t* call);

// ----------------------------------------
// INLINE QUERIES
// ----------------------------------------

/*
 * Check whether a draw group has valid instancing data.
 * Returns true if the draw call contains a non-null instance transform array
 * and a positive instance count.
 */
static inline bool r3d_draw_has_instances(const r3d_draw_group_t* group)
{
    return (group->instances.capacity > 0) && (group->instanceCount > 0);
}

/*
 * Check whether there are any deferred draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
static inline bool r3d_draw_has_deferred(void)
{
    return
        (R3D_MOD_DRAW.list[R3D_DRAW_DEFERRED].numCalls > 0) ||
        (R3D_MOD_DRAW.list[R3D_DRAW_DEFERRED_INST].numCalls > 0);
}

/*
 * Check whether there are any prepass draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
static inline bool r3d_draw_has_prepass(void)
{
    return
        (R3D_MOD_DRAW.list[R3D_DRAW_PREPASS].numCalls > 0) ||
        (R3D_MOD_DRAW.list[R3D_DRAW_PREPASS_INST].numCalls > 0);
}

/*
 * Check whether there are any forward draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
static inline bool r3d_draw_has_forward(void)
{
    return
        (R3D_MOD_DRAW.list[R3D_DRAW_FORWARD].numCalls > 0) ||
        (R3D_MOD_DRAW.list[R3D_DRAW_FORWARD_INST].numCalls > 0);
}

/*
 * Check whether there are any decal draw calls queued for the current frame.
 * Includes both instanced and non-instanced variants.
 */
static inline bool r3d_draw_has_decal(void)
{
    return
        (R3D_MOD_DRAW.list[R3D_DRAW_DECAL].numCalls > 0) ||
        (R3D_MOD_DRAW.list[R3D_DRAW_DECAL_INST].numCalls > 0);
}

#endif // R3D_MODULE_DRAW_H
