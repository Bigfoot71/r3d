/* r3d_material.h -- R3D Material Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_material.h>
#include <r3d/r3d_utils.h>
#include <glad.h>

#include "./modules/r3d_texture.h"

// ========================================
// PUBLIC API
// ========================================

R3D_Material R3D_GetDefaultMaterial(void)
{
    R3D_Material material = { 0 };

    // Albedo map
    material.albedo.texture = R3D_GetWhiteTexture();
    material.albedo.color = WHITE;

    // Emission map
    material.emission.texture = R3D_GetWhiteTexture();
    material.emission.color = WHITE;
    material.emission.energy = 0.0f;

    // Normal map
    material.normal.texture = R3D_GetNormalTexture();
    material.normal.scale = 1.0f;

    // ORM map
    material.orm.texture = R3D_GetWhiteTexture();
    material.orm.occlusion = 1.0f;
    material.orm.roughness = 1.0f;
    material.orm.metalness = 0.0f;

    // Misc
    material.transparencyMode = R3D_TRANSPARENCY_DISABLED;
    material.billboardMode = R3D_BILLBOARD_DISABLED;
    material.blendMode = R3D_BLEND_MIX;
    material.cullMode = R3D_CULL_BACK;
    material.uvOffset = (Vector2) {0.0f, 0.0f};
    material.uvScale = (Vector2) {1.0f, 1.0f};
    material.alphaCutoff = 0.01f;

    return material;
}

void R3D_UnloadMaterial(const R3D_Material* material)
{
#define UNLOAD_TEXTURE_IF_VALID(id) \
    do { \
        if ((id) != 0 && !r3d_texture_is_default(id)) { \
            glDeleteTextures(1, &id); \
        } \
    } while (0)

    UNLOAD_TEXTURE_IF_VALID(material->albedo.texture.id);
    UNLOAD_TEXTURE_IF_VALID(material->emission.texture.id);
    UNLOAD_TEXTURE_IF_VALID(material->normal.texture.id);
    UNLOAD_TEXTURE_IF_VALID(material->orm.texture.id);

#undef UNLOAD_TEXTURE_IF_VALID
}
