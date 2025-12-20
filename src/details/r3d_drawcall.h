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

#ifndef R3D_DETAILS_DRAWCALL_H
#define R3D_DETAILS_DRAWCALL_H

#include "r3d/r3d_animation.h"
#include <r3d/r3d_model.h>
#include <raylib.h>
#include <stddef.h>

/* === Types === */

typedef enum {
    R3D_DRAWCALL_RENDER_DEFERRED,
    R3D_DRAWCALL_RENDER_FORWARD
} r3d_drawcall_render_mode_e;

typedef struct {

    R3D_Mesh mesh;                      //< Mesh to render
    Matrix transform;
    R3D_Material material;
    R3D_Skeleton skeleton;                  //< Skeleton that contains the bind pose (if any)
    const R3D_AnimationPlayer* player;      //< Animation player used. (can be null, skeleton bind pose can be used)
    r3d_drawcall_render_mode_e renderMode;

    struct {
        const Matrix* transforms;
        const Color* colors;
        BoundingBox allAabb;
        size_t transStride;
        size_t colStride;
        size_t count;
    } instanced;

} r3d_drawcall_t;

/* === Functions === */

void r3d_drawcall_sort_front_to_back(r3d_drawcall_t* calls, size_t count);
void r3d_drawcall_sort_back_to_front(r3d_drawcall_t* calls, size_t count);

bool r3d_drawcall_geometry_is_visible(const r3d_drawcall_t* call);
bool r3d_drawcall_instanced_geometry_is_visible(const r3d_drawcall_t* call);

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call, bool shadow, const Matrix* matVP);
void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call, bool shadow, const Matrix* matVP);

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call, const Matrix* matVP);
void r3d_drawcall_raster_forward(const r3d_drawcall_t* call, const Matrix* matVP);
void r3d_drawcall_raster_decal(const r3d_drawcall_t* call, const Matrix* matVP);

#endif // R3D_DETAILS_DRAWCALL_H
