/* r3d_draw.c -- Internal R3D draw module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_draw.h"
#include <r3d_config.h>
#include <raymath.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <rlgl.h>
#include <glad.h>

#include "../common/r3d_frustum.h"
#include "../common/r3d_math.h"

// ========================================
// MODULE STATE
// ========================================

struct r3d_draw R3D_MOD_DRAW;

// ========================================
// HELPER MACROS
// ========================================

#define IS_CALL_DECAL(call) (                                               \
    ((call)->type == R3D_DRAW_CALL_DECAL)                                   \
)

#define IS_CALL_PREPASS(call) (                                             \
    ((call)->type == R3D_DRAW_CALL_MESH) &&                                 \
    ((call)->mesh.material.transparencyMode == R3D_TRANSPARENCY_PREPASS)    \
)

#define IS_CALL_FORWARD(call) (                                             \
    ((call)->type == R3D_DRAW_CALL_MESH) &&                                 \
    ((call)->mesh.material.transparencyMode == R3D_TRANSPARENCY_ALPHA ||    \
     (call)->mesh.material.blendMode != R3D_BLEND_MIX)                      \
)

// ========================================
// INTERNAL SHAPE FUNCTIONS
// ========================================

static void setup_shape_vertex_attribs(void)
{
    // position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, position));

    // texcoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, texcoord));

    // normal (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, normal));

    // color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, color));

    // tangent (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, tangent));

    // boneIds (ivec4) / weights (vec4) - (disabled)
    glVertexAttrib4iv(5, (int[4]){0, 0, 0, 0});
    glVertexAttrib4f(6, 0.0f, 0.0f, 0.0f, 0.0f);

    // instance position (vec3) (disabled)
    glVertexAttribDivisor(10, 1);
    glVertexAttrib3f(10, 0.0f, 0.0f, 0.0f);

    // instance rotation (vec4) (disabled)
    glVertexAttribDivisor(11, 1);
    glVertexAttrib4f(11, 0.0f, 0.0f, 0.0f, 1.0f);

    // instance scale (vec3) (disabled)
    glVertexAttribDivisor(12, 1);
    glVertexAttrib3f(12, 1.0f, 1.0f, 1.0f);

    // instance color (vec4) (disabled)
    glVertexAttribDivisor(13, 1);
    glVertexAttrib4f(13, 1.0f, 1.0f, 1.0f, 1.0f);
}

static void load_shape(r3d_draw_shape_t* shape, const R3D_Vertex* verts, int vertCount, const GLubyte* indices, int idxCount)
{
    glGenVertexArrays(1, &shape->vao);
    glGenBuffers(1, &shape->vbo);
    glGenBuffers(1, &shape->ebo);

    glBindVertexArray(shape->vao);

    glBindBuffer(GL_ARRAY_BUFFER, shape->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertCount * sizeof(R3D_Vertex), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(GLubyte), indices, GL_STATIC_DRAW);

    shape->vertexCount = vertCount;
    shape->indexCount = idxCount;

    setup_shape_vertex_attribs();
}

typedef void (*draw_shape_loader_func)(r3d_draw_shape_t*);

static void load_shape_dummy(r3d_draw_shape_t* dummy);
static void load_shape_quad(r3d_draw_shape_t* quad);
static void load_shape_cube(r3d_draw_shape_t* cube);

static const draw_shape_loader_func SHAPE_LOADERS[] = {
    [R3D_DRAW_SHAPE_DUMMY] = load_shape_dummy,
    [R3D_DRAW_SHAPE_QUAD] = load_shape_quad,
    [R3D_DRAW_SHAPE_CUBE] = load_shape_cube,
};

void load_shape_dummy(r3d_draw_shape_t* shape)
{
    glGenVertexArrays(1, &shape->vao);
    glBindVertexArray(shape->vao);
    shape->vertexCount = 3;
    shape->indexCount = 0;
}

void load_shape_quad(r3d_draw_shape_t* shape)
{
    static const R3D_Vertex VERTS[] = {
        {{-0.5f, 0.5f, 0}, {0, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{-0.5f,-0.5f, 0}, {0, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f, 0.5f, 0}, {1, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f,-0.5f, 0}, {1, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
    };
    static const GLubyte INDICES[] = {0, 1, 2, 1, 3, 2};
    
    load_shape(shape, VERTS, 4, INDICES, 6);
}

void load_shape_cube(r3d_draw_shape_t* shape)
{
    static const R3D_Vertex VERTS[] = {
        // Front (Z+)
        {{-0.5f, 0.5f, 0.5f}, {0, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{-0.5f,-0.5f, 0.5f}, {0, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f, 0.5f, 0.5f}, {1, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f,-0.5f, 0.5f}, {1, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        // Back (Z-)
        {{-0.5f, 0.5f,-0.5f}, {1, 1}, {0, 0,-1}, {255, 255, 255, 255}, {-1, 0, 0, 1}},
        {{-0.5f,-0.5f,-0.5f}, {1, 0}, {0, 0,-1}, {255, 255, 255, 255}, {-1, 0, 0, 1}},
        {{ 0.5f, 0.5f,-0.5f}, {0, 1}, {0, 0,-1}, {255, 255, 255, 255}, {-1, 0, 0, 1}},
        {{ 0.5f,-0.5f,-0.5f}, {0, 0}, {0, 0,-1}, {255, 255, 255, 255}, {-1, 0, 0, 1}},
        // Left (X-)
        {{-0.5f, 0.5f,-0.5f}, {0, 1}, {-1, 0, 0}, {255, 255, 255, 255}, {0, 0,-1, 1}},
        {{-0.5f,-0.5f,-0.5f}, {0, 0}, {-1, 0, 0}, {255, 255, 255, 255}, {0, 0,-1, 1}},
        {{-0.5f, 0.5f, 0.5f}, {1, 1}, {-1, 0, 0}, {255, 255, 255, 255}, {0, 0,-1, 1}},
        {{-0.5f,-0.5f, 0.5f}, {1, 0}, {-1, 0, 0}, {255, 255, 255, 255}, {0, 0,-1, 1}},
        // Right (X+)
        {{ 0.5f, 0.5f, 0.5f}, {0, 1}, {1, 0, 0}, {255, 255, 255, 255}, {0, 0, 1, 1}},
        {{ 0.5f,-0.5f, 0.5f}, {0, 0}, {1, 0, 0}, {255, 255, 255, 255}, {0, 0, 1, 1}},
        {{ 0.5f, 0.5f,-0.5f}, {1, 1}, {1, 0, 0}, {255, 255, 255, 255}, {0, 0, 1, 1}},
        {{ 0.5f,-0.5f,-0.5f}, {1, 0}, {1, 0, 0}, {255, 255, 255, 255}, {0, 0, 1, 1}},
        // Top (Y+)
        {{-0.5f, 0.5f,-0.5f}, {0, 0}, {0, 1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{-0.5f, 0.5f, 0.5f}, {0, 1}, {0, 1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f, 0.5f,-0.5f}, {1, 0}, {0, 1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f, 0.5f, 0.5f}, {1, 1}, {0, 1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        // Bottom (Y-)
        {{-0.5f,-0.5f, 0.5f}, {0, 0}, {0,-1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{-0.5f,-0.5f,-0.5f}, {0, 1}, {0,-1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f,-0.5f, 0.5f}, {1, 0}, {0,-1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f,-0.5f,-0.5f}, {1, 1}, {0,-1, 0}, {255, 255, 255, 255}, {1, 0, 0, 1}},
    };
    static const GLubyte INDICES[] = {
        0,1,2, 2,1,3,   6,5,4, 7,5,6,   8,9,10, 10,9,11,
        12,13,14, 14,13,15,   16,17,18, 18,17,19,   20,21,22, 22,21,23
    };
    
    load_shape(shape, VERTS, 24, INDICES, 36);
}

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

    GROW_AND_ASSIGN(clusters);
    GROW_AND_ASSIGN(groupVisibility);
    GROW_AND_ASSIGN(callIndices);
    GROW_AND_ASSIGN(groups);

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; ++i) {
        GROW_AND_ASSIGN(list[i].calls);
    }

    GROW_AND_ASSIGN(calls);
    GROW_AND_ASSIGN(groupIndices);
    GROW_AND_ASSIGN(sortCache);

    #undef GROW_AND_ASSIGN

    R3D_MOD_DRAW.capacity = newCapacity;

    return true;
}

// ========================================
// INTERNAL BINDING FUNCTIONS
// ========================================

static inline GLenum get_opengl_primitive(R3D_PrimitiveType primitive)
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

static void bind_draw_call_vao(const r3d_draw_call_t* call, GLenum* primitive, GLenum* elemType, GLint* vertCount, GLint* elemCount)
{
    assert(primitive && elemType && vertCount && elemCount);

    *primitive = GL_NONE;
    *elemType = GL_NONE;
    *vertCount = 0;
    *elemCount = 0;

    switch (call->type) {
    case R3D_DRAW_CALL_MESH:
        {
            const R3D_Mesh* mesh = &call->mesh.instance;
            glBindVertexArray(mesh->vao);

            *primitive = get_opengl_primitive(mesh->primitiveType);
            *vertCount = mesh->vertexCount;
            *elemCount = mesh->indexCount;
            *elemType = GL_UNSIGNED_INT;
        }
        break;
    case R3D_DRAW_CALL_DECAL:
        {
            r3d_draw_shape_t* buffer = &R3D_MOD_DRAW.shapes[R3D_DRAW_SHAPE_CUBE];
            if (buffer->vao == 0) SHAPE_LOADERS[R3D_DRAW_SHAPE_CUBE](buffer);
            else glBindVertexArray(buffer->vao);

            *primitive = GL_TRIANGLES;
            *vertCount = buffer->vertexCount;
            *elemCount = buffer->indexCount;
            *elemType = GL_UNSIGNED_BYTE;
        }
        break;
    default:
        assert(false);
        break;
    }
}

// ========================================
// INTERNAL CULLING FUNCTIONS
// ========================================

static inline bool frustum_test_aabb(const r3d_frustum_t* frustum, const BoundingBox* aabb, const Matrix* transform)
{
    if (memcmp(aabb, &(BoundingBox){0}, sizeof(BoundingBox)) == 0) {
        return true;
    }

    if (!transform || r3d_matrix_is_identity(transform)) {
        return r3d_frustum_is_aabb_in(frustum, aabb);
    }

    return r3d_frustum_is_obb_in(frustum, aabb, transform);
}

static inline bool frustum_test_draw_call(const r3d_frustum_t* frustum, const r3d_draw_call_t* call, const Matrix* transform)
{
    switch (call->type) {
    case R3D_DRAW_CALL_MESH:
        return frustum_test_aabb(frustum, &call->mesh.instance.aabb, transform);
    case R3D_DRAW_CALL_DECAL:
        return frustum_test_aabb(frustum, &(BoundingBox) {
            .min.x = -0.5f, .min.y = -0.5f, .min.z = -0.5f,
            .max.x = +0.5f, .max.y = +0.5f, .max.z = +0.5f
        }, transform);
    default:
        assert(false);
        break;
    }

    return false;
}

// ========================================
// INTERNAL SORTING FUNCTIONS
// ========================================

static Vector3 G_sortViewPosition = {0};

static inline float calculate_center_distance_to_camera(const BoundingBox* aabb, const Matrix* transform)
{
    Vector3 center = {
        (aabb->min.x + aabb->max.x) * 0.5f,
        (aabb->min.y + aabb->max.y) * 0.5f,
        (aabb->min.z + aabb->max.z) * 0.5f
    };
    center = Vector3Transform(center, *transform);

    return Vector3DistanceSqr(G_sortViewPosition, center);
}

static inline float calculate_max_distance_to_camera(const BoundingBox* aabb, const Matrix* transform)
{
    float maxDistSq = 0.0f;

    for (int i = 0; i < 8; ++i) {
        Vector3 corner = {
            (i & 1) ? aabb->max.x : aabb->min.x,
            (i & 2) ? aabb->max.y : aabb->min.y,
            (i & 4) ? aabb->max.z : aabb->min.z
        };

        corner = Vector3Transform(corner, *transform);
        float distSq = Vector3DistanceSqr(G_sortViewPosition, corner);
        maxDistSq = (distSq > maxDistSq) ? distSq : maxDistSq;
    }

    return maxDistSq;
}

static inline void sort_fill_material_data(r3d_draw_sort_t* sortData, const r3d_draw_call_t* call)
{
    switch (call->type) {
    case R3D_DRAW_CALL_MESH:
        sortData->material.shader = (uintptr_t)call->mesh.material.shader;
        sortData->material.albedo = call->mesh.material.albedo.texture.id;
        sortData->material.normal = call->mesh.material.normal.texture.id;
        sortData->material.orm = call->mesh.material.orm.texture.id;
        sortData->material.emission = call->mesh.material.emission.texture.id;
        sortData->material.blend = call->mesh.material.blendMode;
        sortData->material.cull = call->mesh.material.cullMode;
        sortData->material.transparency = call->mesh.material.transparencyMode;
        sortData->material.billboard = call->mesh.material.billboardMode;
        break;
    
    case R3D_DRAW_CALL_DECAL:
        sortData->material.shader = (uintptr_t)call->decal.instance.shader;
        sortData->material.albedo = call->decal.instance.albedo.texture.id;
        sortData->material.normal = call->decal.instance.normal.texture.id;
        sortData->material.orm = call->decal.instance.orm.texture.id;
        sortData->material.emission = call->decal.instance.emission.texture.id;
        sortData->material.blend = R3D_BLEND_MIX;
        sortData->material.cull = R3D_CULL_NONE;
        sortData->material.transparency = R3D_TRANSPARENCY_ALPHA;
        sortData->material.billboard = R3D_BILLBOARD_DISABLED;
        break;
    }
}

static void sort_fill_cache_front_to_back(r3d_draw_list_enum_t list)
{
    assert(list < R3D_DRAW_LIST_NON_INST_COUNT && "Instantiated render lists should not be sorted by distance");
    assert(list != R3D_DRAW_LIST_DECAL && "Decal render list should not be sorted by distance");

    r3d_draw_list_t* drawList = &R3D_MOD_DRAW.list[list];

    for (int i = 0; i < drawList->numCalls; i++)
    {
        int callIndex = drawList->calls[i];
        const r3d_draw_call_t* call = &R3D_MOD_DRAW.calls[callIndex];
        const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
        r3d_draw_sort_t* sortData = &R3D_MOD_DRAW.sortCache[callIndex];

        sortData->distance = calculate_center_distance_to_camera(
            &call->mesh.instance.aabb, &group->transform
        );
        
        sort_fill_material_data(sortData, call);
    }
}

static void sort_fill_cache_back_to_front(r3d_draw_list_enum_t list)
{
    assert(list < R3D_DRAW_LIST_NON_INST_COUNT && "Instantiated render lists should not be sorted by distance");
    assert(list != R3D_DRAW_LIST_DECAL && "Decal render list should not be sorted by distance");

    r3d_draw_list_t* drawList = &R3D_MOD_DRAW.list[list];

    for (int i = 0; i < drawList->numCalls; i++)
    {
        int callIndex = drawList->calls[i];
        const r3d_draw_call_t* call = &R3D_MOD_DRAW.calls[callIndex];
        const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
        r3d_draw_sort_t* sortData = &R3D_MOD_DRAW.sortCache[callIndex];

        sortData->distance = calculate_max_distance_to_camera(
            &call->mesh.instance.aabb, &group->transform
        );

        // For back-to-front (transparency), we don't sort by material.
        //sort_fill_material_data(sortData, call);
    }
}

static void sort_fill_cache_by_material(r3d_draw_list_enum_t list)
{
    r3d_draw_list_t* drawList = &R3D_MOD_DRAW.list[list];

    for (int i = 0; i < drawList->numCalls; i++)
    {
        int callIndex = drawList->calls[i];
        const r3d_draw_call_t* call = &R3D_MOD_DRAW.calls[callIndex];
        r3d_draw_sort_t* sortData = &R3D_MOD_DRAW.sortCache[callIndex];

        sortData->distance = 0.0f;

        sort_fill_material_data(sortData, call);
    }
}

static int compare_front_to_back(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;

    int materialCmp = memcmp(
        &R3D_MOD_DRAW.sortCache[indexA].material,
        &R3D_MOD_DRAW.sortCache[indexB].material,
        sizeof(R3D_MOD_DRAW.sortCache[0].material)
    );

    if (materialCmp != 0) {
        return materialCmp;
    }

    float distA = R3D_MOD_DRAW.sortCache[indexA].distance;
    float distB = R3D_MOD_DRAW.sortCache[indexB].distance;

    return (distA > distB) - (distA < distB);
}

static int compare_back_to_front(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;

    float distA = R3D_MOD_DRAW.sortCache[indexA].distance;
    float distB = R3D_MOD_DRAW.sortCache[indexB].distance;

    return (distA < distB) - (distA > distB);
}

static int compare_materials_only(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;

    return memcmp(
        &R3D_MOD_DRAW.sortCache[indexA].material,
        &R3D_MOD_DRAW.sortCache[indexB].material,
        sizeof(R3D_MOD_DRAW.sortCache[0].material)
    );
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
            R3D_TRACELOG(LOG_FATAL, "Failed to init draw module; " logfmt, ##__VA_ARGS__); \
            goto fail; \
        } \
        R3D_MOD_DRAW.field = _p; \
    } while (0)

    memset(&R3D_MOD_DRAW, 0, sizeof(R3D_MOD_DRAW));

    ALLOC_AND_ASSIGN(clusters, "Draw cluster array allocation failed");
    ALLOC_AND_ASSIGN(groupVisibility, "Group visibility array allocation failed");
    ALLOC_AND_ASSIGN(callIndices, "Draw call indices array allocation failed");
    ALLOC_AND_ASSIGN(groups, "Draw group array allocation failed");

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        ALLOC_AND_ASSIGN(list[i].calls, "Draw call array %i allocation failed", i);
    }

    ALLOC_AND_ASSIGN(calls, "Draw call array allocation failed");
    ALLOC_AND_ASSIGN(groupIndices, "Draw group indices array allocation failed");
    ALLOC_AND_ASSIGN(sortCache, "Sorting cache array allocation failed");

    #undef ALLOC_AND_ASSIGN

    R3D_MOD_DRAW.capacity = DRAW_RESERVE_COUNT;
    R3D_MOD_DRAW.activeCluster = -1;

    return true;

fail:
    r3d_draw_quit();
    return false;
}

void r3d_draw_quit(void)
{
    for (int i = 0; i < R3D_DRAW_SHAPE_COUNT; i++) {
        r3d_draw_shape_t* buffer = &R3D_MOD_DRAW.shapes[i];
        if (buffer->vao) glDeleteVertexArrays(1, &buffer->vao);
        if (buffer->vbo) glDeleteBuffers(1, &buffer->vbo);
        if (buffer->ebo) glDeleteBuffers(1, &buffer->ebo);
    }

    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        RL_FREE(R3D_MOD_DRAW.list[i].calls);
    }

    RL_FREE(R3D_MOD_DRAW.groupVisibility);
    RL_FREE(R3D_MOD_DRAW.groupIndices);
    RL_FREE(R3D_MOD_DRAW.callIndices);
    RL_FREE(R3D_MOD_DRAW.sortCache);
    RL_FREE(R3D_MOD_DRAW.groups);
    RL_FREE(R3D_MOD_DRAW.calls);
}

void r3d_draw_clear(void)
{
    for (int i = 0; i < R3D_DRAW_LIST_COUNT; i++) {
        R3D_MOD_DRAW.list[i].numCalls = 0;
    }
    R3D_MOD_DRAW.numClusters = 0;
    R3D_MOD_DRAW.numGroups = 0;
    R3D_MOD_DRAW.numCalls = 0;
}

bool r3d_draw_cluster_begin(BoundingBox aabb)
{
    if (R3D_MOD_DRAW.activeCluster >= 0) {
        return false;
    }

    if (R3D_MOD_DRAW.numClusters >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Bad alloc on draw cluster begin");
            return false;
        }
    }

    R3D_MOD_DRAW.activeCluster = R3D_MOD_DRAW.numClusters++;

    r3d_draw_cluster_t* cluster = &R3D_MOD_DRAW.clusters[R3D_MOD_DRAW.activeCluster];
    cluster->visible = R3D_DRAW_VISBILITY_UNKNOWN;
    cluster->aabb = aabb;

    return true;
}

bool r3d_draw_cluster_end(void)
{
    if (R3D_MOD_DRAW.activeCluster < 0) return false;
    R3D_MOD_DRAW.activeCluster = -1;
    return true;
}

void r3d_draw_group_push(const r3d_draw_group_t* group)
{
    if (R3D_MOD_DRAW.numGroups >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Bad alloc on draw group push");
            return;
        }
    }

    int groupIndex = R3D_MOD_DRAW.numGroups++;

    R3D_MOD_DRAW.groupVisibility[groupIndex] = (r3d_draw_group_visibility_t) {
        .clusterIndex = R3D_MOD_DRAW.activeCluster,
        .visible = R3D_DRAW_VISBILITY_UNKNOWN
    };

    R3D_MOD_DRAW.callIndices[groupIndex] = (r3d_draw_indices_t) {0};
    R3D_MOD_DRAW.groups[groupIndex] = *group;
}

void r3d_draw_call_push(const r3d_draw_call_t* call)
{
    if (R3D_MOD_DRAW.numCalls >= R3D_MOD_DRAW.capacity) {
        if (!growth_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Bad alloc on draw call push");
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
    r3d_draw_list_enum_t list = R3D_DRAW_LIST_DEFERRED;
    if (IS_CALL_DECAL(call)) list = R3D_DRAW_LIST_DECAL;
    else if (IS_CALL_PREPASS(call)) list = R3D_DRAW_LIST_PREPASS;
    else if (IS_CALL_FORWARD(call)) list = R3D_DRAW_LIST_FORWARD;
    if (r3d_draw_has_instances(group)) list += R3D_DRAW_LIST_NON_INST_COUNT;

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
        r3d_draw_group_visibility_t* visibility = &R3D_MOD_DRAW.groupVisibility[i];
        const r3d_draw_group_t* group = &R3D_MOD_DRAW.groups[i];
        const BoundingBox* aabb = &group->aabb;

        if (visibility->clusterIndex >= 0) {
            r3d_draw_cluster_t* cluster = &R3D_MOD_DRAW.clusters[visibility->clusterIndex];

            if (cluster->visible == R3D_DRAW_VISBILITY_UNKNOWN) {
                cluster->visible = frustum_test_aabb(frustum, &cluster->aabb, NULL);
            }

            if (cluster->visible == R3D_DRAW_VISBILITY_TRUE) {
                // If the group represents multiple instances or a skinned model, rely only on the cluster visibility
                // TODO: It would be better to find an improved method for skinned models
                visibility->visible =
                    r3d_draw_has_instances(group) || (group->texPose > 0) ||
                    frustum_test_aabb(frustum, &group->aabb, &group->transform);
            }
            else {
                visibility->visible = R3D_DRAW_VISBILITY_FALSE;
            }
        }
        else {
            // If the group represents multiple instances or a skinned model, consider to be always visible
            // TODO: It would be better to find an improved method for skinned models
            visibility->visible =
                r3d_draw_has_instances(group) || (group->texPose > 0) ||
                frustum_test_aabb(frustum, &group->aabb, &group->transform);
        }
    }
}

bool r3d_draw_call_is_visible(const r3d_draw_call_t* call, const r3d_frustum_t* frustum)
{
    // If the parent group is not visible, discard this call immediately
    // Then, if the call count for the group is one, it means it has already been tested
    // Finally, if the group represents instances or a skinned model, rely only on the group's visibility

    int callIndex = get_draw_call_index(call);
    int groupIndex = R3D_MOD_DRAW.groupIndices[callIndex];
    const r3d_draw_group_t* group = &R3D_MOD_DRAW.groups[groupIndex];

    if (!R3D_MOD_DRAW.groupVisibility[groupIndex].visible) return false;
    if (R3D_MOD_DRAW.callIndices[groupIndex].numCall == 1) return true;
    if (r3d_draw_has_instances(group) || group->texPose > 0) return true;

    return frustum_test_draw_call(frustum, call, &group->transform);
}

void r3d_draw_sort_list(r3d_draw_list_enum_t list, Vector3 viewPosition, r3d_draw_sort_enum_t mode)
{
    G_sortViewPosition = viewPosition;

    int (*compare_func)(const void *a, const void *b) = NULL;
    r3d_draw_list_t* drawList = &R3D_MOD_DRAW.list[list];

    switch (mode) {
    case R3D_DRAW_SORT_FRONT_TO_BACK:
        compare_func = compare_front_to_back;
        sort_fill_cache_front_to_back(list);
        break;
    case R3D_DRAW_SORT_BACK_TO_FRONT:
        compare_func = compare_back_to_front;
        sort_fill_cache_back_to_front(list);
        break;
    case R3D_DRAW_SORT_MATERIAL_ONLY:
        compare_func = compare_materials_only;
        sort_fill_cache_by_material(list);
        break;
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
        assert(false && "This shouldn't happen");
        break;
    }
}

void r3d_draw(const r3d_draw_call_t* call)
{
    GLenum primitive, elemType;
    GLint vertCount, elemCount;

    bind_draw_call_vao(call, &primitive, &elemType, &vertCount, &elemCount);

    if (elemCount == 0) glDrawArrays(primitive, 0, vertCount);
    else glDrawElements(primitive, elemCount, elemType, NULL);
}

void r3d_draw_instanced(const r3d_draw_call_t* call)
{
    GLenum primitive, elemType;
    GLint vertCount, elemCount;

    bind_draw_call_vao(call, &primitive, &elemType, &vertCount, &elemCount);

    const r3d_draw_group_t* group = r3d_draw_get_call_group(call);
    const R3D_InstanceBuffer* instances = &group->instances;

    if (BIT_TEST(instances->flags, R3D_INSTANCE_POSITION)) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[0]);
        glEnableVertexAttribArray(10);
        glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), 0);
    }
    else {
        glDisableVertexAttribArray(10);
    }

    if (BIT_TEST(instances->flags, R3D_INSTANCE_ROTATION)) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[1]);
        glEnableVertexAttribArray(11);
        glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(Quaternion), 0);
    }
    else {
        glDisableVertexAttribArray(11);
    }

    if (BIT_TEST(instances->flags, R3D_INSTANCE_SCALE)) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[2]);
        glEnableVertexAttribArray(12);
        glVertexAttribPointer(12, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), 0);
    }
    else {
        glDisableVertexAttribArray(12);
    }

    if (BIT_TEST(instances->flags, R3D_INSTANCE_COLOR)) {
        glBindBuffer(GL_ARRAY_BUFFER, instances->buffers[3]);
        glEnableVertexAttribArray(13);
        glVertexAttribPointer(13, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Color), 0);
    }
    else {
        glDisableVertexAttribArray(13);
    }

    if (elemCount == 0) {
        glDrawArraysInstanced(primitive, 0, vertCount, group->instanceCount);
    }
    else {
        glDrawElementsInstanced(primitive, elemCount, elemType, NULL, group->instanceCount);
    }
}

void r3d_draw_shape(r3d_draw_shape_enum_t shape)
{
    r3d_draw_shape_t* buffer = &R3D_MOD_DRAW.shapes[shape];

    // NOTE: The loader leaves the vao bound
    if (buffer->vao == 0) SHAPE_LOADERS[shape](buffer);
    else glBindVertexArray(buffer->vao);

    if (buffer->indexCount > 0) {
        glDrawElements(GL_TRIANGLES, buffer->indexCount, GL_UNSIGNED_BYTE, 0);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, buffer->vertexCount);
    }
}
