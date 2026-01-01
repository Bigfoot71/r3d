/* r3d_draw.c -- Internal R3D draw module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_draw.h"
#include <raymath.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <rlgl.h>
#include <glad.h>

#include "../details/r3d_frustum.h"
#include "../details/r3d_math.h"

// ========================================
// MODULE STATE
// ========================================

struct r3d_draw R3D_MOD_DRAW;

// ========================================
// HELPER MACROS
// ========================================

#define IS_CALL_PREPASS(call)                                           \
    ((call)->material.transparencyMode == R3D_TRANSPARENCY_PREPASS)

#define IS_CALL_FORWARD(call)                                           \
    ((call)->material.transparencyMode == R3D_TRANSPARENCY_ALPHA ||     \
     (call)->material.blendMode != R3D_BLEND_MIX)

// ========================================
// INTERNAL ARRAY FUNCTIONS
// ========================================

static inline size_t get_draw_call_index(const r3d_draw_call_t* call)
{
    assert(call >= R3D_MOD_DRAW.calls);
    return (size_t)(call - R3D_MOD_DRAW.calls);
}

static inline int get_last_group_index(void)
{
    int groupIndex = R3D_MOD_DRAW.numGroups - 1;
    assert(groupIndex >= 0);
    return groupIndex;
}

static inline r3d_draw_group_t* get_last_group(void)
{
    int groupIndex = get_last_group_index();
    return &R3D_MOD_DRAW.groups[groupIndex];
}

static bool growth_arrays(void)
{
    #define GROW_AND_ASSIGN(field) do { \
        void* _p = RL_REALLOC(R3D_MOD_DRAW.field, newCapacity * sizeof(*R3D_MOD_DRAW.field)); \
        if (_p == NULL) return false; \
        R3D_MOD_DRAW.field = _p; \
    } while (0)

    int newCapacity = 2 * R3D_MOD_DRAW.capacity;

    GROW_AND_ASSIGN(calls);
    GROW_AND_ASSIGN(groups);
    GROW_AND_ASSIGN(cacheDists);
    GROW_AND_ASSIGN(callIndices);
    GROW_AND_ASSIGN(groupIndices);
    GROW_AND_ASSIGN(visibleGroups);

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; ++i) {
        GROW_AND_ASSIGN(list[i].calls);
    }

    #undef GROW_AND_ASSIGN

    R3D_MOD_DRAW.capacity = newCapacity;

    return true;
}

// ========================================
// INTERNAL SORTING FUNCTIONS
// ========================================

static Vector3 G_sortViewPosition = {0};

static float calculate_max_distance_to_camera(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const Vector3* min = &call->mesh.aabb.min;
    const Vector3* max = &call->mesh.aabb.max;

    float maxDistSq = 0.0f;

    for (int i = 0; i < 8; ++i) {
        Vector3 corner = {
            (i & 1) ? max->x : min->x,
            (i & 2) ? max->y : min->y,
            (i & 4) ? max->z : min->z
        };

        corner = Vector3Transform(corner, group->transform);
        float distSq = Vector3DistanceSqr(G_sortViewPosition, corner);
        maxDistSq = (distSq > maxDistSq) ? distSq : maxDistSq;
    }

    return maxDistSq;
}

static float calculate_center_distance_to_camera(const r3d_draw_call_t* call)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    Vector3 center = {
        (call->mesh.aabb.min.x + call->mesh.aabb.max.x) * 0.5f,
        (call->mesh.aabb.min.y + call->mesh.aabb.max.y) * 0.5f,
        (call->mesh.aabb.min.z + call->mesh.aabb.max.z) * 0.5f
    };
    center = Vector3Transform(center, group->transform);

    return Vector3DistanceSqr(G_sortViewPosition, center);
}

static int compare_back_to_front(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;
    
    float distA = R3D_MOD_DRAW.cacheDists[indexA];
    float distB = R3D_MOD_DRAW.cacheDists[indexB];

    return (distA < distB) - (distA > distB);
}

static int compare_front_to_back(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;
    
    float distA = R3D_MOD_DRAW.cacheDists[indexA];
    float distB = R3D_MOD_DRAW.cacheDists[indexB];

    return (distA > distB) - (distA < distB);
}

// ========================================
// INTERNAL DRAW FUNCTIONS
// ========================================

static inline const BoundingBox* get_group_aabb(const r3d_draw_group_t* group)
{
    return r3d_draw_has_instances(group) ? &group->instanced.allAabb : &group->aabb;
}

static GLenum get_opengl_primitive(R3D_PrimitiveType primitive)
{
    switch (primitive) {
    case R3D_PRIMITIVE_POINTS:          return GL_POINTS;
    case R3D_PRIMITIVE_LINES:           return GL_LINES;
    case R3D_PRIMITIVE_LINE_STRIP:      return GL_LINE_STRIP;
    case R3D_PRIMITIVE_LINE_LOOP:       return GL_LINE_LOOP;
    case R3D_PRIMITIVE_TRIANGLES:       return GL_TRIANGLES;
    case R3D_PRIMITIVE_TRIANGLE_STRIP:  return GL_TRIANGLE_STRIP;
    case R3D_PRIMITIVE_TRIANGLE_FAN:    return GL_TRIANGLE_FAN;
    default: break;
    }

    return GL_TRIANGLES; // consider an error...
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_draw_init(void)
{
    const int DRAW_RESERVE_COUNT = 1024;

    #define ALLOC_AND_ASSIGN(field, logfmt, ...)  do { \
        void* _p = RL_MALLOC(DRAW_RESERVE_COUNT * sizeof(*R3D_MOD_DRAW.field)); \
        if (_p == NULL) { \
            TraceLog(LOG_FATAL, "R3D: Failed to init draw module; " logfmt, ##__VA_ARGS__); \
            goto fail; \
        } \
        R3D_MOD_DRAW.field = _p; \
    } while (0)

    memset(&R3D_MOD_DRAW, 0, sizeof(R3D_MOD_DRAW));

    ALLOC_AND_ASSIGN(calls, "Draw call array allocation failed");
    ALLOC_AND_ASSIGN(groups, "Draw group array allocation failed");
    ALLOC_AND_ASSIGN(callIndices, "Draw call indices array allocation failed");
    ALLOC_AND_ASSIGN(groupIndices, "Draw group indices array allocation failed");
    ALLOC_AND_ASSIGN(visibleGroups, "Visible group array allocation failed");
    ALLOC_AND_ASSIGN(cacheDists, "Distance cache array allocation failed");

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        ALLOC_AND_ASSIGN(list[i].calls, "Draw call array %i allocation failed", i);
    }

    #undef ALLOC_AND_ASSIGN

    R3D_MOD_DRAW.capacity = DRAW_RESERVE_COUNT;

    return true;

fail:
    r3d_draw_quit();
    return false;
}

void r3d_draw_quit(void)
{
    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        RL_FREE(R3D_MOD_DRAW.list[i].calls);
    }
    RL_FREE(R3D_MOD_DRAW.visibleGroups);
    RL_FREE(R3D_MOD_DRAW.groupIndices);
    RL_FREE(R3D_MOD_DRAW.callIndices);
    RL_FREE(R3D_MOD_DRAW.cacheDists);
    RL_FREE(R3D_MOD_DRAW.groups);
    RL_FREE(R3D_MOD_DRAW.calls);
}

void r3d_draw_clear(void)
{
    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        R3D_MOD_DRAW.list[i].numCalls = 0;
    }
    R3D_MOD_DRAW.numGroups = 0;
    R3D_MOD_DRAW.numCalls = 0;
}

void r3d_draw_group_push(const r3d_draw_group_t* group)
{
    if (R3D_MOD_DRAW.numGroups >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            TraceLog(LOG_FATAL, "R3D: Bad alloc on draw group push");
            return;
        }
    }

    int groupIndex = R3D_MOD_DRAW.numGroups++;
    R3D_MOD_DRAW.callIndices[groupIndex] = (r3d_draw_indices_t) {0};
    R3D_MOD_DRAW.groups[groupIndex] = *group;
}

void r3d_draw_call_push(const r3d_draw_call_t* call, bool decal)
{
    if (R3D_MOD_DRAW.numCalls >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            TraceLog(LOG_FATAL, "R3D: Bad alloc on draw call push");
            return;
        }
    }

    // Get group and their call indices
    int groupIndex = get_last_group_index();
    r3d_draw_group_t* group = &R3D_MOD_DRAW.groups[groupIndex];
    r3d_draw_indices_t* indices = &R3D_MOD_DRAW.callIndices[groupIndex];

    // Get call index and set call group indices
    int callIndex = R3D_MOD_DRAW.numCalls++;
    if (indices->numCall == 0) {
        indices->firstCall = callIndex;
    }
    ++indices->numCall;

    // Set group index for this draw call
    R3D_MOD_DRAW.groupIndices[callIndex] = groupIndex;

    // Determine the draw call list
    r3d_draw_list_enum_t list = R3D_DRAW_DEFERRED;
    if (decal) list = R3D_DRAW_DECAL;
    else if (IS_CALL_PREPASS(call)) list = R3D_DRAW_PREPASS;
    else if (IS_CALL_FORWARD(call)) list = R3D_DRAW_FORWARD;
    if (r3d_draw_has_instances(group)) list += 4;

    // Push the draw call and its index to the list
    R3D_MOD_DRAW.calls[callIndex] = *call;
    int listIndex = R3D_MOD_DRAW.list[list].numCalls++;
    R3D_MOD_DRAW.list[list].calls[listIndex] = callIndex;
}

r3d_draw_group_t* r3d_draw_get_call_group(const r3d_draw_call_t* call)
{
    int callIndex = get_draw_call_index(call);
    int groupIndex = R3D_MOD_DRAW.groupIndices[callIndex];
    r3d_draw_group_t* group = &R3D_MOD_DRAW.groups[groupIndex];

    return group;
}

void r3d_draw_compute_visible_groups(const r3d_frustum_t* frustum)
{
    for (int i = 0; i < R3D_MOD_DRAW.numGroups; i++)
    {
        const r3d_draw_group_t* group = &R3D_MOD_DRAW.groups[i];
        const BoundingBox* aabb = get_group_aabb(group);

        if (memcmp(aabb, &(BoundingBox){0}, sizeof(BoundingBox)) == 0) {
            R3D_MOD_DRAW.visibleGroups[i] = true;
        }
        else if (r3d_matrix_is_identity(&group->transform)) {
            R3D_MOD_DRAW.visibleGroups[i] = r3d_frustum_is_aabb_in(frustum, aabb);
        }
        else {
            R3D_MOD_DRAW.visibleGroups[i] = r3d_frustum_is_obb_in(frustum, aabb, &group->transform);
        }
    }
}

bool r3d_draw_call_is_visible(const r3d_draw_call_t* call, const r3d_frustum_t* frustum)
{
    int callIndex = get_draw_call_index(call);
    int groupIndex = R3D_MOD_DRAW.groupIndices[callIndex];
    if (!R3D_MOD_DRAW.visibleGroups[groupIndex]) return false;

    // If the number of calls to this group is 1, then the object has already been tested
    if (R3D_MOD_DRAW.callIndices[groupIndex].numCall == 1) {
        return true;
    }

    const r3d_draw_group_t* group = &R3D_MOD_DRAW.groups[groupIndex];
    const BoundingBox* aabb = get_group_aabb(group);

    // If the AABB is 'zero', the object is considered visible
    if (memcmp(aabb, &(BoundingBox){0}, sizeof(BoundingBox)) == 0) {
        return true;
    }

    if (r3d_matrix_is_identity(&group->transform)) {
        return r3d_frustum_is_aabb_in(frustum, aabb);
    }

    return r3d_frustum_is_obb_in(frustum, aabb, &group->transform);
}

void r3d_draw_sort_list(r3d_draw_list_enum_t list, Vector3 viewPosition, r3d_draw_sort_enum_t mode)
{
    G_sortViewPosition = viewPosition;

    r3d_draw_list_t* drawList = &R3D_MOD_DRAW.list[list];

    if (mode == R3D_DRAW_SORT_BACK_TO_FRONT) {
        for (int i = 0; i < drawList->numCalls; i++) {
            int callIndex = drawList->calls[i];
            const r3d_draw_call_t* call = &R3D_MOD_DRAW.calls[callIndex];
            R3D_MOD_DRAW.cacheDists[callIndex] = calculate_max_distance_to_camera(call);
        }
    }
    else {
        for (int i = 0; i < drawList->numCalls; i++) {
            int callIndex = drawList->calls[i];
            const r3d_draw_call_t* call = &R3D_MOD_DRAW.calls[callIndex];
            R3D_MOD_DRAW.cacheDists[callIndex] = calculate_center_distance_to_camera(call);
        }
    }

    int (*compare_func)(const void *a, const void *b) = NULL;

    switch (mode) {
    case R3D_DRAW_SORT_FRONT_TO_BACK:
        compare_func = compare_front_to_back;
        break;
    case R3D_DRAW_SORT_BACK_TO_FRONT:
        compare_func = compare_back_to_front;
        break;
    default:
        assert(false);
        return;
    }

    qsort(
        drawList->calls,
        drawList->numCalls,
        sizeof(*drawList->calls),
        compare_func
    );
}

void r3d_draw_apply_cull_mode(R3D_CullMode mode)
{
    switch (mode) {
    case R3D_CULL_NONE:
        glDisable(GL_CULL_FACE);
        break;
    case R3D_CULL_BACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_CULL_FRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    }
}

void r3d_draw_apply_blend_mode(R3D_BlendMode blend, R3D_TransparencyMode transparency)
{
    switch (blend) {
    case R3D_BLEND_MIX:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    case R3D_BLEND_ADDITIVE:
        if (transparency == R3D_TRANSPARENCY_DISABLED) {
            glBlendFunc(GL_ONE, GL_ONE);
        }
        else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }
        break;
    case R3D_BLEND_MULTIPLY:
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    case R3D_BLEND_PREMULTIPLIED_ALPHA:
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    default:
        break;
    }
}

void r3d_draw_apply_shadow_cast_mode(R3D_ShadowCastMode castMode, R3D_CullMode cullMode)
{
    switch (castMode) {
    case R3D_SHADOW_CAST_ON_AUTO:
    case R3D_SHADOW_CAST_ONLY_AUTO:
        r3d_draw_apply_cull_mode(cullMode);
        break;

    case R3D_SHADOW_CAST_ON_DOUBLE_SIDED:
    case R3D_SHADOW_CAST_ONLY_DOUBLE_SIDED:
        glDisable(GL_CULL_FACE);
        break;

    case R3D_SHADOW_CAST_ON_FRONT_SIDE:
    case R3D_SHADOW_CAST_ONLY_FRONT_SIDE:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;

    case R3D_SHADOW_CAST_ON_BACK_SIDE:
    case R3D_SHADOW_CAST_ONLY_BACK_SIDE:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;

    case R3D_SHADOW_CAST_DISABLED:
    default:
        assert("This shouldn't happen" && false);
        break;
    }
}

void r3d_draw(const r3d_draw_call_t* call)
{
    GLenum primitive = get_opengl_primitive(call->mesh.primitiveType);

    glBindVertexArray(call->mesh.vao);

    if (call->mesh.ebo == 0) {
        glDrawArrays(primitive, 0, call->mesh.vertexCount);
    }
    else {
        glDrawElements(primitive, call->mesh.indexCount, GL_UNSIGNED_INT, NULL);
    }

    glBindVertexArray(0);
}

void r3d_draw_instanced(const r3d_draw_call_t* call)
{
    GLenum primitive = get_opengl_primitive(call->mesh.primitiveType);

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_InstanceBuffer* instances = &group->instanced.buffer;

    glBindVertexArray(call->mesh.vao);

    if (instances->flags & R3D_INSTANCE_POSITION) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[0]);
        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), 0);
    }
    else {
        glDisableVertexAttribArray(10);
    }

    if (instances->flags & R3D_INSTANCE_ROTATION) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[1]);
        glEnableVertexAttribArray(11);
        glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(Quaternion), 0);
    }
    else {
        glDisableVertexAttribArray(11);
    }

    if (instances->flags & R3D_INSTANCE_SCALE) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[2]);
        glEnableVertexAttribArray(12);
        glVertexAttribPointer(12, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), 0);
    }
    else {
        glDisableVertexAttribArray(12);
    }

    if (instances->flags & R3D_INSTANCE_COLOR) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[3]);
        glEnableVertexAttribArray(13);
        glVertexAttribPointer(13, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Color), 0);
    }
    else {
        glDisableVertexAttribArray(13);
    }

    if (call->mesh.ebo == 0) {
        glDrawArraysInstanced(primitive, 0, call->mesh.vertexCount, group->instanced.count);
    }
    else {
        glDrawElementsInstanced(primitive, call->mesh.indexCount, GL_UNSIGNED_INT, NULL, group->instanced.count);
    }

    glBindVertexArray(0);
}
