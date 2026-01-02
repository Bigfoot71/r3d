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
 * @brief Create a model from a single mesh.
 *
 * Creates a model structure containing a single mesh with a default material.
 * This is useful for procedurally generated meshes or simple geometry.
 *
 * @warning The model's bounding box calculation assumes that the mesh's
 * bounding boxes has already been computed.
 *
 * @param mesh The mesh to be wrapped in a model structure.
 *
 * @return Model structure containing the specified mesh.
 */
R3DAPI R3D_Model R3D_LoadModelFromMesh(R3D_Mesh mesh);

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
