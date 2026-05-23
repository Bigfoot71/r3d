/* r3d_draw.h -- R3D Draw Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_DRAW_H
#define R3D_DRAW_H

#include "./r3d_animation_player.h"
#include "./r3d_instance.h"
#include "./r3d_platform.h"
#include "./r3d_camera.h"
#include "./r3d_model.h"
#include "./r3d_decal.h"
#include <raylib.h>

/**
 * @defgroup Draw
 * @{
 */

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief Describes a complete R3D rendering view.
 *
 * A view defines where and how a scene is rendered for a single rendering
 * session. It combines an R3D camera, an optional viewport and an optional
 * render target.
 *
 * If `target` is zero-initialized, rendering is directed to the default
 * framebuffer. If `viewport.width` or `viewport.height` is less than or equal
 * to zero, the full target size is used.
 */
typedef struct R3D_View {
    R3D_Camera camera;      ///< Camera used for this view.
    Rectangle viewport;     ///< Viewport inside the target. If width or height <= 0, the full target is used.
    RenderTexture target;   ///< Render target. Zero-initialized means screen/backbuffer.
} R3D_View;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Begins a rendering session using a raylib camera.
 *
 * This is the simplest entry point and follows the usual raylib-style workflow.
 * Rendering output is directed to the default framebuffer.
 *
 * The given raylib camera is converted internally to an R3D camera using
 * `R3D_CameraFromRL()`.
 *
 * @param camera Camera used to render the scene.
 */
R3DAPI void R3D_Begin(Camera3D camera);

/**
 * @brief Begins a rendering session using an R3D camera.
 *
 * Rendering output is directed to the default framebuffer.
 *
 * This entry point provides access to R3D-specific camera features such as
 * layer masks, custom near/far clipping planes and quaternion-based orientation.
 *
 * @param camera Camera used to render the scene.
 */
R3DAPI void R3D_BeginEx(R3D_Camera camera);

/**
 * @brief Begins a rendering session using a complete R3D view descriptor.
 *
 * This is the advanced entry point. It allows the caller to specify the camera,
 * render target and viewport used for the rendering session.
 *
 * Use this function for render-to-texture workflows, custom viewports,
 * multipass rendering, editor views, minimaps, probes or any case where the
 * default framebuffer is not enough.
 *
 * @param view View descriptor used to render the scene.
 */
R3DAPI void R3D_BeginPro(R3D_View view);

/**
 * @brief Ends the current rendering session.
 *
 * This function is the one that actually performs the full
 * rendering of the described scene. It carries out culling,
 * sorting, shadow rendering, scene rendering, and screen /
 * post-processing effects.
 */
R3DAPI void R3D_End(void);

/**
 * @brief Begins a clustered draw pass.
 *
 * All draw calls submitted in this pass are first tested against the
 * cluster AABB. If the cluster fails the scene/shadow frustum test,
 * none of the contained objects are tested or drawn.
 *
 * @param aabb Bounding box used as the cluster-level frustum test.
 */
R3DAPI void R3D_BeginCluster(BoundingBox aabb);

/**
 * @brief Ends the current clustered draw pass.
 *
 * Stops submitting draw calls to the active cluster.
 */
R3DAPI void R3D_EndCluster(void);

/**
 * @brief Queues a mesh draw command with position and uniform scale.
 * 
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawMesh(R3D_Mesh mesh, R3D_Material material, Vector3 position, float scale);

/**
 * @brief Queues a mesh draw command with position, rotation and non-uniform scale.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawMeshEx(R3D_Mesh mesh, R3D_Material material, Vector3 position, Quaternion rotation, Vector3 scale);

/**
 * @brief Queues a mesh draw command using a full transform matrix.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawMeshPro(R3D_Mesh mesh, R3D_Material material, Matrix transform);

/**
 * @brief Queues an instanced mesh draw command.
 *
 * Draws multiple instances using the provided instance buffer.
 * Does nothing if the number of instances is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawMeshInstanced(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int count);

/**
 * @brief Queues an instanced mesh draw command with an instance range.
 *
 * Draws 'count' instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawMeshInstancedEx(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int offset, int count);

/**
 * @brief Queues an instanced mesh draw command with an instance range and an additional transform.
 *
 * Draws 'count' instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 * The transform is applied to all instances.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawMeshInstancedPro(R3D_Mesh mesh, R3D_Material material, R3D_InstanceBuffer instances, int offset, int count, Matrix transform);

/**
 * @brief Queues a model draw command with position and uniform scale.
 * 
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawModel(R3D_Model model, Vector3 position, float scale);

/**
 * @brief Queues a model draw command with position, rotation and non-uniform scale.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawModelEx(R3D_Model model, Vector3 position, Quaternion rotation, Vector3 scale);

/**
 * @brief Queues a model draw command using a full transform matrix.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawModelPro(R3D_Model model, Matrix transform);

/**
 * @brief Queues an instanced model draw command.
 *
 * Draws multiple instances using the provided instance buffer.
 * Does nothing if the number of instances is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawModelInstanced(R3D_Model model, R3D_InstanceBuffer instances, int count);

/**
 * @brief Queues an instanced model draw command with an instance range.
 *
 * Draws 'count' instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawModelInstancedEx(R3D_Model model, R3D_InstanceBuffer instances, int offset, int count);

/**
 * @brief Queues an instanced model draw command with an instance range and an additional transform.
 *
 * Draws 'count' instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 * The transform is applied to all instances.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawModelInstancedPro(R3D_Model model, R3D_InstanceBuffer instances, int offset, int count, Matrix transform);

/**
 * @brief Queues an animated model draw command.
 *
 * Uses the provided animation player to compute the pose.
 * 
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawAnimatedModel(R3D_Model model, R3D_AnimationPlayer player, Vector3 position, float scale);

/**
 * @brief Queues an animated model draw command with position, rotation and non-uniform scale.
 *
 * Uses the provided animation player to compute the pose.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawAnimatedModelEx(R3D_Model model, R3D_AnimationPlayer player, Vector3 position, Quaternion rotation, Vector3 scale);

/**
 * @brief Queues an animated model draw command using a full transform matrix.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawAnimatedModelPro(R3D_Model model, R3D_AnimationPlayer player, Matrix transform);

/**
 * @brief Queues an instanced animated model draw command.
 *
 * Draws multiple animated instances using the provided instance buffer.
 * Does nothing if the number of instances is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawAnimatedModelInstanced(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int count);

/**
 * @brief Queues an instanced animated model draw command with an instance range.
 *
 * Draws 'count' animated instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawAnimatedModelInstancedEx(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int offset, int count);

/**
 * @brief Queues an instanced animated model draw command with an instance range and an additional transform.
 *
 * Draws 'count' animated instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 * The transform is applied to all instances.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawAnimatedModelInstancedPro(R3D_Model model, R3D_AnimationPlayer player, R3D_InstanceBuffer instances, int offset, int count, Matrix transform);

/**
 * @brief Queues a decal draw command with position and uniform scale.
 * 
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawDecal(R3D_Decal decal, Vector3 position, float scale);

/**
 * @brief Queues a decal draw command with position, rotation and non-uniform scale.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawDecalEx(R3D_Decal decal, Vector3 position, Quaternion rotation, Vector3 scale);

/**
 * @brief Queues a decal draw command using a full transform matrix.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawDecalPro(R3D_Decal decal, Matrix transform);

/**
 * @brief Queues an instanced decal draw command.
 *
 * Draws multiple instances using the provided instance buffer.
 * Does nothing if the number of instances is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawDecalInstanced(R3D_Decal decal, R3D_InstanceBuffer instances, int count);

/**
 * @brief Queues an instanced decal draw command with an instance range.
 *
 * Draws 'count' instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawDecalInstancedEx(R3D_Decal decal, R3D_InstanceBuffer instances, int offset, int count);

/**
 * @brief Queues an instanced decal draw command with an instance range and an additional transform.
 *
 * Draws 'count' instances starting at 'offset' in the instance buffer.
 * Both 'offset' and 'count' are clamped to stay within [0, instances.capacity]:
 *   - offset is clamped to [0, capacity]
 *   - count is clamped to [0, capacity - offset]
 * Does nothing if the resulting count is <= 0.
 * The transform is applied to all instances.
 *
 * The command is executed during R3D_End().
 */
R3DAPI void R3D_DrawDecalInstancedPro(R3D_Decal decal, R3D_InstanceBuffer instances, int offset, int count, Matrix transform);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Draw

#endif // R3D_DRAW_H
