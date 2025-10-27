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

#include <stddef.h>
#include <raylib.h>
#include <r3d.h>

/* === Types === */

typedef enum {
    R3D_DRAWCALL_GEOMETRY_MODEL,    //< Simple meshes are also considered as model here
    R3D_DRAWCALL_GEOMETRY_SPRITE
} r3d_drawcall_geometry_e;

typedef enum {
    R3D_DRAWCALL_RENDER_DEFERRED,
    R3D_DRAWCALL_RENDER_FORWARD
} r3d_drawcall_render_mode_e;

typedef struct {

    Matrix transform;
    R3D_Material material;
    R3D_ShadowCastMode shadowCastMode;

    r3d_drawcall_geometry_e geometryType;
    r3d_drawcall_render_mode_e renderMode;

    union {

        struct {
            const R3D_Mesh* mesh;               //< Mesh to render
            const R3D_ModelAnimation* anim;     //< Animation to apply to the mesh (can be NULL)
            const Matrix* boneOffsets;          //< Bone offset matrices from the R3D_Model
            const Matrix* boneOverride;         //< Bone override matrices for user supplied animation logic  
            int frame;                          //< Animation frame to apply to the mesh
        } model;

        struct {
            Vector3 quad[4];    //< Used only to represent the sprite in world space
        } sprite;

    } geometry;

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

void r3d_drawcall_update_model_animation(const r3d_drawcall_t* call);

void r3d_drawcall_raster_depth(const r3d_drawcall_t* call, bool forward, bool shadow, const Matrix* matVP);
void r3d_drawcall_raster_depth_cube(const r3d_drawcall_t* call, bool forward, bool shadow, const Matrix* matVP);

void r3d_drawcall_raster_geometry(const r3d_drawcall_t* call, const Matrix* matVP);
void r3d_drawcall_raster_forward(const r3d_drawcall_t* call, const Matrix* matVP);

#endif // R3D_DETAILS_DRAWCALL_H
