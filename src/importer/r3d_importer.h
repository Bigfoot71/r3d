/* r3d_importer.h -- Module to manage model imports via assimp.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_IMPORTER_H
#define R3D_IMPORTER_H

#include <raylib.h>
#include <uthash.h>

#include <r3d/r3d_animation.h>
#include <r3d/r3d_material.h>
#include <r3d/r3d_skeleton.h>
#include <r3d/r3d_model.h>

#include <assimp/postprocess.h>
#include <assimp/quaternion.h>
#include <assimp/matrix4x4.h>
#include <assimp/cimport.h>
#include <assimp/vector2.h>
#include <assimp/vector3.h>
#include <assimp/color4.h>
#include <assimp/scene.h>

// ========================================
// MACROS
// ========================================

#define r3d_importer_cast(src) _Generic((src), \
    struct aiVector2D:    r3d_importer_cast_aivector2d_to_vector2, \
    struct aiVector3D:    r3d_importer_cast_aivector3d_to_vector3, \
    struct aiQuaternion:  r3d_importer_cast_aiquaternion_to_quaternion, \
    struct aiColor4D:     r3d_importer_cast_aicolor4d_to_color, \
    struct aiMatrix4x4:   r3d_importer_cast_aimatrix4x4_to_matrix \
)(src)

#define r3d_importer_cast_to_vector2(src) _Generic((src), \
    struct aiVector2D: r3d_importer_cast_aivector2d_to_vector2, \
    struct aiVector3D: r3d_importer_cast_aivector3d_to_vector2 \
)(src)

// ========================================
// SCENE IMPORTER
// ========================================

typedef struct {
    char name[128];          // Key
    int index;               // Value
    UT_hash_handle hh;       // Uthash handle
} r3d_bone_map_entry_t;

typedef struct {
    const struct aiScene* scene;
    r3d_bone_map_entry_t* boneMap;
    int boneCount;
} r3d_importer_t;

// ========================================
// TEXTURE CACHE
// ========================================

typedef enum {
    R3D_MAP_ALBEDO      = 0,
    R3D_MAP_EMISSION    = 1,
    R3D_MAP_ORM         = 2,
    R3D_MAP_NORMAL      = 3,
    R3D_MAP_COUNT
} r3d_importer_texture_map_t;

typedef struct r3d_importer_texture_cache r3d_importer_texture_cache_t;

// ========================================
// PUBLIC FUNCTIONS
// ========================================

/**
 * Create an Assimp importer and cache data used for loading
 * by reading from a file.
 */
bool r3d_importer_create_from_file(r3d_importer_t* importer, const char* filePath);

/**
 * Create an Assimp importer and cache data used for loading
 * by reading from memory.
 */
bool r3d_importer_create_from_memory(r3d_importer_t* importer, const void* data, uint32_t size, const char* hint);

/**
 * Release the Assimp importer along with all associated cached data.
 */
void r3d_importer_destroy(r3d_importer_t* importer);

/**
 * Return the index of a bone by its name, stored in a hash table.
 */
int r3d_importer_get_bone_index(const r3d_importer_t* importer, const char* name);

/**
 * Create a texture cache that loads all textures for all materials
 * This will spawn worker threads to load images in parallel, then
 * progressively upload them to GPU as they become ready
 */
r3d_importer_texture_cache_t* r3d_importer_load_texture_cache(const r3d_importer_t* importer, R3D_ColorSpace colorSpace, TextureFilter filter);

/**
 * Destroy the texture cache and free all unused resources
 */
void r3d_importer_unload_texture_cache(r3d_importer_texture_cache_t* cache);

/**
 * Get a texture for a specific material and map type
 * Returns NULL if the texture doesn't exist
 */
Texture2D* r3d_importer_get_loaded_texture(r3d_importer_texture_cache_t* cache, int materialIndex, r3d_importer_texture_map_t map);

/**
 * Load all meshes from the importer into the model
 * Returns true on success, false on failure
 */
bool r3d_importer_load_meshes(const r3d_importer_t* importer, R3D_Model* model);

/**
 * Process and create a skeleton from the imported scene
 * Returns NULL if the scene has no bones or on allocation failure
 * The returned skeleton must be freed by the caller
 */
bool r3d_importer_load_skeleton(const r3d_importer_t* importer, R3D_Skeleton* skeleton);

/**
 * Load all materials from the importer into the model
 * Returns true on success, false on failure
 */
bool r3d_importer_load_materials(const r3d_importer_t* importer, R3D_Model* model, r3d_importer_texture_cache_t* textureCache);

/**
 * Load all animations from the imported scene
 * Returns NULL if no animations are found or on error
 * The returned animation library must be freed by the caller
 */
bool r3d_importer_load_animations(const r3d_importer_t* importer, R3D_AnimationLib* animationLib);

// ========================================
// INLINE FUNCTIONS
// ========================================

static const struct aiAnimation* r3d_importer_get_animation(const r3d_importer_t* importer, int index)
{
    return importer->scene->mAnimations[index];
}

static const struct aiMaterial* r3d_importer_get_material(const r3d_importer_t* importer, int index)
{
    return importer->scene->mMaterials[index];
}

static const struct aiTexture* r3d_importer_get_texture(const r3d_importer_t* importer, int index)
{
    return importer->scene->mTextures[index];
}

static const struct aiMesh* r3d_importer_get_mesh(const r3d_importer_t* importer, int index)
{
    return importer->scene->mMeshes[index];
}

static const struct aiNode* r3d_importer_get_root(const r3d_importer_t* importer)
{
    return importer->scene->mRootNode;
}

static const struct aiScene* r3d_importer_get_scene(const r3d_importer_t* importer)
{
    return importer->scene;
}

static int r3d_importer_get_animation_count(const r3d_importer_t* importer)
{
    return importer->scene->mNumAnimations;
}

static int r3d_importer_get_material_count(const r3d_importer_t* importer)
{
    return importer->scene->mNumMaterials;
}

static int r3d_importer_get_texture_count(const r3d_importer_t* importer)
{
    return importer->scene->mNumTextures;
}

static int r3d_importer_get_mesh_count(const r3d_importer_t* importer)
{
    return importer->scene->mNumMeshes;
}

static int r3d_importer_get_bone_count(const r3d_importer_t* importer)
{
    return importer->boneCount;
}

static bool r3d_importer_is_valid(const r3d_importer_t* importer)
{
    return importer && importer->scene;
}

// ========================================
// ASSIMP CAST HELPERS
// ========================================

static inline Vector2 r3d_importer_cast_aivector2d_to_vector2(struct aiVector2D src)
{
    return (Vector2) { src.x, src.y };
}

static inline Vector2 r3d_importer_cast_aivector3d_to_vector2(struct aiVector3D src)
{
    return (Vector2) { src.x, src.y };
}

static inline Vector3 r3d_importer_cast_aivector3d_to_vector3(struct aiVector3D src)
{
    return (Vector3) { src.x, src.y, src.z };
}

static inline Quaternion r3d_importer_cast_aiquaternion_to_quaternion(struct aiQuaternion src)
{
    return (Quaternion) { src.x, src.y, src.z, src.w };
}

static inline Color r3d_importer_cast_aicolor4d_to_color(struct aiColor4D src)
{
    return (Color) {
        (uint8_t)(fminf(1.0f, fmaxf(0.0f, src.r)) * 255),
        (uint8_t)(fminf(1.0f, fmaxf(0.0f, src.g)) * 255),
        (uint8_t)(fminf(1.0f, fmaxf(0.0f, src.b)) * 255),
        (uint8_t)(fminf(1.0f, fmaxf(0.0f, src.a)) * 255)
    };
}

static inline Matrix r3d_importer_cast_aimatrix4x4_to_matrix(struct aiMatrix4x4 src)
{
    return (Matrix) {
        src.a1, src.a2, src.a3, src.a4,
        src.b1, src.b2, src.b3, src.b4,
        src.c1, src.c2, src.c3, src.c4,
        src.d1, src.d2, src.d3, src.d4,
    };
}

#endif // R3D_IMPORTER_H
