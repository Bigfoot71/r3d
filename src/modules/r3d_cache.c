/* r3d_draw.c -- Internal R3D cache module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_cache.h"
#include "r3d/r3d_environment.h"
#include <raymath.h>

// ========================================
// MODULE STATE
// ========================================

struct r3d_cache R3D_MOD_CACHE;

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_cache_init(R3D_Flags flags)
{
    R3D_MOD_CACHE.matCubeViews[0] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  1.0f,  0.0f,  0.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });
    R3D_MOD_CACHE.matCubeViews[1] = MatrixLookAt((Vector3) { 0 }, (Vector3) { -1.0f,  0.0f,  0.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });
    R3D_MOD_CACHE.matCubeViews[2] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f,  1.0f,  0.0f }, (Vector3) { 0.0f,  0.0f,  1.0f });
    R3D_MOD_CACHE.matCubeViews[3] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f, -1.0f,  0.0f }, (Vector3) { 0.0f,  0.0f, -1.0f });
    R3D_MOD_CACHE.matCubeViews[4] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f,  0.0f,  1.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });
    R3D_MOD_CACHE.matCubeViews[5] = MatrixLookAt((Vector3) { 0 }, (Vector3) {  0.0f,  0.0f, -1.0f }, (Vector3) { 0.0f, -1.0f,  0.0f });

    R3D_MOD_CACHE.environment = R3D_ENVIRONMENT_BASE;

    R3D_MOD_CACHE.textureFilter = TEXTURE_FILTER_TRILINEAR;
    R3D_MOD_CACHE.layers = R3D_LAYER_ALL;
    R3D_MOD_CACHE.state = flags;

    return true;
}

void r3d_cache_quit(void)
{
    (void)0; // do nothing for now...
}
