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

#include "./r3d_primitives.h"

#include <r3d/r3d_mesh_data.h>
#include <stddef.h>
#include <glad.h>

r3d_primitive_t r3d_primitive_load_quad(void)
{
    static const R3D_Vertex VERTICES[] = {
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
    };

    static const GLuint INDICES[] = {
        0, 1, 2,
        1, 3, 2
    };

    r3d_primitive_t quad = {0};

    glGenVertexArrays(1, &quad.vao);
    glGenBuffers(1, &quad.vbo);
    glGenBuffers(1, &quad.ebo);

    glBindVertexArray(quad.vao);

    glBindBuffer(GL_ARRAY_BUFFER, quad.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES), VERTICES, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(INDICES), INDICES, GL_STATIC_DRAW);

    quad.indexCount = sizeof(INDICES) / sizeof(INDICES[0]);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, position));

    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, texcoord));

    glEnableVertexAttribArray(2); // normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, normal));

    glEnableVertexAttribArray(3); // color
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, color));

    glEnableVertexAttribArray(4); // tangent
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, tangent));

    glBindVertexArray(0);

    return quad;
}

r3d_primitive_t r3d_primitive_load_cube(void)
{
    static const R3D_Vertex VERTICES[] = {
        // Front face (Z+) - tangent points right (X+)
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f,  1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 0: Front top-left
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f,  1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 1: Front bottom-left
        {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f,  1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 2: Front top-right
        {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f,  1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 3: Front bottom-right

        // Back face (Z-) - tangent points left (X-)
        {{-1.0f,  1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 4: Back top-left
        {{-1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 5: Back bottom-left
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 6: Back top-right
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 7: Back bottom-right

        // Left face (X-) - tangent points back (Z-)
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 8: Left top-back
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 9: Left bottom-back
        {{-1.0f,  1.0f,  1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 10: Left top-front
        {{-1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 11: Left bottom-front

        // Right face (X+) - tangent points forward (Z+)
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, { 1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 12: Right top-front
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 13: Right bottom-front
        {{ 1.0f,  1.0f, -1.0f}, {1.0f, 1.0f}, { 1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 14: Right top-back
        {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, { 1.0f, 0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 15: Right bottom-back

        // Top face (Y+) - tangent points right (X+)
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f,  1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 16: Top back-left
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f}, {0.0f,  1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 17: Top front-left
        {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f,  1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 18: Top back-right
        {{ 1.0f,  1.0f,  1.0f}, {1.0f, 1.0f}, {0.0f,  1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 19: Top front-right

        // Bottom face (Y-) - tangent points right (X+)
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 20: Bottom front-left
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 21: Bottom back-left
        {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 22: Bottom front-right
        {{ 1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 23: Bottom back-right
    };

    static const GLubyte INDICES[] = {
        0, 1, 2, 2, 1, 3,
        4, 5, 6, 6, 5, 7,
        8, 9, 10, 10, 9, 11,
        12, 13, 14, 14, 13, 15,
        16, 17, 18, 18, 17, 19,
        20, 21, 22, 22, 21, 23
    };

    r3d_primitive_t cube = { 0 };

    glGenVertexArrays(1, &cube.vao);
    glGenBuffers(1, &cube.vbo);
    glGenBuffers(1, &cube.ebo);

    glBindVertexArray(cube.vao);

    glBindBuffer(GL_ARRAY_BUFFER, cube.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES), VERTICES, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cube.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(INDICES), INDICES, GL_STATIC_DRAW);

    cube.indexCount = sizeof(INDICES) / sizeof(INDICES[0]);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, position));

    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, texcoord));

    glEnableVertexAttribArray(2); // normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, normal));

    glEnableVertexAttribArray(3); // color
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, color));

    glEnableVertexAttribArray(4); // tangent
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(R3D_Vertex), (void*)offsetof(R3D_Vertex, tangent));

    glBindVertexArray(0);

    return cube;
}

void r3d_primitive_unload(const r3d_primitive_t* primitive)
{
    glDeleteBuffers(2, (GLuint[2]){primitive->vbo, primitive->ebo});
    glDeleteVertexArrays(1, &primitive->vao);
}

void r3d_primitive_draw(const r3d_primitive_t* primitive)
{
    glBindVertexArray(primitive->vao);
    glDrawElements(GL_TRIANGLES, primitive->indexCount, GL_UNSIGNED_BYTE, NULL);
    glBindVertexArray(0);
}

void r3d_primitive_draw_instanced(const r3d_primitive_t* primitive, int instances)
{
    glBindVertexArray(primitive->vao);
    glDrawElementsInstanced(GL_TRIANGLES, primitive->indexCount, GL_UNSIGNED_BYTE, NULL, instances);
    glBindVertexArray(0);
}
