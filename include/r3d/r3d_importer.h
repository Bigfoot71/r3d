/* r3d_importer.h -- R3D Importer Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_IMPORTER_H
#define R3D_IMPORTER_H

#include "./r3d_platform.h"
#include <stdint.h>

/**
 * @defgroup Importer
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @typedef R3D_ImportFlags
 * @brief Flags controlling importer behavior.
 *
 * These flags define how the importer processes the source asset.
 */
typedef uint32_t R3D_ImportFlags;

/**
 * @brief Keep a CPU-side copy of mesh data.
 *
 * When enabled, raw mesh data is preserved in RAM after model import.
 */
#define R3D_IMPORT_MESH_DATA    (1 << 0)

/**
 * @brief Enable high-quality import processing.
 *
 * When enabled, the importer uses a higher-quality post-processing
 * (e.g. smooth normals, mesh optimization, data validation).
 * This mode is intended for editor usage and offline processing.
 *
 * When disabled, a faster import preset is used, suitable for runtime.
 */
#define R3D_IMPORT_QUALITY      (1 << 1)

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Opaque importer handle.
 *
 * Represents a loaded asset file that can be used to extract multiple
 * resources (models, skeletons, animations) without re-importing the file.
 */
typedef struct R3D_Importer R3D_Importer;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load an importer from a file.
 *
 * Creates an importer instance from the specified file path.
 * The file is parsed once and can be reused to extract multiple
 * resources such as models and animations.
 *
 * @param filePath Path to the asset file.
 * @param flags Importer behavior flags.
 *
 * @return Pointer to a new importer instance, or NULL on failure.
 */
R3DAPI R3D_Importer* R3D_LoadImporter(const char* filePath, R3D_ImportFlags flags);

/**
 * @brief Load an importer from a memory buffer.
 *
 * Creates an importer instance from in-memory asset data.
 * This is useful for embedded assets or streamed content.
 *
 * @param data Pointer to the asset data.
 * @param size Size of the data buffer in bytes.
 * @param hint Optional file format hint (may be NULL).
 * @param flags Importer behavior flags.
 *
 * @return Pointer to a new importer instance, or NULL on failure.
 */
R3DAPI R3D_Importer* R3D_LoadImporterFromMemory(const void* data, unsigned int size, const char* hint, R3D_ImportFlags flags);

/**
 * @brief Destroy an importer instance.
 *
 * Frees all resources associated with the importer.
 * Any models or animations extracted from it remain valid.
 *
 * @param importer Importer instance to destroy.
 */
R3DAPI void R3D_UnloadImporter(R3D_Importer* importer);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Importer

#endif // R3D_IMPORTER_H
