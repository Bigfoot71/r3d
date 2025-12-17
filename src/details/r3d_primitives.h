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

#ifndef R3D_DETAIL_PRIMITIVES_H
#define R3D_DETAIL_PRIMITIVES_H

/* === Structs === */

typedef struct {
    unsigned int vao;
    unsigned int vbo;
    unsigned int ebo;
    int indexCount;
} r3d_primitive_t;

/* === Functions === */

r3d_primitive_t r3d_primitive_load_quad(void);
r3d_primitive_t r3d_primitive_load_cube(void);

void r3d_primitive_unload(const r3d_primitive_t* primitive);

void r3d_primitive_draw_instanced(const r3d_primitive_t* primitive, int instances);
void r3d_primitive_draw(const r3d_primitive_t* primitive);

#endif // R3D_DETAIL_PRIMITIVES_H
