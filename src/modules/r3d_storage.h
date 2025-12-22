/* r3d_storage.h -- Internal R3D storage module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_STORAGE_H
#define R3D_MODULE_STORAGE_H

#include <raylib.h>
#include <glad.h>

// ========================================
// STORAGE CONSTANTS
// ========================================

#define R3D_STORAGE_MAX_BONE_MATRICES 256

// ========================================
// STORAGE ENUM
// ========================================

typedef enum {
    R3D_STORAGE_BONE_MATRICES,
    R3D_STORAGE_COUNT
} r3d_storage_t;

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_storage_init(void);

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_storage_quit(void);

/*
 * Bind the storage texture to the specified slot, then upload the data to it.
 */
void r3d_storage_use(r3d_storage_t storage, int slot, const void* data, int count);

#endif // R3D_MODULE_STORAGE_H
