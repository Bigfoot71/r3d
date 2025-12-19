/* r3d_mod_primitive.h -- Internal R3D primitive drawing module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_PRIMTIVE_H
#define R3D_MODULE_PRIMTIVE_H

#include <raylib.h>

// ========================================
// PRIMITIVE ENUM
// ========================================

typedef enum {
    R3D_PRIMITIVE_DUMMY,        //< Calls glDrawArrays with 3 vertices without an attached VBO/EBO
    R3D_PRIMITIVE_QUAD,         //< Draws a quad with dimensions 2.0 (-1.0 .. +1.0)
    R3D_PRIMITIVE_CUBE,         //< Draws a cube with dimensions 2.0 (-1.0 .. +1.0)
    R3D_PRIMITIVE_COUNT
} r3d_primitive_t;

// ========================================
// HELPER MACROS
// ========================================

#define R3D_PRIMITIVE_DRAW_SCREEN() do {        \
    r3d_primitive_draw(R3D_PRIMITIVE_DUMMY);    \
} while(0)

#define R3D_PRIMITIVE_DRAW_QUAD() do {          \
    r3d_primitive_draw(R3D_PRIMITIVE_QUAD);     \
} while(0)

#define R3D_PRIMITIVE_DRAW_CUBE() do {          \
    r3d_primitive_draw(R3D_PRIMITIVE_CUBE);     \
} while(0)

// ========================================
// PRIMITIVE FUNCTIONS
// ========================================

void r3d_primitive_draw(r3d_primitive_t primitive);

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_mod_primitive_init(void);
void r3d_mod_primitive_quit(void);

#endif // R3D_MODULE_PRIMTIVE_H
