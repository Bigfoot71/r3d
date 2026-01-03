#ifndef R3D_CUBEMAP_H
#define R3D_CUBEMAP_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

// ========================================
// ENUM TYPES
// ========================================

typedef enum R3D_CubemapLayout {
    R3D_CUBEMAP_LAYOUT_AUTO_DETECT = 0,         // Automatically detect layout type
    R3D_CUBEMAP_LAYOUT_LINE_VERTICAL,           // Layout is defined by a vertical line with faces
    R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL,         // Layout is defined by a horizontal line with faces
    R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR,     // Layout is defined by a 3x4 cross with cubemap faces
    R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE,     // Layout is defined by a 4x3 cross with cubemap faces
    R3D_CUBEMAP_LAYOUT_PANORAMA
} R3D_CubemapLayout;

// ========================================
// STRUCT TYPES
// ========================================

typedef struct R3D_Cubemap {
    uint32_t texture;
    uint32_t fbo;
    int size;
} R3D_Cubemap;

// ========================================
// PUBLIC API
// ========================================

R3DAPI R3D_Cubemap R3D_LoadCubemapEmpty(int size);
R3DAPI R3D_Cubemap R3D_LoadCubemap(const char* fileName, R3D_CubemapLayout layout);
R3DAPI R3D_Cubemap R3D_LoadCubemapFromImage(Image image, R3D_CubemapLayout layout);

R3DAPI void R3D_UnloadCubemap(R3D_Cubemap cubemap);

#endif // R3D_CUBEMAP_H
