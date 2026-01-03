#ifndef R3D_ENVIRONMENT_MAP_H
#define R3D_ENVIRONMENT_MAP_H

#include "./r3d_cubemap.h"
#include <raylib.h>

// ========================================
// STRUCT TYPES
// ========================================

typedef struct R3D_EnvironmentMap {
    R3D_Cubemap irradiance;
    R3D_Cubemap prefilter;
} R3D_EnvironmentMap;

// ========================================
// PUBLIC API
// ========================================

R3DAPI R3D_EnvironmentMap R3D_GenEnvironmentMap(R3D_Cubemap cubemap);
R3DAPI void R3D_UpdateEnvironmentMap(R3D_EnvironmentMap envmap, R3D_Cubemap cubemap);

#endif // R3D_ENVIRONMENT_MAP_H
