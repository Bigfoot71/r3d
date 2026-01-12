/* r3d_primitive.c -- Internal R3D primitive drawing module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_primitive.h"

#include <r3d/r3d_mesh_data.h>
#include <stddef.h>
#include <string.h>
#include <glad.h>

// ========================================
// MODULE STATE
// ========================================

typedef struct {
    GLuint vao, vbo, ebo;
    int indexCount;
} primitive_buffer_t;

static struct {
    primitive_buffer_t buffers[R3D_PRIMITIVE_COUNT];
} R3D_MOD_PRIMITIVE;

// ========================================
// HELPER FUNCTION
// ========================================

static void setup_vertex_attribs(void)
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

static void load_mesh(primitive_buffer_t* buf, const R3D_Vertex* verts, int vertCount, const GLubyte* indices, int idxCount)
{
    glGenVertexArrays(1, &buf->vao);
    glGenBuffers(1, &buf->vbo);
    glGenBuffers(1, &buf->ebo);
    
    glBindVertexArray(buf->vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, buf->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertCount * sizeof(R3D_Vertex), verts, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxCount * sizeof(GLubyte), indices, GL_STATIC_DRAW);
    
    buf->indexCount = idxCount;
    
    setup_vertex_attribs();
}

// ========================================
// PRIMITIVE LOADERS
// ========================================

typedef void (*primitive_loader_func)(primitive_buffer_t*);

static void load_dummy(primitive_buffer_t* dummy);
static void load_quad(primitive_buffer_t* quad);
static void load_cube(primitive_buffer_t* cube);

static const primitive_loader_func LOADERS[] = {
    [R3D_PRIMITIVE_DUMMY] = load_dummy,
    [R3D_PRIMITIVE_QUAD] = load_quad,
    [R3D_PRIMITIVE_CUBE] = load_cube,
};

void load_dummy(primitive_buffer_t* buf)
{
    glGenVertexArrays(1, &buf->vao);
    glBindVertexArray(buf->vao);
    buf->indexCount = 0;
}

void load_quad(primitive_buffer_t* buf)
{
    static const R3D_Vertex VERTS[] = {
        {{-0.5f, 0.5f, 0}, {0, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{-0.5f,-0.5f, 0}, {0, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f, 0.5f, 0}, {1, 1}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
        {{ 0.5f,-0.5f, 0}, {1, 0}, {0, 0, 1}, {255, 255, 255, 255}, {1, 0, 0, 1}},
    };
    static const GLubyte INDICES[] = {0, 1, 2, 1, 3, 2};
    
    load_mesh(buf, VERTS, 4, INDICES, 6);
}

void load_cube(primitive_buffer_t* buf)
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
    
    load_mesh(buf, VERTS, 24, INDICES, 36);
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_primitive_init(void)
{
    memset(&R3D_MOD_PRIMITIVE, 0, sizeof(R3D_MOD_PRIMITIVE));
    return true;
}

void r3d_primitive_quit(void)
{
    for (int i = 0; i < R3D_PRIMITIVE_COUNT; i++) {
        primitive_buffer_t* buf = &R3D_MOD_PRIMITIVE.buffers[i];
        if (buf->vao) glDeleteVertexArrays(1, &buf->vao);
        if (buf->vbo) glDeleteBuffers(1, &buf->vbo);
        if (buf->ebo) glDeleteBuffers(1, &buf->ebo);
    }
}

void r3d_primitive_draw(r3d_primitive_t primitive)
{
    primitive_buffer_t* buf = &R3D_MOD_PRIMITIVE.buffers[primitive];

    // NOTE: The loader leaves the vao bound
    if (buf->vao == 0) LOADERS[primitive](buf);
    else glBindVertexArray(buf->vao);

    if (buf->indexCount > 0) {
        glDrawElements(GL_TRIANGLES, buf->indexCount, GL_UNSIGNED_BYTE, 0);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, 3); // dummy
    }
}

void r3d_primitive_draw_instanced(r3d_primitive_t primitive, const R3D_InstanceBuffer* instances, int count)
{
    primitive_buffer_t* buf = &R3D_MOD_PRIMITIVE.buffers[primitive];

    // NOTE: The loader leaves the vao bound
    if (buf->vao == 0) LOADERS[primitive](buf);
    else glBindVertexArray(buf->vao);

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

    if (buf->indexCount > 0) {
        glDrawElementsInstanced(GL_TRIANGLES, buf->indexCount, GL_UNSIGNED_BYTE, 0, count);
    }
    else {
        glDrawArraysInstanced(GL_TRIANGLES, 0, 3, count);
    }
}
