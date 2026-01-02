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
// INTERNAL STATE
// ========================================

static R3D_Material G_DefaultMaterial = R3D_MATERIAL_BASE;

// ========================================
// PUBLIC API
// ========================================

R3D_Material R3D_GetDefaultMaterial(void)
{
    return G_DefaultMaterial;
}

void R3D_SetDefaultMaterial(R3D_Material material)
{
    G_DefaultMaterial = material;
}

void R3D_UnloadMaterial(R3D_Material material)
{
#define UNLOAD_TEXTURE_IF_VALID(id) \
    do { \
        if ((id) != 0 && !r3d_texture_is_default(id)) { \
            glDeleteTextures(1, &id); \
        } \
    } while (0)

    UNLOAD_TEXTURE_IF_VALID(material.albedo.texture.id);
    UNLOAD_TEXTURE_IF_VALID(material.emission.texture.id);
    UNLOAD_TEXTURE_IF_VALID(material.normal.texture.id);
    UNLOAD_TEXTURE_IF_VALID(material.orm.texture.id);

#undef UNLOAD_TEXTURE_IF_VALID
}
