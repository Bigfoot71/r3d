/* r3d_draw.h -- R3D Draw Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_DRAW_H
#define R3D_DRAW_H

#include "./r3d_particles.h"
#include "./r3d_model.h"
#include "./r3d_decal.h"
#include "./r3d_api.h"
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
 * This function renders a mesh multiple times with different transformation matrices 
 * for each instance.
 * 
 * @param mesh A pointer to the mesh to render. Cannot be NULL.
 * @param material A pointer to the material to apply to the mesh. Can be NULL, default material will be used.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawMeshInstanced(const R3D_Mesh* mesh, const R3D_Material* material, const Matrix* instanceTransforms, int instanceCount);

/**
 * @brief Draws a mesh with instancing support and different colors per instance.
 * 
 * This function renders a mesh multiple times with different transformation matrices 
 * and different colors for each instance.
 * 
 * @param mesh A pointer to the mesh to render. Cannot be NULL.
 * @param material A pointer to the material to apply to the mesh. Can be NULL, default material will be used.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceColors Array of colors for each instance. Can be NULL if no per-instance colors are needed.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawMeshInstancedEx(const R3D_Mesh* mesh, const R3D_Material* material, const Matrix* instanceTransforms, const Color* instanceColors, int instanceCount);

/**
 * @brief Draws a mesh with instancing support, a global transformation, and different colors per instance.
 *
 * This function renders a mesh multiple times using instancing, with a global transformation
 * applied to all instances, and individual transformation matrices and colors for each instance.
 * Each instance can have its own position, rotation, scale, and color while sharing the same mesh
 * and material.
 *
 * @param mesh A pointer to the mesh to render. Cannot be NULL.
 * @param material A pointer to the material to apply to the mesh. Can be NULL, default material will be used.
 * @param globalAabb Optional bounding box encompassing all instances, in local space. Used for frustum culling.
 *                   Can be NULL to disable culling. Will be transformed by the global matrix if necessary.
 * @param globalTransform The global transformation matrix applied to all instances.
 * @param instanceTransforms Pointer to an array of transformation matrices for each instance, allowing unique transformations. Cannot be NULL.
 * @param transformsStride The stride (in bytes) between consecutive transformation matrices in the array.
 *                         Set to 0 if the matrices are tightly packed (stride equals sizeof(Matrix)).
 *                         If matrices are embedded in a struct, set to the size of the struct or the actual byte offset between elements.
 * @param instanceColors Pointer to an array of colors for each instance, allowing unique colors. Can be NULL if no per-instance colors are needed.
 * @param colorsStride The stride (in bytes) between consecutive colors in the array.
 *                     Set to 0 if the colors are tightly packed (stride equals sizeof(Color)).
 *                     If colors are embedded in a struct, set to the size of the struct or the actual byte offset between elements.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawMeshInstancedPro(const R3D_Mesh* mesh, const R3D_Material* material,
                                     const BoundingBox* globalAabb, Matrix globalTransform,
                                     const Matrix* instanceTransforms, int transformsStride,
                                     const Color* instanceColors, int colorsStride,
                                     int instanceCount);

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
 * This function renders a model multiple times with different transformation matrices 
 * for each instance.
 * 
 * @param model A pointer to the model to render. Cannot be NULL.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawModelInstanced(const R3D_Model* model, const Matrix* instanceTransforms, int instanceCount);

/**
 * @brief Draws a model with instancing support and different colors per instance.
 * 
 * This function renders a model multiple times with different transformation matrices 
 * and different colors for each instance.
 * 
 * @param model A pointer to the model to render. Cannot be NULL.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceColors Array of colors for each instance. Can be NULL if no per-instance colors are needed.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawModelInstancedEx(const R3D_Model* model, const Matrix* instanceTransforms, const Color* instanceColors, int instanceCount);

/**
 * @brief Draws a model with instancing support, a global transformation, and different colors per instance.
 * 
 * This function renders a model multiple times using instancing, with a global transformation
 * applied to all instances, and individual transformation matrices and colors for each instance.
 * Each instance can have its own position, rotation, scale, and color while sharing the same model.
 * 
 * @param model A pointer to the model to render. Cannot be NULL.
 * @param globalAabb Optional bounding box encompassing all instances, in local space. Used for frustum culling.
 *                   Can be NULL to disable culling. Will be transformed by the global matrix if necessary.
 * @param globalTransform The global transformation matrix applied to all instances.
 * @param instanceTransforms Pointer to an array of transformation matrices for each instance, allowing unique transformations. Cannot be NULL.
 * @param transformsStride The stride (in bytes) between consecutive transformation matrices in the array.
 *                         Set to 0 if the matrices are tightly packed (stride equals sizeof(Matrix)).
 *                         If matrices are embedded in a struct, set to the size of the struct or the actual byte offset between elements.
 * @param instanceColors Pointer to an array of colors for each instance, allowing unique colors. Can be NULL if no per-instance colors are needed.
 * @param colorsStride The stride (in bytes) between consecutive colors in the array.
 *                     Set to 0 if the colors are tightly packed (stride equals sizeof(Color)).
 *                     If colors are embedded in a struct, set to the size of the struct or the actual byte offset between elements.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawModelInstancedPro(const R3D_Model* model,
                                      const BoundingBox* globalAabb, Matrix globalTransform,
                                      const Matrix* instanceTransforms, int transformsStride,
                                      const Color* instanceColors, int colorsStride,
                                      int instanceCount);

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
 * This function renders a decal multiple times with different transformation matrices
 * for each instance.
 *
 * @param decal A pointer to the decal to render. Cannot be NULL.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawDecalInstanced(const R3D_Decal* decal, const Matrix* instanceTransforms, int instanceCount);

/**
 * @brief Renders the current state of a CPU-based particle system.
 *
 * This function draws the particles of a CPU-simulated particle system
 * in their current state. It does not modify the simulation or update
 * particle properties such as position, velocity, or lifetime.
 *
 * @param system A pointer to the `R3D_ParticleSystem` to be rendered.
 *               The particle system must be properly initialized and updated
 *               to the desired state before calling this function.
 * @param mesh A pointer to the mesh used to represent each particle. Cannot be NULL.
 * @param material A pointer to the material applied to the particle mesh. Can be NULL, default material will be used.
 */
R3DAPI void R3D_DrawParticleSystem(const R3D_ParticleSystem* system, const R3D_Mesh* mesh, const R3D_Material* material);

/**
 * @brief Renders the current state of a CPU-based particle system with a global transformation.
 *
 * This function is similar to `R3D_DrawParticleSystem`, but it applies an additional
 * global transformation to all particles. This is useful for rendering particle effects
 * in a transformed space (e.g., attached to a moving object).
 *
 * @param system A pointer to the `R3D_ParticleSystem` to be rendered.
 *               The particle system must be properly initialized and updated
 *               to the desired state before calling this function.
 * @param mesh A pointer to the mesh used to represent each particle. Cannot be NULL.
 * @param material A pointer to the material applied to the particle mesh. Can be NULL, default material will be used.
 * @param transform A transformation matrix applied to all particles.
 */
R3DAPI void R3D_DrawParticleSystemEx(const R3D_ParticleSystem* system, const R3D_Mesh* mesh, const R3D_Material* material, Matrix transform);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Draw

#endif // R3D_DRAW_H
