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

#include "../details/r3d_math.h"

// TODO: Optimize cull with draw group pre-culling

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
    assert(call >= R3D_MOD_DRAW.drawCalls);
    return (size_t)(call - R3D_MOD_DRAW.drawCalls);
}

static inline int get_last_group_index(void)
{
    int groupIndex = R3D_MOD_DRAW.numDrawGroups - 1;
    assert(groupIndex >= 0);
    return groupIndex;
}

static inline r3d_draw_group_t* get_last_group(void)
{
    int groupIndex = get_last_group_index();
    return &R3D_MOD_DRAW.drawGroups[groupIndex];
}

static bool growth_arrays(void)
{
    int newCapacity = 2 * R3D_MOD_DRAW.capacity;

    void* newDrawCalls = RL_REALLOC(R3D_MOD_DRAW.drawCalls, newCapacity * sizeof(*R3D_MOD_DRAW.drawCalls));
    if (newDrawCalls == NULL) return false;
    R3D_MOD_DRAW.drawCalls = newDrawCalls;

    void* newDrawGroups = RL_REALLOC(R3D_MOD_DRAW.drawGroups, newCapacity * sizeof(*R3D_MOD_DRAW.drawGroups));
    if (newDrawGroups == NULL) return false;
    R3D_MOD_DRAW.drawGroups = newDrawGroups;

    void* newDrawIndices = RL_REALLOC(R3D_MOD_DRAW.drawIndices, newCapacity * sizeof(*R3D_MOD_DRAW.drawIndices));
    if (newDrawIndices == NULL) return false;
    R3D_MOD_DRAW.drawIndices = newDrawIndices;

    void* newDrawCallGroupIndices = RL_REALLOC(R3D_MOD_DRAW.drawCallGroupIndices, newCapacity * sizeof(*R3D_MOD_DRAW.drawCallGroupIndices));
    if (newDrawCallGroupIndices == NULL) return false;
    R3D_MOD_DRAW.drawCallGroupIndices = newDrawCallGroupIndices;

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        void* newDrawList = RL_REALLOC(R3D_MOD_DRAW.list[i].drawCalls, newCapacity * sizeof(*R3D_MOD_DRAW.list[i].drawCalls));
        if (newDrawList == NULL) return false;
        R3D_MOD_DRAW.list[i].drawCalls = newDrawList;
    }

    R3D_MOD_DRAW.capacity = newCapacity;

    return true;
}

// ========================================
// INTERNAL SORTING FUNCTIONS
// ========================================

static Vector3 G_sortViewPosition = {0};

static float calculate_max_distance_to_camera(const r3d_draw_call_t* drawCall)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(drawCall);

    Vector3 corners[8] = {
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.min.z},
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.max.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.min.y, drawCall->mesh.aabb.max.z},
        {drawCall->mesh.aabb.min.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.max.z},
        {drawCall->mesh.aabb.max.x, drawCall->mesh.aabb.max.y, drawCall->mesh.aabb.max.z}
    };

    float maxDistSq = 0.0f;
    for (int i = 0; i < 8; ++i) {
        Vector3 corner = Vector3Transform(corners[i], group->transform);
        float distSq = Vector3DistanceSqr(G_sortViewPosition, corner);
        if (distSq > maxDistSq) maxDistSq = distSq;
    }

    return maxDistSq;
}

static float calculate_center_distance_to_camera(const r3d_draw_call_t* drawCall)
{
    int callIndex = get_draw_call_index(drawCall);
    int groupIndex = R3D_MOD_DRAW.drawCallGroupIndices[callIndex];
    const r3d_draw_group_t* group = &R3D_MOD_DRAW.drawGroups[groupIndex];

    Vector3 center = {
        (drawCall->mesh.aabb.min.x + drawCall->mesh.aabb.max.x) * 0.5f,
        (drawCall->mesh.aabb.min.y + drawCall->mesh.aabb.max.y) * 0.5f,
        (drawCall->mesh.aabb.min.z + drawCall->mesh.aabb.max.z) * 0.5f
    };
    center = Vector3Transform(center, group->transform);

    return Vector3DistanceSqr(G_sortViewPosition, center);
}

static int compare_back_to_front(const void* a, const void* b)
{
    const r3d_draw_call_t* drawCallA = &R3D_MOD_DRAW.drawCalls[*(int*)(a)];
    const r3d_draw_call_t* drawCallB = &R3D_MOD_DRAW.drawCalls[*(int*)(b)];

    float distA = calculate_max_distance_to_camera(drawCallA);
    float distB = calculate_max_distance_to_camera(drawCallB);

    return (distA < distB) - (distA > distB);
}

static int compare_front_to_back(const void* a, const void* b)
{
    const r3d_draw_call_t* drawCallA = &R3D_MOD_DRAW.drawCalls[*(int*)(a)];
    const r3d_draw_call_t* drawCallB = &R3D_MOD_DRAW.drawCalls[*(int*)(b)];

    float distA = calculate_center_distance_to_camera(drawCallA);
    float distB = calculate_center_distance_to_camera(drawCallB);

    return (distA > distB) - (distA < distB);
}

// ========================================
// INTERNAL DRAW FUNCTIONS
// ========================================

GLenum get_opengl_primitive(R3D_PrimitiveType primitive)
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
    memset(&R3D_MOD_DRAW, 0, sizeof(R3D_MOD_DRAW));

    const int DRAW_RESERVE_COUNT = 1024;

    R3D_MOD_DRAW.drawCalls = RL_MALLOC(DRAW_RESERVE_COUNT * sizeof(*R3D_MOD_DRAW.drawCalls));
    if (R3D_MOD_DRAW.drawCalls == NULL) {
        TraceLog(LOG_FATAL, "R3D: Failed to init draw module; Draw call array allocation failed");
        return false;
    }

    R3D_MOD_DRAW.drawGroups = RL_MALLOC(DRAW_RESERVE_COUNT * sizeof(*R3D_MOD_DRAW.drawGroups));
    if (R3D_MOD_DRAW.drawGroups == NULL) {
        TraceLog(LOG_FATAL, "R3D: Failed to init draw module; Draw group array allocation failed");
        return false;
    }

    R3D_MOD_DRAW.drawIndices = RL_MALLOC(DRAW_RESERVE_COUNT * sizeof(*R3D_MOD_DRAW.drawIndices));
    if (R3D_MOD_DRAW.drawIndices == NULL) {
        TraceLog(LOG_FATAL, "R3D: Failed to init draw module; Draw call indices array allocation failed");
        return false;
    }

    R3D_MOD_DRAW.drawCallGroupIndices = RL_MALLOC(DRAW_RESERVE_COUNT * sizeof(*R3D_MOD_DRAW.drawCallGroupIndices));
    if (R3D_MOD_DRAW.drawCallGroupIndices == NULL) {
        TraceLog(LOG_FATAL, "R3D: Failed to init draw module; Draw group indices array allocation failed");
        return false;
    }

    R3D_MOD_DRAW.capacity = DRAW_RESERVE_COUNT;

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        R3D_MOD_DRAW.list[i].drawCalls = RL_MALLOC(DRAW_RESERVE_COUNT * sizeof(*R3D_MOD_DRAW.list[i].drawCalls));
        if (R3D_MOD_DRAW.list[i].drawCalls == NULL) {
            TraceLog(LOG_FATAL, "R3D: Failed to init draw module; Draw call array %i allocation failed", i);
            for (int j = 0; j <= i; j++) RL_FREE(R3D_MOD_DRAW.list[j].drawCalls);
            return false;
        }
    }

    return true;
}

void r3d_draw_quit(void)
{
    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        RL_FREE(R3D_MOD_DRAW.list[i].drawCalls);
    }
    RL_FREE(R3D_MOD_DRAW.drawCallGroupIndices);
    RL_FREE(R3D_MOD_DRAW.drawIndices);
    RL_FREE(R3D_MOD_DRAW.drawGroups);
    RL_FREE(R3D_MOD_DRAW.drawCalls);
}

void r3d_draw_clear(void)
{
    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        R3D_MOD_DRAW.list[i].numDrawCalls = 0;
    }
    R3D_MOD_DRAW.numDrawGroups = 0;
    R3D_MOD_DRAW.numDrawCalls = 0;
}

void r3d_draw_group_push(const r3d_draw_group_t* group)
{
    if (R3D_MOD_DRAW.numDrawGroups >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            TraceLog(LOG_FATAL, "R3D: Bad alloc on draw group push");
            return;
        }
    }

    int groupIndex = R3D_MOD_DRAW.numDrawGroups++;
    R3D_MOD_DRAW.drawIndices[groupIndex] = (r3d_draw_indices_t) {0};
    R3D_MOD_DRAW.drawGroups[groupIndex] = *group;
}

void r3d_draw_call_push(const r3d_draw_call_t* call, bool decal)
{
    if (R3D_MOD_DRAW.numDrawCalls >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            TraceLog(LOG_FATAL, "R3D: Bad alloc on draw call push");
            return;
        }
    }

    // Get group and their call indices
    int groupIndex = get_last_group_index();
    r3d_draw_group_t* group = &R3D_MOD_DRAW.drawGroups[groupIndex];
    r3d_draw_indices_t* indices = &R3D_MOD_DRAW.drawIndices[groupIndex];

    // Get call index and set call group indices
    int callIndex = R3D_MOD_DRAW.numDrawCalls++;
    if (indices->numCall == 0) {
        indices->firstCall = callIndex;
    }
    ++indices->numCall;

    // Set group index for this draw call
    R3D_MOD_DRAW.drawCallGroupIndices[callIndex] = groupIndex;

    // Determine the draw call list
    r3d_draw_list_enum_t list = R3D_DRAW_DEFERRED;
    if (decal) list = R3D_DRAW_DECAL;
    else if (IS_CALL_PREPASS(call)) list = R3D_DRAW_PREPASS;
    else if (IS_CALL_FORWARD(call)) list = R3D_DRAW_FORWARD;
    if (r3d_draw_has_instances(group)) list += 4;

    // Push the draw call and its index to the list
    R3D_MOD_DRAW.drawCalls[callIndex] = *call;
    int listIndex = R3D_MOD_DRAW.list[list].numDrawCalls++;
    R3D_MOD_DRAW.list[list].drawCalls[listIndex] = callIndex;
}

r3d_draw_group_t* r3d_draw_get_call_group(const r3d_draw_call_t* call)
{
    int callIndex = get_draw_call_index(call);
    int groupIndex = R3D_MOD_DRAW.drawCallGroupIndices[callIndex];
    r3d_draw_group_t* group = &R3D_MOD_DRAW.drawGroups[groupIndex];

    return group;
}

bool r3d_draw_call_is_visible(const r3d_draw_call_t* call, const r3d_frustum_t* frustum)
{
    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    const BoundingBox* aabb = r3d_draw_has_instances(group)
        ? &group->instanced.allAabb : &call->mesh.aabb;

    if (memcmp(aabb, &(BoundingBox){0}, sizeof(BoundingBox)) == 0) {
        return true;
    }

    if (r3d_matrix_is_identity(&group->transform)) {
        return r3d_frustum_is_aabb_in(frustum, aabb);
    }

    return r3d_frustum_is_obb_in(frustum, aabb, &group->transform);
}

void r3d_draw_cull_list(r3d_draw_list_enum_t list, const r3d_frustum_t* frustum)
{
    r3d_draw_list_t* drawList = &R3D_MOD_DRAW.list[list];

    for (int i = drawList->numDrawCalls - 1; i >= 0; i--) {
        if (!r3d_draw_call_is_visible(&R3D_MOD_DRAW.drawCalls[drawList->drawCalls[i]], frustum)) {
            drawList->drawCalls[i] = drawList->drawCalls[--drawList->numDrawCalls];
        }
    }
}

void r3d_draw_sort_list(r3d_draw_list_enum_t list, Vector3 viewPosition, r3d_draw_sort_enum_t mode)
{
    G_sortViewPosition = viewPosition;

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
        R3D_MOD_DRAW.list[list].drawCalls,
        R3D_MOD_DRAW.list[list].numDrawCalls,
        sizeof(*R3D_MOD_DRAW.list[list].drawCalls),
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
    if (call->mesh.ebo == 0) glDrawArrays(primitive, 0, call->mesh.vertexCount);
    else glDrawElements(primitive, call->mesh.indexCount, GL_UNSIGNED_INT, NULL);
    glBindVertexArray(0);
}

void r3d_draw_instanced(const r3d_draw_call_t* call, int locInstanceModel, int locInstanceColor)
{
    // NOTE: All this mess here will be reviewed with the instance buffers

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);

    glBindVertexArray(call->mesh.vao);

    unsigned int vboTransforms = 0;
    unsigned int vboColors = 0;

    // Enable the attribute for the transformation matrix (decomposed into 4 vec4 vectors)
    if (locInstanceModel >= 0 && group->instanced.transforms) {
        size_t stride = (group->instanced.transStride == 0) ? sizeof(Matrix) : group->instanced.transStride;

        // Create and bind VBO for transforms
        glGenBuffers(1, &vboTransforms);
        glBindBuffer(GL_ARRAY_BUFFER, vboTransforms);
        glBufferData(GL_ARRAY_BUFFER, group->instanced.count * stride, group->instanced.transforms, GL_DYNAMIC_DRAW);

        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(locInstanceModel + i);
            glVertexAttribPointer(locInstanceModel + i, 4, GL_FLOAT, GL_FALSE, (int)stride, (void*)(i * sizeof(Vector4)));
            glVertexAttribDivisor(locInstanceModel + i, 1);
        }
    }

    // Handle per-instance colors if available
    if (locInstanceColor >= 0 && group->instanced.colors) {
        size_t stride = (group->instanced.colStride == 0) ? sizeof(Color) : group->instanced.colStride;

        // Create and bind VBO for colors
        glGenBuffers(1, &vboColors);
        glBindBuffer(GL_ARRAY_BUFFER, vboColors);
        glBufferData(GL_ARRAY_BUFFER, group->instanced.count * stride, group->instanced.colors, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(locInstanceColor);
        glVertexAttribPointer(locInstanceColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, (int)stride, (void*)0);
        glVertexAttribDivisor(locInstanceColor, 1);
    }

    // Draw the geometry
    if (call->mesh.ebo == 0) {
        glDrawArraysInstanced(
            get_opengl_primitive(call->mesh.primitiveType),
            0, call->mesh.vertexCount, (int)group->instanced.count
        );
    }
    else {
        glDrawElementsInstanced(
            get_opengl_primitive(call->mesh.primitiveType),
            call->mesh.indexCount, GL_UNSIGNED_INT, NULL, (int)group->instanced.count
        );
    }

    // Clean up instanced data
    if (vboTransforms > 0) {
        for (int i = 0; i < 4; i++) {
            glDisableVertexAttribArray(locInstanceModel + i);
            glVertexAttribDivisor(locInstanceModel + i, 0);
        }
        glDeleteBuffers(1, &vboTransforms);
    }
    if (vboColors > 0) {
        glDisableVertexAttribArray(locInstanceColor);
        glVertexAttribDivisor(locInstanceColor, 0);
        glDeleteBuffers(1, &vboColors);
    }

    glBindVertexArray(0);
}
