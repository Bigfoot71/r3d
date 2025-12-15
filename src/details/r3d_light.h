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

#ifndef R3D_LIGHT_H
#define R3D_LIGHT_H

#include <r3d/r3d_lighting.h>
#include <raylib.h>

/* === Types === */

typedef struct {
    R3D_ShadowUpdateMode mode;
    float frequencySec;
    float timerSec;
    bool shouldUpdate;
} r3d_shadow_update_conf_t;

typedef struct {
    unsigned int id;
    unsigned int depth;
    float texelSize;
    int resolution;
} r3d_shadow_map_t;

typedef struct {
    r3d_shadow_update_conf_t updateConf;
    r3d_shadow_map_t map;
    Matrix matVP;
    float softness;
    float depthBias;
    float slopeBias;
    bool enabled;
} r3d_shadow_t;

typedef struct {
    r3d_shadow_t shadow;
    Vector3 color;
    Vector3 position;
    Vector3 direction;
    float specular;
    float energy;
    float range;
    float near;
    float far;
    float attenuation;
    float innerCutOff;
    float outerCutOff;
    R3D_LightType type;
    bool enabled;
} r3d_light_t;

typedef struct {
    r3d_light_t* data;
    BoundingBox aabb;
} r3d_light_batched_t;

/* === Functions === */

void r3d_light_init(r3d_light_t* light, R3D_LightType type);

void r3d_light_create_shadow_map(r3d_light_t* light, int resolution);
void r3d_light_destroy_shadow_map(r3d_light_t* light);

void r3d_light_process_shadow_update(r3d_light_t* light);
void r3d_light_indicate_shadow_update(r3d_light_t* light);

BoundingBox r3d_light_get_bounding_box(const r3d_light_t* light);

void r3d_light_get_matrix_vp_dir(r3d_light_t* light, BoundingBox sceneBounds, Matrix* view, Matrix* proj);

Matrix r3d_light_get_matrix_view_spot(r3d_light_t* light);
Matrix r3d_light_get_matrix_proj_spot(r3d_light_t* light);

Matrix r3d_light_get_matrix_view_omni(r3d_light_t* light, int face);
Matrix r3d_light_get_matrix_proj_omni(r3d_light_t* light);

#endif // R3D_LIGHT_H
