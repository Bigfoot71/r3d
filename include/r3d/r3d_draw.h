/* r3d_draw.h -- R3D Draw Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_DRAW_H
#define R3D_DRAW_H

#include "./r3d_instance.h"
#include "./r3d_platform.h"
#include "./r3d_model.h"
#include "./r3d_decal.h"
#include <raylib.h>

/**
 * @defgroup Draw
 * @{
 */

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Begins a rendering session for a 3D camera.
 * 
 * This function starts a rendering session, preparing the engine to handle subsequent 
 * draw calls using the provided camera settings. Rendering output will be directed 
 * to the default screen framebuffer.
 * 
 * @param camera The camera to use for rendering the scene.
 */
R3DAPI void R3D_Begin(Camera3D camera);

/**
 * @brief Begins a rendering session for a 3D camera with an optional custom render target.
 * 
 * This function starts a rendering session, preparing the engine to handle subsequent 
 * draw calls using the provided camera settings. If a render target is provided, rendering 
 * output will be directed to it. If the target is `NULL`, rendering will be performed 
 * directly to the screen framebuffer (same behavior as R3D_Begin).
 * 
 * @param camera The camera to use for rendering the scene.
 * @param target Optional pointer to a RenderTexture to render into. Can be NULL to render 
 *               directly to the screen.
 */
R3DAPI void R3D_BeginEx(Camera3D camera, const RenderTexture* target);

/**
 * @brief Ends the current rendering session.
 * 
 * This function signals the end of a rendering session, at which point the engine 
 * will process all necessary render passes and output the final result to the main 
 * or custom framebuffer.
 */
R3DAPI void R3D_End(void);

/**
 * @brief Draws a mesh with a specified material and transformation.
 * 
 * This function renders a mesh with the provided material and transformation matrix.
 * 
 * @param mesh A pointer to the mesh to render. Cannot be NULL.
 * @param material A pointer to the material to apply to the mesh. Can be NULL, default material will be used.
 * @param transform The transformation matrix to apply to the mesh.
 */
R3DAPI void R3D_DrawMesh(const R3D_Mesh* mesh, const R3D_Material* material, Matrix transform);

/**
 * @brief Draws a mesh with instancing support.
 * 
 * This function renders a mesh multiple times for each instance.
 * 
 * @param mesh A pointer to the mesh to render. Cannot be NULL.
 * @param material A pointer to the material to apply to the mesh. Can be NULL, default material will be used.
 * @param instances A pointer tot the instance buffer to use. Cannot be NULL.
 * @param count The number of instances to render. Clamped between 1 and instance buffer capacity.
 */
R3DAPI void R3D_DrawMeshInstanced(const R3D_Mesh* mesh, const R3D_Material* material, const R3D_InstanceBuffer* instances, int count);

/**
 * @brief Draws a mesh with instancing support.
 *
 * This function renders a mesh multiple times for each instance.
 * Allows to provide a bounding box and global transformation of all instances.
 *
 * @param mesh A pointer to the mesh to render. Cannot be NULL.
 * @param material A pointer to the material to apply to the mesh. Can be NULL, default material will be used.
 * @param globalAaabb Bounding box for frustum culling. Ignored if zeroed.
 * @param globalTransform Global transformation matrix for all instances.
 * @param instances A pointer tot the instance buffer to use. Cannot be NULL.
 * @param count The number of instances to render. Clamped between 1 and instance buffer capacity.
 */
R3DAPI void R3D_DrawMeshInstancedEx(const R3D_Mesh* mesh, const R3D_Material* material,
                                    BoundingBox globalAabb, Matrix globalTransform,
                                    const R3D_InstanceBuffer* instances, int count);

/**
 * @brief Draws a model at a specified position and scale.
 *
 * This function renders a model at the given position with the specified scale factor.
 *
 * @param model A pointer to the model to render.
 * @param position The position to place the model at.
 * @param scale The scale factor to apply to the model.
 */
R3DAPI void R3D_DrawModel(const R3D_Model* model, Vector3 position, float scale);

/**
 * @brief Draws a model with advanced transformation options.
 *
 * This function renders a model with a specified position, rotation axis, rotation 
 * angle, and scale. It provides more control over how the model is transformed before 
 * rendering.
 *
 * @param model A pointer to the model to render.
 * @param position The position to place the model at.
 * @param rotationAxis The axis of rotation for the model.
 * @param rotationAngle The angle to rotate the model.
 * @param scale The scale factor to apply to the model.
 */
R3DAPI void R3D_DrawModelEx(const R3D_Model* model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale);

/**
 * @brief Draws a model using a transformation matrix.
 *
 * This function renders a model using a custom transformation matrix, allowing full control 
 * over the model's position, rotation, scale, and skew. It is intended for advanced rendering 
 * scenarios where a single matrix defines the complete transformation.
 *
 * @param model A pointer to the model to render.
 * @param transform A transformation matrix that defines how to position, rotate, and scale the model.
 */
R3DAPI void R3D_DrawModelPro(const R3D_Model* model, Matrix transform);

/**
 * @brief Draws a model with instancing support.
 *
 * This function renders a model multiple times for each instance.
 * 
 * @param model A pointer to the model to render. Cannot be NULL.
 * @param instances A pointer tot the instance buffer to use. Cannot be NULL.
 * @param count The number of instances to render. Clamped between 1 and instance buffer capacity.
 */
R3DAPI void R3D_DrawModelInstanced(const R3D_Model* model, const R3D_InstanceBuffer* instances, int count);

/**
 * @brief Draws a model with instancing support.
 *
 * This function renders a model multiple times for each instance.
 * Allows to provide a bounding box and global transformation of all instances.
 *
 * @param model A pointer to the model to render. Cannot be NULL.
 * @param globalAaabb Bounding box for frustum culling. Ignored if zeroed.
 * @param globalTransform Global transformation matrix for all instances.
 * @param instances A pointer tot the instance buffer to use. Cannot be NULL.
 * @param count The number of instances to render. Clamped between 1 and instance buffer capacity.
 */
R3DAPI void R3D_DrawModelInstancedEx(const R3D_Model* model,
                                     BoundingBox globalAabb, Matrix globalTransform,
                                     const R3D_InstanceBuffer* instances, int count);

/**
 * @brief Draws a decal using a transformation matrix.
 *
 * This function renders a decal in 3D space at the given position.
 *
 * @param decal A pointer to the decal to render.
 * @param transform A transformation matrix that defines how to position, rotate, and scale the decal.
 */
R3DAPI void R3D_DrawDecal(const R3D_Decal* decal, Matrix transform);

/**
 * @brief Draws a decal with instancing support.
 *
 * This function renders a model multiple times for each instance.
 *
 * @param decal A pointer to the decal to render. Cannot be NULL.
 * @param instances A pointer tot the instance buffer to use. Cannot be NULL.
 * @param count The number of instances to render. Clamped between 1 and instance buffer capacity.
 */
R3DAPI void R3D_DrawDecalInstanced(const R3D_Decal* decal, const R3D_InstanceBuffer* instances, int count);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Draw

#endif // R3D_DRAW_H
