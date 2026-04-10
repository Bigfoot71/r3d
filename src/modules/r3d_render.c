/* r3d_render.c -- Internal R3D render module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_render.h"
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
#include "../common/r3d_hash.h"

// ========================================
// MODULE STATE
// ========================================

struct r3d_mod_render R3D_MOD_RENDER;

// ========================================
// INTERNAL BUFFER RESIZE FUNCTIONS
// ========================================

/*
 * Reconfigures all vertex attribute pointers on the global VAO after a VBO resize.
 * Must be called with the global VAO already bound.
 */
static void reconfigure_global_vao_attribs(void)
{
    glBindBuffer(GL_ARRAY_BUFFER, R3D_MOD_RENDER.globalVbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, texcoord));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, normal));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, color));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, tangent));

    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, boneIds));

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, weights));
}

/*
 * Grows the global VBO to at least 'minCapacity' vertices.
 * Creates a new buffer, copies the old content via glCopyBufferSubData,
 * deletes the old buffer, and reconfigures the VAO attrib pointers.
 */
static bool grow_global_vbo(int minCapacity)
{
    int newCapacity = R3D_MOD_RENDER.globalVertexCapacity * 2;
    while (newCapacity < minCapacity) newCapacity *= 2;

    GLuint newVbo;
    glGenBuffers(1, &newVbo);

    // Allocate the new buffer without data
    glBindBuffer(GL_COPY_WRITE_BUFFER, newVbo);
    glBufferData(GL_COPY_WRITE_BUFFER, newCapacity * sizeof(R3D_Vertex), NULL, GL_DYNAMIC_DRAW);

    // Copy the old content
    glBindBuffer(GL_COPY_READ_BUFFER, R3D_MOD_RENDER.globalVbo);
    glCopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
        0, 0,
        R3D_MOD_RENDER.globalVertexCount * sizeof(R3D_Vertex)
    );

    // Replaces the old buffer
    glDeleteBuffers(1, &R3D_MOD_RENDER.globalVbo);
    R3D_MOD_RENDER.globalVbo = newVbo;
    R3D_MOD_RENDER.globalVertexCapacity = newCapacity;

    // Reconfigure the assignments on the new VBO
    glBindVertexArray(R3D_MOD_RENDER.globalVao);
    reconfigure_global_vao_attribs();
    glBindVertexArray(0);

    return true;
}

/*
 * Grows the global EBO to at least 'minCapacity' indices.
 * Same strategy as grow_global_vbo: new buffer + copy + delete + rebind.
 */
static bool grow_global_ebo(int minCapacity)
{
    int newCapacity = R3D_MOD_RENDER.globalElementCapacity * 2;
    while (newCapacity < minCapacity) newCapacity *= 2;

    GLuint newEbo;
    glGenBuffers(1, &newEbo);

    glBindBuffer(GL_COPY_WRITE_BUFFER, newEbo);
    glBufferData(GL_COPY_WRITE_BUFFER, newCapacity * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_COPY_READ_BUFFER, R3D_MOD_RENDER.globalEbo);
    glCopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
        0, 0,
        R3D_MOD_RENDER.globalElementCount * sizeof(GLuint)
    );

    glDeleteBuffers(1, &R3D_MOD_RENDER.globalEbo);
    R3D_MOD_RENDER.globalEbo = newEbo;
    R3D_MOD_RENDER.globalElementCapacity = newCapacity;

    // Rebind the EBO into the global VAO
    glBindVertexArray(R3D_MOD_RENDER.globalVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, R3D_MOD_RENDER.globalEbo);
    glBindVertexArray(0);

    return true;
}

/*
 * Copies 'count' vertex slots from 'srcOffset' to 'dstOffset' within
 * the global VBO. Ranges must not overlap.
 */
static void copy_global_vertices(int dstOffset, int srcOffset, int count)
{
    GLsizeiptr size = count * sizeof(R3D_Vertex);

    glBindBuffer(GL_COPY_READ_BUFFER, R3D_MOD_RENDER.globalVbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, R3D_MOD_RENDER.globalVbo);
    glCopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
        srcOffset * sizeof(R3D_Vertex),
        dstOffset * sizeof(R3D_Vertex),
        size
    );
}

/*
 * Copies 'count' index slots from 'srcOffset' to 'dstOffset' within
 * the global EBO. Ranges must not overlap.
 */
static void copy_global_elements(int dstOffset, int srcOffset, int count)
{
    GLsizeiptr size = count * sizeof(GLuint);

    glBindBuffer(GL_COPY_READ_BUFFER, R3D_MOD_RENDER.globalEbo);
    glBindBuffer(GL_COPY_WRITE_BUFFER, R3D_MOD_RENDER.globalEbo);
    glCopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
        srcOffset * sizeof(GLuint),
        dstOffset * sizeof(GLuint),
        size
    );
}

// ========================================
// INTERNAL FREE LIST FUNCTIONS
// ========================================

/*
 * After inserting a range into a free list, sort by offset and merge
 * adjacent/overlapping blocks. Keeps the list compact and prevents
 * fragmentation from accumulating across many alloc/free cycles.
 */
static void coalesce_free_list(r3d_render_range_t* list, int* count)
{
    // Offset sorting (insertion sort, the list is almost sorted in practice)
    for (int i = 1; i < *count; i++) {
        r3d_render_range_t key = list[i];
        int j = i - 1;
        while (j >= 0 && list[j].offset > key.offset) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = key;
    }

    // Merging of adjacent blocks
    int write = 0;
    for (int i = 1; i < *count; i++) {
        if (list[write].offset + list[write].count >= list[i].offset) {
            // Contiguous or overlapping blocks: we extend
            int end = list[write].offset + list[write].count;
            int end_i = list[i].offset + list[i].count;
            if (end_i > end) list[write].count = end_i - list[write].offset;
        }
        else {
            list[++write] = list[i];
        }
    }
    *count = write + 1;
}

/*
 * Pushes a range onto a free list, growing the list's backing array if needed.
 * Returns false on allocation failure.
 */
static bool push_free_range(r3d_render_range_t** list, int* count, int* capacity,
                            int offset, int rangeCount)
{
    if (*count >= *capacity) {
        int newCapacity = (*capacity) * 2;
        r3d_render_range_t* p = RL_REALLOC(*list, newCapacity * sizeof(r3d_render_range_t));
        if (!p) return false;
        *list = p;
        *capacity = newCapacity;
    }

    (*list)[(*count)++] = (r3d_render_range_t) {
        .offset = offset, .count = rangeCount
    };

    return true;
}

/*
 * First-fit search in a free list.
 * If a block large enough is found:
 *   - it is split if strictly larger than needed (remainder stays in the list)
 *   - it is removed entirely if it matches exactly
 * Returns the offset on success, -1 if nothing fits.
 */
static int pop_free_range(r3d_render_range_t* list, int* count, int needed)
{
    for (int i = 0; i < *count; i++) {
        if (list[i].count >= needed) {
            int offset = list[i].offset;
            if (list[i].count > needed) {
                // Cut: we keep the rest in the list
                list[i].offset += needed;
                list[i].count  -= needed;
            }
            else {
                // Exact match: remove the entry
                list[i] = list[--(*count)];
            }
            return offset;
        }
    }
    return -1;
}

/*
 * Searches the free list for a block starting exactly at 'afterOffset'
 * with at least 'needed' slots. If found, consumes 'needed' slots from it
 * (splitting the remainder back) and returns true.
 */
static bool try_extend_in_place(r3d_render_range_t* list, int* count,
                                int afterOffset, int needed)
{
    for (int i = 0; i < *count; i++) {
        if (list[i].offset != afterOffset) continue;

        if (list[i].count < needed) {
            // Adjacent block but too small
            return false;
        }

        if (list[i].count == needed) {
            // Exact match: remove the entry
            list[i] = list[--(*count)];
        }
        else {
            // We consume 'needed' since the beginning of the block
            list[i].offset += needed;
            list[i].count  -= needed;
        }

        return true;
    }

    return false;
}

// ========================================
// INTERNAL SHAPE FUNCTIONS
// ========================================

typedef void (*shape_loader_func)(r3d_render_shape_t*);

static void load_shape_dummy(r3d_render_shape_t* dummy);
static void load_shape_quad(r3d_render_shape_t* quad);
static void load_shape_cube(r3d_render_shape_t* cube);

static const shape_loader_func SHAPE_LOADERS[] = {
    [R3D_RENDER_SHAPE_DUMMY] = load_shape_dummy,
    [R3D_RENDER_SHAPE_QUAD] = load_shape_quad,
    [R3D_RENDER_SHAPE_CUBE] = load_shape_cube,
};

void load_shape_dummy(r3d_render_shape_t* shape)
{
    shape->vertices.count = 3;
    shape->elements.count = 0;
}

void load_shape_quad(r3d_render_shape_t* shape)
{
    static const R3D_Vertex VERTICES[] = {
        {{-0.5f, 0.5f, 0}, {0, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{-0.5f,-0.5f, 0}, {0, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f, 0.5f, 0}, {1, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f,-0.5f, 0}, {1, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
    };
    static const uint32_t INDICES[] = {0, 1, 2, 1, 3, 2};

    r3d_render_alloc_vertices(ARRAY_SIZE(VERTICES), &shape->vertices.offset);
    r3d_render_alloc_elements(ARRAY_SIZE(INDICES), &shape->elements.offset);

    r3d_render_upload_vertices(shape->vertices.offset, VERTICES, ARRAY_SIZE(VERTICES));
    r3d_render_upload_elements(shape->elements.offset, INDICES, ARRAY_SIZE(INDICES));

    shape->vertices.count = ARRAY_SIZE(VERTICES);
    shape->elements.count = ARRAY_SIZE(INDICES);
}

void load_shape_cube(r3d_render_shape_t* shape)
{
    static const R3D_Vertex VERTICES[] = {
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
    static const uint32_t INDICES[] = {
        0,1,2, 2,1,3,   6,5,4, 7,5,6,   8,9,10, 10,9,11,
        12,13,14, 14,13,15,   16,17,18, 18,17,19,   20,21,22, 22,21,23
    };

    r3d_render_alloc_vertices(ARRAY_SIZE(VERTICES), &shape->vertices.offset);
    r3d_render_alloc_elements(ARRAY_SIZE(INDICES), &shape->elements.offset);

    r3d_render_upload_vertices(shape->vertices.offset, VERTICES, ARRAY_SIZE(VERTICES));
    r3d_render_upload_elements(shape->elements.offset, INDICES, ARRAY_SIZE(INDICES));

    shape->vertices.count = ARRAY_SIZE(VERTICES);
    shape->elements.count = ARRAY_SIZE(INDICES);
}

// ========================================
// INTERNAL INSTANCES FUNCTIONS
// ========================================

static void enable_instances(const GLuint buffers[R3D_INSTANCE_ATTRIBUTE_COUNT], R3D_InstanceFlags flags, int offset)
{
    if (BIT_TEST(flags, R3D_INSTANCE_POSITION)) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
        glEnableVertexAttribArray(10);
        glVertexAttribDivisor(10, 1);
        glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), (void*)(offset * sizeof(Vector3)));
    }
    else {
        glDisableVertexAttribArray(10);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_ROTATION)) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
        glEnableVertexAttribArray(11);
        glVertexAttribDivisor(11, 1);
        glVertexAttribPointer(11, 4, GL_FLOAT, GL_FALSE, sizeof(Quaternion), (void*)(offset * sizeof(Quaternion)));
    }
    else {
        glDisableVertexAttribArray(11);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_SCALE)) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[2]);
        glEnableVertexAttribArray(12);
        glVertexAttribDivisor(12, 1);
        glVertexAttribPointer(12, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), (void*)(offset * sizeof(Vector3)));
    }
    else {
        glDisableVertexAttribArray(12);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_COLOR)) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[3]);
        glEnableVertexAttribArray(13);
        glVertexAttribDivisor(13, 1);
        glVertexAttribPointer(13, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Color), (void*)(offset * sizeof(Color)));
    }
    else {
        glDisableVertexAttribArray(13);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_CUSTOM)) {
        glBindBuffer(GL_ARRAY_BUFFER, buffers[4]);
        glEnableVertexAttribArray(14);
        glVertexAttribDivisor(14, 1);
        glVertexAttribPointer(14, 4, GL_FLOAT, GL_FALSE, sizeof(Vector4), (void*)(offset * sizeof(Vector4)));
    }
    else {
        glDisableVertexAttribArray(14);
    }
}

static void disable_instances(R3D_InstanceFlags flags)
{
    if (BIT_TEST(flags, R3D_INSTANCE_POSITION)) {
        glDisableVertexAttribArray(10);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_ROTATION)) {
        glDisableVertexAttribArray(11);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_SCALE)) {
        glDisableVertexAttribArray(12);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_COLOR)) {
        glDisableVertexAttribArray(13);
    }

    if (BIT_TEST(flags, R3D_INSTANCE_CUSTOM)) {
        glDisableVertexAttribArray(14);
    }
}

// ========================================
// INTERNAL ARRAY FUNCTIONS
// ========================================

static inline int get_draw_call_index(const r3d_render_call_t* call)
{
    assert(call >= R3D_MOD_RENDER.calls);
    return (int)(call - R3D_MOD_RENDER.calls);
}

static inline int get_last_group_index(void)
{
    int groupIndex = R3D_MOD_RENDER.numGroups - 1;
    assert(groupIndex >= 0);
    return groupIndex;
}

static inline r3d_render_group_t* get_last_group(void)
{
    int groupIndex = get_last_group_index();
    return &R3D_MOD_RENDER.groups[groupIndex];
}

static bool growth_arrays(void)
{
    #define GROW_AND_ASSIGN(field) do { \
        void* _p = RL_REALLOC(R3D_MOD_RENDER.field, newCapacity * sizeof(*R3D_MOD_RENDER.field)); \
        if (_p == NULL) return false; \
        R3D_MOD_RENDER.field = _p; \
    } while (0)

    int newCapacity = 2 * R3D_MOD_RENDER.capacity;

    GROW_AND_ASSIGN(clusters);
    GROW_AND_ASSIGN(groupVisibility);
    GROW_AND_ASSIGN(callIndices);
    GROW_AND_ASSIGN(groups);

    for (int i = 0; i < R3D_RENDER_LIST_COUNT; ++i) {
        GROW_AND_ASSIGN(list[i].calls);
    }

    GROW_AND_ASSIGN(calls);
    GROW_AND_ASSIGN(groupIndices);
    GROW_AND_ASSIGN(sortCache);

    #undef GROW_AND_ASSIGN

    R3D_MOD_RENDER.capacity = newCapacity;

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

static void get_draw_call_info(const r3d_render_call_t* call, GLenum* primitive, r3d_render_range_t* vertexRange, r3d_render_range_t* indexRange)
{
    assert(primitive && vertexRange && indexRange);

    *primitive = GL_NONE;
    *vertexRange = (r3d_render_range_t) {0};
    *indexRange = (r3d_render_range_t) {0};

    switch (call->type) {
    case R3D_RENDER_CALL_MESH:
        {
            const R3D_Mesh* mesh = &call->mesh.instance;

            *primitive = get_opengl_primitive(mesh->primitiveType);
            vertexRange->offset = mesh->vertexOffset;
            vertexRange->count = mesh->vertexCount;
            indexRange->offset = mesh->indexOffset;
            indexRange->count = mesh->indexCount;
        }
        break;
    case R3D_RENDER_CALL_DECAL:
        {
            r3d_render_shape_t* shape = &R3D_MOD_RENDER.shapes[R3D_RENDER_SHAPE_CUBE];
            if (shape->vertices.count == 0) {
                SHAPE_LOADERS[R3D_RENDER_SHAPE_CUBE](shape);
                glBindVertexArray(R3D_MOD_RENDER.globalVao);
            }

            *primitive = GL_TRIANGLES;
            vertexRange->offset = shape->vertices.offset;
            vertexRange->count = shape->vertices.count;
            indexRange->offset = shape->elements.offset;
            indexRange->count = shape->elements.count;
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

static inline bool is_aabb_visible(const r3d_frustum_t* frustum, BoundingBox aabb)
{
    if (memcmp(&aabb, &(BoundingBox){0}, sizeof(BoundingBox)) == 0) {
        return true;
    }

    return r3d_frustum_is_aabb_in(frustum, aabb);
}

static inline bool is_obb_visible(const r3d_frustum_t* frustum, r3d_oriented_box_t obb)
{
    if (memcmp(&obb, &(r3d_oriented_box_t){0}, sizeof(r3d_oriented_box_t)) == 0) {
        return true;
    }

    return r3d_frustum_is_obb_in(frustum, obb);
}

static inline bool is_transformed_aabb_visible(const r3d_frustum_t* frustum, BoundingBox aabb, Matrix transform)
{
    if (memcmp(&aabb, &(BoundingBox){0}, sizeof(BoundingBox)) == 0) {
        return true;
    }

    if (r3d_matrix_is_identity(transform)) {
        return r3d_frustum_is_aabb_in(frustum, aabb);
    }

    return r3d_frustum_is_obb_in(frustum, r3d_compute_obb(aabb, transform));
}

static inline bool is_draw_call_visible(const r3d_frustum_t* frustum, const r3d_render_call_t* call, Matrix transform)
{
    switch (call->type) {
    case R3D_RENDER_CALL_MESH:
        return is_transformed_aabb_visible(frustum, call->mesh.instance.aabb, transform);
    case R3D_RENDER_CALL_DECAL:
        return is_transformed_aabb_visible(frustum, (BoundingBox) {
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

static inline void sort_fill_material_data(r3d_render_sort_t* sortData, const r3d_render_call_t* call)
{
    memset(sortData, 0, sizeof(*sortData));

    switch (call->type) {
    case R3D_RENDER_CALL_MESH:
        sortData->material.shader = (uintptr_t)call->mesh.material.shader;
        sortData->material.shading = call->mesh.material.unlit;
        sortData->material.albedo = call->mesh.material.albedo.texture.id;
        sortData->material.normal = call->mesh.material.normal.texture.id;
        sortData->material.orm = call->mesh.material.orm.texture.id;
        sortData->material.emission = call->mesh.material.emission.texture.id;
        sortData->material.stencil = r3d_hash_fnv1a_32(&call->mesh.material.stencil, sizeof(call->mesh.material.stencil));
        sortData->material.depth = r3d_hash_fnv1a_32(&call->mesh.material.depth, sizeof(call->mesh.material.depth));
        sortData->material.blend = call->mesh.material.blendMode;
        sortData->material.cull = call->mesh.material.cullMode;
        sortData->material.transparency = call->mesh.material.transparencyMode;
        sortData->material.billboard = call->mesh.material.billboardMode;
        break;

    case R3D_RENDER_CALL_DECAL:
        sortData->material.shader = (uintptr_t)call->decal.instance.shader;
        sortData->material.albedo = call->decal.instance.albedo.texture.id;
        sortData->material.normal = call->decal.instance.normal.texture.id;
        sortData->material.orm = call->decal.instance.orm.texture.id;
        sortData->material.emission = call->decal.instance.emission.texture.id;
        break;
    }
}

static void sort_fill_cache_front_to_back(r3d_render_list_enum_t list)
{
    assert(list < R3D_RENDER_LIST_NON_INST_COUNT && "Instantiated render lists should not be sorted by distance");
    assert(list != R3D_RENDER_LIST_DECAL && "Decal render list should not be sorted by distance");

    r3d_render_list_t* drawList = &R3D_MOD_RENDER.list[list];

    for (int i = 0; i < drawList->numCalls; i++)
    {
        int callIndex = drawList->calls[i];
        const r3d_render_call_t* call = &R3D_MOD_RENDER.calls[callIndex];
        const r3d_render_group_t* group = r3d_render_get_call_group(call);
        r3d_render_sort_t* sortData = &R3D_MOD_RENDER.sortCache[callIndex];

        sortData->distance = calculate_center_distance_to_camera(
            &call->mesh.instance.aabb, &group->transform
        );
        
        sort_fill_material_data(sortData, call);
    }
}

static void sort_fill_cache_back_to_front(r3d_render_list_enum_t list)
{
    assert(list < R3D_RENDER_LIST_NON_INST_COUNT && "Instantiated render lists should not be sorted by distance");
    assert(list != R3D_RENDER_LIST_DECAL && "Decal render list should not be sorted by distance");

    r3d_render_list_t* drawList = &R3D_MOD_RENDER.list[list];

    for (int i = 0; i < drawList->numCalls; i++)
    {
        int callIndex = drawList->calls[i];
        const r3d_render_call_t* call = &R3D_MOD_RENDER.calls[callIndex];
        const r3d_render_group_t* group = r3d_render_get_call_group(call);
        r3d_render_sort_t* sortData = &R3D_MOD_RENDER.sortCache[callIndex];

        sortData->distance = calculate_max_distance_to_camera(
            &call->mesh.instance.aabb, &group->transform
        );

        // For back-to-front (transparency), we don't sort by material.
        //sort_fill_material_data(sortData, call);
    }
}

static void sort_fill_cache_by_material(r3d_render_list_enum_t list)
{
    r3d_render_list_t* drawList = &R3D_MOD_RENDER.list[list];

    for (int i = 0; i < drawList->numCalls; i++)
    {
        int callIndex = drawList->calls[i];
        const r3d_render_call_t* call = &R3D_MOD_RENDER.calls[callIndex];
        r3d_render_sort_t* sortData = &R3D_MOD_RENDER.sortCache[callIndex];

        sortData->distance = 0.0f;

        sort_fill_material_data(sortData, call);
    }
}

static int compare_front_to_back(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;

    int materialCmp = memcmp(
        &R3D_MOD_RENDER.sortCache[indexA].material,
        &R3D_MOD_RENDER.sortCache[indexB].material,
        sizeof(R3D_MOD_RENDER.sortCache[0].material)
    );

    if (materialCmp != 0) {
        return materialCmp;
    }

    float distA = R3D_MOD_RENDER.sortCache[indexA].distance;
    float distB = R3D_MOD_RENDER.sortCache[indexB].distance;

    return (distA > distB) - (distA < distB);
}

static int compare_back_to_front(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;

    float distA = R3D_MOD_RENDER.sortCache[indexA].distance;
    float distB = R3D_MOD_RENDER.sortCache[indexB].distance;

    return (distA < distB) - (distA > distB);
}

static int compare_materials_only(const void* a, const void* b)
{
    int indexA = *(int*)a;
    int indexB = *(int*)b;

    return memcmp(
        &R3D_MOD_RENDER.sortCache[indexA].material,
        &R3D_MOD_RENDER.sortCache[indexB].material,
        sizeof(R3D_MOD_RENDER.sortCache[0].material)
    );
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_render_init(void)
{
    memset(&R3D_MOD_RENDER, 0, sizeof(R3D_MOD_RENDER));

    /* --- CPU array allocation (draw calls, groups, etc) --- */

    #define ALLOC_AND_ASSIGN(field, logfmt, ...) do { \
        void* _p = RL_CALLOC(R3D_RENDER_INITIAL_DRAW_CALL_RESERVE, sizeof(*R3D_MOD_RENDER.field)); \
        if (_p == NULL) { \
            R3D_TRACELOG(LOG_FATAL, "Failed to init render module; " logfmt, ##__VA_ARGS__); \
            goto fail; \
        } \
        R3D_MOD_RENDER.field = _p; \
    } while (0)

    ALLOC_AND_ASSIGN(clusters, "Render cluster array allocation failed");
    ALLOC_AND_ASSIGN(groupVisibility, "Render group visibility array allocation failed");
    ALLOC_AND_ASSIGN(callIndices, "Draw call indices array allocation failed");
    ALLOC_AND_ASSIGN(groups, "Render group array allocation failed");

    for (int i = 0; i < R3D_RENDER_LIST_COUNT; i++) {
        ALLOC_AND_ASSIGN(list[i].calls, "Draw call list[%i] allocation failed", i);
    }

    ALLOC_AND_ASSIGN(calls, "Draw call array allocation failed");
    ALLOC_AND_ASSIGN(groupIndices, "Render group indices array allocation failed");
    ALLOC_AND_ASSIGN(sortCache, "Sorting cache array allocation failed");

    #undef ALLOC_AND_ASSIGN

    R3D_MOD_RENDER.capacity = R3D_RENDER_INITIAL_DRAW_CALL_RESERVE;
    R3D_MOD_RENDER.activeCluster = -1;

    /* --- CPU free list allocation --- */

    #define ALLOC_FREELIST(field, cap_field, logmsg) do { \
        R3D_MOD_RENDER.field = RL_MALLOC(R3D_RENDER_INITIAL_FREE_LIST_RESERVE * sizeof(*R3D_MOD_RENDER.field)); \
        if (!R3D_MOD_RENDER.field) { \
            R3D_TRACELOG(LOG_FATAL, "Failed to init render module; " logmsg); \
            goto fail; \
        } \
        R3D_MOD_RENDER.cap_field = R3D_RENDER_INITIAL_FREE_LIST_RESERVE; \
    } while (0)

    ALLOC_FREELIST(freeVertices, freeVertexCapacity, "Free vertex list allocation failed");
    ALLOC_FREELIST(freeElements, freeElementCapacity, "Free element list allocation failed");

    #undef ALLOC_FREELIST

    /* --- Creation of the global VAO/VBO/EBO --- */

    glGenVertexArrays(1, &R3D_MOD_RENDER.globalVao);
    glBindVertexArray(R3D_MOD_RENDER.globalVao);

    glGenBuffers(1, &R3D_MOD_RENDER.globalVbo);
    glBindBuffer(GL_ARRAY_BUFFER, R3D_MOD_RENDER.globalVbo);
    glBufferData(GL_ARRAY_BUFFER,
        R3D_RENDER_INITIAL_VERTICES_RESERVE * sizeof(R3D_Vertex),
        NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &R3D_MOD_RENDER.globalEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, R3D_MOD_RENDER.globalEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        R3D_RENDER_INITIAL_ELEMENTS_RESERVE * sizeof(GLuint),
        NULL, GL_DYNAMIC_DRAW);

    R3D_MOD_RENDER.globalVertexCapacity  = R3D_RENDER_INITIAL_VERTICES_RESERVE;
    R3D_MOD_RENDER.globalElementCapacity = R3D_RENDER_INITIAL_ELEMENTS_RESERVE;

    /* --- Configuring vertex attributes --- */

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, texcoord));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, normal));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, color));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, tangent));

    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, boneIds));

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, weights));

    // Default values ​​for instance attributes (disabled)
    glVertexAttrib3f(10, 0.0f, 0.0f, 0.0f);
    glVertexAttrib4f(11, 0.0f, 0.0f, 0.0f, 1.0f);
    glVertexAttrib3f(12, 1.0f, 1.0f, 1.0f);
    glVertexAttrib4f(13, 1.0f, 1.0f, 1.0f, 1.0f);
    glVertexAttrib4f(14, 0.0f, 0.0f, 0.0f, 0.0f);

    glBindVertexArray(0);

    return true;

fail:
    r3d_render_quit();
    return false;
}

void r3d_render_quit(void)
{
    /* --- Delete global GL buffers --- */

    if (R3D_MOD_RENDER.globalVao) glDeleteVertexArrays(1, &R3D_MOD_RENDER.globalVao);
    if (R3D_MOD_RENDER.globalVbo) glDeleteBuffers(1, &R3D_MOD_RENDER.globalVbo);
    if (R3D_MOD_RENDER.globalEbo) glDeleteBuffers(1, &R3D_MOD_RENDER.globalEbo);

    /* --- Release CPU arrays --- */

    for (int i = 0; i < R3D_RENDER_LIST_COUNT; i++) {
        RL_FREE(R3D_MOD_RENDER.list[i].calls);
    }

    RL_FREE(R3D_MOD_RENDER.groupVisibility);
    RL_FREE(R3D_MOD_RENDER.groupIndices);
    RL_FREE(R3D_MOD_RENDER.callIndices);
    RL_FREE(R3D_MOD_RENDER.sortCache);
    RL_FREE(R3D_MOD_RENDER.clusters);
    RL_FREE(R3D_MOD_RENDER.groups);
    RL_FREE(R3D_MOD_RENDER.calls);

    /* --- Realease free lists --- */

    RL_FREE(R3D_MOD_RENDER.freeVertices);
    RL_FREE(R3D_MOD_RENDER.freeElements);
}

bool r3d_render_alloc_vertices(int count, int* outOffset)
{
    assert(outOffset != NULL);
    assert(count > 0);

    // First search the free list
    int offset = pop_free_range(
        R3D_MOD_RENDER.freeVertices,
        &R3D_MOD_RENDER.numFreeVertices,
        count
    );

    if (offset >= 0) {
        *outOffset = offset;
        return true;
    }

    // No free block, we extend from the end of the buffer
    int needed = R3D_MOD_RENDER.globalVertexCount + count;
    if (needed > R3D_MOD_RENDER.globalVertexCapacity) {
        if (!grow_global_vbo(needed)) {
            R3D_TRACELOG(LOG_FATAL, "r3d_render_alloc_vertices: VBO resize failed");
            return false;
        }
    }

    *outOffset = R3D_MOD_RENDER.globalVertexCount;
    R3D_MOD_RENDER.globalVertexCount += count;
    return true;
}

bool r3d_render_alloc_elements(int count, int* outOffset)
{
    assert(outOffset != NULL);
    assert(count > 0);

    int offset = pop_free_range(
        R3D_MOD_RENDER.freeElements,
        &R3D_MOD_RENDER.numFreeElements,
        count
    );

    if (offset >= 0) {
        *outOffset = offset;
        return true;
    }

    int needed = R3D_MOD_RENDER.globalElementCount + count;
    if (needed > R3D_MOD_RENDER.globalElementCapacity) {
        if (!grow_global_ebo(needed)) {
            R3D_TRACELOG(LOG_FATAL, "r3d_render_alloc_elements: EBO resize failed");
            return false;
        }
    }

    *outOffset = R3D_MOD_RENDER.globalElementCount;
    R3D_MOD_RENDER.globalElementCount += count;
    return true;
}

bool r3d_render_realloc_vertices(int* offset, int* count, int newCount, bool keepData)
{
    assert(offset != NULL && count != NULL);
    assert(*offset >= 0 && *count >= 0 && newCount > 0);

    if (newCount == *count) {
        return true;
    }

    // Reduction
    // The queue is released: the (count - newCount) end slots
    // are returned to the free list. No GPU copy is necessary
    if (newCount < *count) {
        r3d_render_free_vertices(*offset + newCount, *count - newCount);
        *count = newCount;
        return true;
    }

    // Enlargement
    int extra = newCount - *count;

    // Case 1: Extension in place if the free list
    // has a contiguous block immediately after
    if (try_extend_in_place(
            R3D_MOD_RENDER.freeVertices,
            &R3D_MOD_RENDER.numFreeVertices,
            *offset + *count, extra))
    {
        *count = newCount;
        return true;
    }

    // Case 2: Extension in place if we are at the end of
    // the buffer and there is still uncommitted capacity
    if (*offset + *count == R3D_MOD_RENDER.globalVertexCount) {
        int needed = R3D_MOD_RENDER.globalVertexCount + extra;
        if (needed > R3D_MOD_RENDER.globalVertexCapacity) {
            if (!grow_global_vbo(needed)) {
                R3D_TRACELOG(LOG_FATAL, "r3d_render_realloc_vertices: VBO resize failed");
                return false;
            }
        }
        R3D_MOD_RENDER.globalVertexCount += extra;
        *count = newCount;
        return true;
    }

    // Case 3: no contiguous space, we look for a new larger block,
    // copy the existing data into it, then free the old one
    int newOffset;
    if (!r3d_render_alloc_vertices(newCount, &newOffset)) {
        return false;
    }

    if (keepData) {
        copy_global_vertices(newOffset, *offset, *count);
    }

    r3d_render_free_vertices(*offset, *count);

    *offset = newOffset;
    *count  = newCount;
    return true;
}

bool r3d_render_realloc_elements(int* offset, int* count, int newCount, bool keepData)
{
    assert(offset != NULL && count != NULL);
    assert(*offset >= 0 && *count >= 0 && newCount > 0);

    if (newCount == *count) {
        return true;
    }

    // Reduction
    // The queue is released: the (count - newCount) end slots
    // are returned to the free list. No GPU copy is necessary
    if (newCount < *count) {
        r3d_render_free_elements(*offset + newCount, *count - newCount);
        *count = newCount;
        return true;
    }

    // Enlargement
    int extra = newCount - *count;

    // Case 1: Extension in place if the free list
    // has a contiguous block immediately after
    if (try_extend_in_place(
            R3D_MOD_RENDER.freeElements,
            &R3D_MOD_RENDER.numFreeElements,
            *offset + *count, extra))
    {
        *count = newCount;
        return true;
    }

    // Case 2: Extension in place if we are at the end of
    // the buffer and there is still uncommitted capacity
    if (*offset + *count == R3D_MOD_RENDER.globalElementCount) {
        int needed = R3D_MOD_RENDER.globalElementCount + extra;
        if (needed > R3D_MOD_RENDER.globalElementCapacity) {
            if (!grow_global_ebo(needed)) {
                R3D_TRACELOG(LOG_FATAL, "r3d_render_realloc_elements: EBO resize failed");
                return false;
            }
        }
        R3D_MOD_RENDER.globalElementCount += extra;
        *count = newCount;
        return true;
    }

    // Case 3: no contiguous space, we look for a new larger block,
    // copy the existing data into it, then free the old one
    int newOffset;
    if (!r3d_render_alloc_elements(newCount, &newOffset)) {
        return false;
    }

    if (keepData) {
        copy_global_elements(newOffset, *offset, *count);
    }

    r3d_render_free_elements(*offset, *count);

    *offset = newOffset;
    *count  = newCount;
    return true;
}

void r3d_render_free_vertices(int offset, int count)
{
    assert(offset >= 0 && count > 0);

    if (!push_free_range(
            &R3D_MOD_RENDER.freeVertices,
            &R3D_MOD_RENDER.numFreeVertices,
            &R3D_MOD_RENDER.freeVertexCapacity,
            offset, count))
    {
        R3D_TRACELOG(LOG_WARNING, "r3d_render_free_vertices: free list push failed (leak)");
        return;
    }

    coalesce_free_list(R3D_MOD_RENDER.freeVertices, &R3D_MOD_RENDER.numFreeVertices);
}

void r3d_render_free_elements(int offset, int count)
{
    assert(offset >= 0 && count > 0);

    if (!push_free_range(
            &R3D_MOD_RENDER.freeElements,
            &R3D_MOD_RENDER.numFreeElements,
            &R3D_MOD_RENDER.freeElementCapacity,
            offset, count))
    {
        R3D_TRACELOG(LOG_WARNING, "r3d_render_free_elements: free list push failed (leak)");
        return;
    }

    coalesce_free_list(R3D_MOD_RENDER.freeElements, &R3D_MOD_RENDER.numFreeElements);
}

void r3d_render_upload_vertices(int offset, const R3D_Vertex* verts, int count)
{
    assert(offset >= 0 && verts != NULL && count > 0);
    assert(offset + count <= R3D_MOD_RENDER.globalVertexCapacity);

    glBindBuffer(GL_ARRAY_BUFFER, R3D_MOD_RENDER.globalVbo);
    glBufferSubData(
        GL_ARRAY_BUFFER,
        offset * sizeof(R3D_Vertex),
        count * sizeof(R3D_Vertex),
        verts
    );
}

void r3d_render_upload_elements(int offset, const GLuint* indices, int count)
{
    assert(offset >= 0 && indices != NULL && count > 0);
    assert(offset + count <= R3D_MOD_RENDER.globalElementCapacity);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, R3D_MOD_RENDER.globalEbo);
    glBufferSubData(
        GL_ELEMENT_ARRAY_BUFFER,
        offset * sizeof(GLuint),
        count * sizeof(GLuint),
        indices
    );
}

void r3d_render_clear(void)
{
    for (int i = 0; i < R3D_RENDER_LIST_COUNT; i++) {
        R3D_MOD_RENDER.list[i].numCalls = 0;
    }

    R3D_MOD_RENDER.numClusters = 0;
    R3D_MOD_RENDER.numGroups = 0;
    R3D_MOD_RENDER.numCalls = 0;

    R3D_MOD_RENDER.groupCulled = false;
    R3D_MOD_RENDER.hasDeferred = false;
    R3D_MOD_RENDER.hasPrepass = false;
    R3D_MOD_RENDER.hasForward = false;
}

bool r3d_render_cluster_begin(BoundingBox aabb)
{
    if (R3D_MOD_RENDER.activeCluster >= 0) {
        return false;
    }

    if (R3D_MOD_RENDER.numClusters >= R3D_MOD_RENDER.capacity) {
        if (!growth_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Bad alloc on render cluster begin");
            return false;
        }
    }

    R3D_MOD_RENDER.activeCluster = R3D_MOD_RENDER.numClusters++;

    r3d_render_cluster_t* cluster = &R3D_MOD_RENDER.clusters[R3D_MOD_RENDER.activeCluster];
    cluster->visible = R3D_RENDER_VISBILITY_UNKNOWN;
    cluster->aabb = aabb;

    return true;
}

bool r3d_render_cluster_end(void)
{
    if (R3D_MOD_RENDER.activeCluster < 0) return false;
    R3D_MOD_RENDER.activeCluster = -1;
    return true;
}

void r3d_render_group_push(const r3d_render_group_t* group)
{
    if (R3D_MOD_RENDER.numGroups >= R3D_MOD_RENDER.capacity) {
        if (!growth_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Bad alloc on render group push");
            return;
        }
    }

    int groupIndex = R3D_MOD_RENDER.numGroups++;

    R3D_MOD_RENDER.groupVisibility[groupIndex] = (r3d_render_group_visibility_t) {
        .clusterIndex = R3D_MOD_RENDER.activeCluster,
        .visible = R3D_RENDER_VISBILITY_UNKNOWN
    };

    R3D_MOD_RENDER.callIndices[groupIndex] = (r3d_render_indices_t) {0};
    R3D_MOD_RENDER.groups[groupIndex] = *group;
}

void r3d_render_call_push(const r3d_render_call_t* call)
{
    if (R3D_MOD_RENDER.numCalls >= R3D_MOD_RENDER.capacity) {
        if (!growth_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Bad alloc on draw call push");
            return;
        }
    }

    // Get group and their call indices
    int groupIndex = get_last_group_index();
    r3d_render_group_t* group = &R3D_MOD_RENDER.groups[groupIndex];
    r3d_render_indices_t* indices = &R3D_MOD_RENDER.callIndices[groupIndex];

    // Get call index and set call group indices
    int callIndex = R3D_MOD_RENDER.numCalls++;
    if (indices->numCall == 0) {
        indices->firstCall = callIndex;
    }
    ++indices->numCall;

    // Set group index for this draw call
    R3D_MOD_RENDER.groupIndices[callIndex] = groupIndex;

    // Determine the draw call list
    r3d_render_list_enum_t list = R3D_RENDER_LIST_OPAQUE;
    if (r3d_render_is_decal(call)) list = R3D_RENDER_LIST_DECAL;
    else if (!r3d_render_is_opaque(call)) list = R3D_RENDER_LIST_TRANSPARENT;
    if (r3d_render_has_instances(group)) list += R3D_RENDER_LIST_NON_INST_COUNT;

    // Update internal flags
    if (r3d_render_is_deferred(call)) R3D_MOD_RENDER.hasDeferred = true;
    else if (r3d_render_is_prepass(call)) R3D_MOD_RENDER.hasPrepass = true;
    else if (r3d_render_is_forward(call)) R3D_MOD_RENDER.hasForward = true;

    // Push the draw call and its index to the list
    R3D_MOD_RENDER.calls[callIndex] = *call;
    int listIndex = R3D_MOD_RENDER.list[list].numCalls++;
    R3D_MOD_RENDER.list[list].calls[listIndex] = callIndex;
}

r3d_render_group_t* r3d_render_get_call_group(const r3d_render_call_t* call)
{
    int callIndex = get_draw_call_index(call);
    int groupIndex = R3D_MOD_RENDER.groupIndices[callIndex];
    r3d_render_group_t* group = &R3D_MOD_RENDER.groups[groupIndex];

    return group;
}

void r3d_render_cull_groups(const r3d_frustum_t* frustum)
{
    // Reset visibility states if groups were already culled in a previous pass
    if (R3D_MOD_RENDER.groupCulled) {
        for (int i = 0; i < R3D_MOD_RENDER.numGroups; i++) {
            R3D_MOD_RENDER.groupVisibility[i].visible = R3D_RENDER_VISBILITY_UNKNOWN;
        }
        for (int i = 0; i < R3D_MOD_RENDER.numClusters; i++) {
            R3D_MOD_RENDER.clusters[i].visible = R3D_RENDER_VISBILITY_UNKNOWN;
        }
    }
    R3D_MOD_RENDER.groupCulled = true;

    // Perform frustum culling for each group
    for (int i = 0; i < R3D_MOD_RENDER.numGroups; i++)
    {
        r3d_render_group_visibility_t* visibility = &R3D_MOD_RENDER.groupVisibility[i];
        const r3d_render_group_t* group = &R3D_MOD_RENDER.groups[i];

        // Branch 1: Group belongs to a cluster
        if (visibility->clusterIndex >= 0) {
            r3d_render_cluster_t* cluster = &R3D_MOD_RENDER.clusters[visibility->clusterIndex];

            // Test cluster once (shared by multiple groups)
            if (cluster->visible == R3D_RENDER_VISBILITY_UNKNOWN) {
                cluster->visible = is_aabb_visible(frustum, cluster->aabb);
            }

            // If cluster is visible, test the group
            if (cluster->visible == R3D_RENDER_VISBILITY_TRUE) {
                // For instanced: trust cluster visibility
                // For others: test group AABB individually
                if (r3d_render_has_instances(group)) visibility->visible = R3D_RENDER_VISBILITY_TRUE;
                else visibility->visible = is_obb_visible(frustum, group->obb);
            }
            else {
                visibility->visible = R3D_RENDER_VISBILITY_FALSE;
            }
        }
        // Branch 2: Group without cluster
        else {
            // For instanced: always visible
            // For others: test group AABB
            if (r3d_render_has_instances(group)) visibility->visible = R3D_RENDER_VISBILITY_TRUE;
            else visibility->visible = is_obb_visible(frustum, group->obb);
        }
    }
}

bool r3d_render_call_is_visible(const r3d_render_call_t* call, const r3d_frustum_t* frustum)
{
    // Get the draw call's parent group and its visibility state
    int callIndex = get_draw_call_index(call);
    int groupIndex = R3D_MOD_RENDER.groupIndices[callIndex];
    const r3d_render_group_t* group = &R3D_MOD_RENDER.groups[groupIndex];
    r3d_render_visibility_enum_t groupVisibility = R3D_MOD_RENDER.groupVisibility[groupIndex].visible;

    // If the group was already culled, reject immediately
    if (groupVisibility == R3D_RENDER_VISBILITY_FALSE) {
        return false;
    }

    // If the group passed culling, check if we can skip per-call testing
    if (groupVisibility == R3D_RENDER_VISBILITY_TRUE) {
        // Single-call groups were already tested at the group level
        if (R3D_MOD_RENDER.callIndices[groupIndex].numCall == 1) {
            return true;
        }
        // Instanced/skinned groups: trust the group-level test
        if (r3d_render_has_instances(group) || group->skinTexture > 0) {
            return true;
        }
        // Multi-call group: fall through to individual call testing
    }
    // If the group hasn't been tested yet, check instanced/skinned groups now
    else if (groupVisibility == R3D_RENDER_VISBILITY_UNKNOWN) {
        if (r3d_render_has_instances(group) || group->skinTexture > 0) {
            return is_obb_visible(frustum, group->obb);
        }
        // Regular multi-call group: fall through to individual call testing
    }

    // Test this specific draw call against the frustum
    return is_draw_call_visible(frustum, call, group->transform);
}

void r3d_render_sort_list(r3d_render_list_enum_t list, Vector3 viewPosition, r3d_render_sort_enum_t mode)
{
    G_sortViewPosition = viewPosition;

    int (*compare_func)(const void *a, const void *b) = NULL;
    r3d_render_list_t* drawList = &R3D_MOD_RENDER.list[list];

    switch (mode) {
    case R3D_RENDER_SORT_FRONT_TO_BACK:
        compare_func = compare_front_to_back;
        sort_fill_cache_front_to_back(list);
        break;
    case R3D_RENDER_SORT_BACK_TO_FRONT:
        compare_func = compare_back_to_front;
        sort_fill_cache_back_to_front(list);
        break;
    case R3D_RENDER_SORT_MATERIAL_ONLY:
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

void r3d_render_prepare_drawing(void)
{
    glBindVertexArray(R3D_MOD_RENDER.globalVao);
}

void r3d_render_draw(const r3d_render_call_t* call)
{
    GLenum primitive;
    r3d_render_range_t vertexRange;
    r3d_render_range_t indexRange;

    get_draw_call_info(call, &primitive, &vertexRange, &indexRange);

    if (indexRange.count == 0) {
        glDrawArrays(primitive, vertexRange.offset, vertexRange.count);
    }
    else {
        glDrawElementsBaseVertex(
            primitive,
            indexRange.count,
            GL_UNSIGNED_INT,
            (void*)(indexRange.offset * sizeof(GLuint)),
            vertexRange.offset
        );
    }
}

void r3d_render_draw_instanced(const r3d_render_call_t* call)
{
    GLenum primitive;
    r3d_render_range_t vertexRange;
    r3d_render_range_t indexRange;

    get_draw_call_info(call, &primitive, &vertexRange, &indexRange);

    const r3d_render_group_t* group = r3d_render_get_call_group(call);

    enable_instances(group->instances.buffers, group->instances.flags, group->instanceOffset);

    if (indexRange.count == 0) {
        glDrawArraysInstanced(primitive, vertexRange.offset, vertexRange.count, group->instanceCount);
    }
    else {
        glDrawElementsInstancedBaseVertex(
            primitive,
            indexRange.count,
            GL_UNSIGNED_INT,
            (void*)(indexRange.offset * sizeof(GLuint)),
            group->instanceCount,
            vertexRange.offset
        );
    }

    disable_instances(group->instances.flags);
}

void r3d_render_draw_shape(r3d_render_shape_enum_t shape)
{
    r3d_render_shape_t* s = &R3D_MOD_RENDER.shapes[shape];
    if (s->vertices.count == 0) {
        SHAPE_LOADERS[shape](s);
        glBindVertexArray(R3D_MOD_RENDER.globalVao);
    }

    if (s->elements.count > 0) {
        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            s->elements.count,
            GL_UNSIGNED_INT,
            (void*)(s->elements.offset * sizeof(GLuint)),
            s->vertices.offset
        );
    }
    else {
        glDrawArrays(GL_TRIANGLES, s->vertices.offset, s->vertices.count);
    }
}
