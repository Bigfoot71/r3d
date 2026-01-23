/* r3d_model.h -- R3D Model Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODEL_H
#define R3D_MODEL_H

#include "./r3d_material.h"
#include "./r3d_skeleton.h"
#include "./r3d_platform.h"
#include "./r3d_importer.h"
#include "./r3d_mesh.h"

/**
 * @defgroup Model
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a complete 3D model with meshes and materials.
 *
 * Contains multiple meshes and their associated materials, along with animation and bounding information.
 */
typedef struct R3D_Model {

    R3D_Mesh* meshes;                   ///< Array of meshes composing the model.
    R3D_MeshData* meshData;             ///< Array of meshes data in RAM (optional, can be NULL).
    R3D_Material* materials;            ///< Array of materials used by the model.
    int* meshMaterials;                 ///< Array of material indices, one per mesh.

    int meshCount;                      ///< Number of meshes.
    int materialCount;                  ///< Number of materials.

    BoundingBox aabb;                   ///< Axis-Aligned Bounding Box encompassing the whole model.
    R3D_Skeleton skeleton;              ///< Skeleton hierarchy and bind pose used for skinning (NULL if non-skinned).

} R3D_Model;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load a 3D model from a file.
 *
 * Loads a 3D model from the specified file path. Supports various 3D file formats
 * and automatically parses meshes, materials, and texture references.
 *
 * @param filePath Path to the 3D model file to load.
 *
 * @return Loaded model structure containing meshes and materials.
 */
R3DAPI R3D_Model R3D_LoadModel(const char* filePath);

/**
 * @brief Load a 3D model from a file with import flags.
 *
 * Extended version of R3D_LoadModel() allowing control over the import
 * process through additional flags.
 *
 * @param filePath Path to the 3D model file to load.
 * @param flags Importer behavior flags.
 *
 * @return Loaded model structure containing meshes and materials.
 */
R3DAPI R3D_Model R3D_LoadModelEx(const char* filePath, R3D_ImportFlags flags);

/**
 * @brief Load a 3D model from memory buffer.
 *
 * Loads a 3D model from a memory buffer containing the file data.
 * Useful for loading models from embedded resources or network streams.
 *
 * @param data Pointer to the memory buffer containing the model data.
 * @param size Size of the data buffer in bytes.
 * @param hint Hint on the model format (can be NULL).
 *
 * @return Loaded model structure containing meshes and materials.
 *
 * @note External dependencies (e.g., textures or linked resources) are not supported.
 *       The model data must be fully self-contained. Use embedded formats like .glb to ensure compatibility.
 */
R3DAPI R3D_Model R3D_LoadModelFromMemory(const void* data, unsigned int size, const char* hint);

/**
 * @brief Load a 3D model from a memory buffer with import flags.
 *
 * Extended version of R3D_LoadModelFromMemory() allowing control over
 * the import process through additional flags.
 *
 * @param data Pointer to the memory buffer containing the model data.
 * @param size Size of the data buffer in bytes.
 * @param hint Hint on the model format (can be NULL).
 * @param flags Importer behavior flags.
 *
 * @return Loaded model structure containing meshes and materials.
 *
 * @note External dependencies (e.g., textures or linked resources) are not supported.
 *       The model data must be fully self-contained.
 */
R3DAPI R3D_Model R3D_LoadModelFromMemoryEx(const void* data, unsigned int size, const char* hint, R3D_ImportFlags flags);

/**
 * @brief Load a 3D model from an existing importer.
 *
 * Creates a model from a previously loaded importer instance.
 * This avoids re-importing the source file.
 *
 * @param importer Importer instance to extract the model from.
 *
 * @return Loaded model structure containing meshes and materials.
 */
R3DAPI R3D_Model R3D_LoadModelFromImporter(const R3D_Importer* importer);

/**
 * @brief Unload a model and optionally its materials.
 *
 * Frees all memory associated with a model, including its meshes.
 * Materials can be optionally unloaded as well.
 *
 * @param model The model to be unloaded.
 * @param unloadMaterials If true, also unloads all materials associated with the model.
 * Set to false if textures are still being used elsewhere to avoid freeing shared resources.
 */
R3DAPI void R3D_UnloadModel(R3D_Model model, bool unloadMaterials);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Model

#endif // R3D_MODEL_H
