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
        0,1,2, 2,1,3,   4,5,6, 6,5,7,   8,9,10, 10,9,11,
        12,13,14, 14,13,15,   16,17,18, 18,17,19,   20,21,22, 22,21,23
    };
    
    load_mesh(buf, VERTS, 24, INDICES, 36);
}

// ========================================
// PUBLIC API
// ========================================

void r3d_primitive_draw(r3d_primitive_t primitive)
{
    primitive_buffer_t* buf = &R3D_MOD_PRIMITIVE.buffers[primitive];

    // NOTE: The loader leaves the vao bound
    if (buf->vao == 0) LOADERS[primitive](buf);
    else glBindVertexArray(buf->vao);

    if (buf->indexCount > 0) {
        glDrawElements(GL_TRIANGLES, buf->indexCount, GL_UNSIGNED_BYTE, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, 3); // dummy
    }
}

void r3d_primitive_draw_instanced(r3d_primitive_t primitive, const void* transforms, size_t transStride, 
                                   const void* colors, size_t colStride, size_t instanceCount,
                                   int locInstanceModel, int locInstanceColor)
{
    // NOTE: All this mess here will be reviewed with the instance buffers

    primitive_buffer_t* buf = &R3D_MOD_PRIMITIVE.buffers[primitive];

    // NOTE: The loader leaves the vao bound
    if (buf->vao == 0) LOADERS[primitive](buf);
    else glBindVertexArray(buf->vao);

    unsigned int vboTransforms = 0;
    unsigned int vboColors = 0;

    // Enable the attribute for the transformation matrix (decomposed into 4 vec4 vectors)
    if (locInstanceModel >= 0 && transforms) {
        size_t stride = (transStride == 0) ? sizeof(Matrix) : transStride;

        // Create and bind VBO for transforms
        glGenBuffers(1, &vboTransforms);
        glBindBuffer(GL_ARRAY_BUFFER, vboTransforms);
        glBufferData(GL_ARRAY_BUFFER, instanceCount * stride, transforms, GL_DYNAMIC_DRAW);

        for (int i = 0; i < 4; i++) {
            glEnableVertexAttribArray(locInstanceModel + i);
            glVertexAttribPointer(locInstanceModel + i, 4, GL_FLOAT, GL_FALSE, (int)stride, (void*)(i * sizeof(Vector4)));
            glVertexAttribDivisor(locInstanceModel + i, 1);
        }
    }

    // Handle per-instance colors if available
    if (locInstanceColor >= 0 && colors) {
        size_t stride = (colStride == 0) ? sizeof(Color) : colStride;

        // Create and bind VBO for colors
        glGenBuffers(1, &vboColors);
        glBindBuffer(GL_ARRAY_BUFFER, vboColors);
        glBufferData(GL_ARRAY_BUFFER, instanceCount * stride, colors, GL_DYNAMIC_DRAW);

        glEnableVertexAttribArray(locInstanceColor);
        glVertexAttribPointer(locInstanceColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, (int)stride, (void*)0);
        glVertexAttribDivisor(locInstanceColor, 1);
    }

    // Draw the geometry (instanced)
    if (buf->indexCount > 0) {
        glDrawElementsInstanced(GL_TRIANGLES, buf->indexCount, GL_UNSIGNED_BYTE, 0, (int)instanceCount);
    } else {
        glDrawArraysInstanced(GL_TRIANGLES, 0, 3, (int)instanceCount);
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

bool r3d_mod_primitive_init(void)
{
    memset(&R3D_MOD_PRIMITIVE, 0, sizeof(R3D_MOD_PRIMITIVE));
    return true;
}

void r3d_mod_primitive_quit(void)
{
    for (int i = 0; i < R3D_PRIMITIVE_COUNT; i++) {
        primitive_buffer_t* buf = &R3D_MOD_PRIMITIVE.buffers[i];
        if (buf->vao) glDeleteVertexArrays(1, &buf->vao);
        if (buf->vbo) glDeleteBuffers(1, &buf->vbo);
        if (buf->ebo) glDeleteBuffers(1, &buf->ebo);
    }
}
