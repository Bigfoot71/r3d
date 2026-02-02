/* r3d_decal.c -- R3D Decal Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_decal.h>

 // ========================================
 // PUBLIC API
 // ========================================

void R3D_UnloadDecalMaps(R3D_Decal decal)
{
    R3D_UnloadAlbedoMap(decal.albedo);
    R3D_UnloadEmissionMap(decal.emission);
    R3D_UnloadOrmMap(decal.orm);
    R3D_UnloadNormalMap(decal.normal);
}