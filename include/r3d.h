/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#ifndef R3D_H
#define R3D_H

#include <raylib.h>


// --------------------------------------------
//                   DEFINES
// --------------------------------------------

#if defined(_WIN32)
#   if defined(__TINYC__)
#       define __declspec(x) __attribute__((x))
#   endif
#   if defined(R3D_BUILD_SHARED)
#       define R3DAPI __declspec(dllexport)
#   elif defined(R3D_USE_SHARED)
#       define R3DAPI __declspec(dllimport)
#   endif
#else
#   if defined(R3D_BUILD_SHARED)
#       define R3DAPI __attribute__((visibility("default")))
#   endif
#endif

#ifndef R3DAPI
#   define R3DAPI extern
#endif



// --------------------------------------------
//                   ENUMS
// --------------------------------------------

/**
 * @brief Flags to configure the rendering engine behavior.
 *
 * These flags control various aspects of the rendering pipeline.
 */
typedef unsigned int R3D_Flags;

#define R3D_FLAG_NONE                   0           /**< No special rendering flags */
#define R3D_FLAG_FXAA                   (1 << 0)    /**< Enables Fast Approximate Anti-Aliasing (FXAA) */
#define R3D_FLAG_BLIT_LINEAR            (1 << 1)    /**< Uses linear filtering when blitting the final image */
#define R3D_FLAG_ASPECT_KEEP            (1 << 2)    /**< Maintains the aspect ratio of the internal resolution when blitting the final image */
#define R3D_FLAG_STENCIL_TEST           (1 << 3)    /**< Performs a stencil test on each rendering pass affecting geometry */
#define R3D_FLAG_8_BIT_NORMALS          (1 << 4)    /**< Use 8-bit precision for the normals buffer (deferred); default is 16-bit float */
#define R3D_FLAG_NO_FRUSTUM_CULLING     (1 << 5)    /**< Disables internal frustum culling. Manual culling is allowed, but may break shadow visibility if objects casting shadows are skipped. */
#define R3D_FLAG_TRANSPARENT_SORTING    (1 << 6)    /**< Back-to-front sorting of transparent objects for correct blending of non-discarded fragments. Be careful, in 'force forward' mode this flag will also sort opaque objects in 'near-to-far' but in the same sorting pass. */
#define R3D_FLAG_OPAQUE_SORTING         (1 << 7)    /**< Front-to-back sorting of opaque objects to optimize depth testing at the cost of additional sorting. Please note, in 'force forward' mode this flag has no effect, see transparent sorting. */
#define R3D_FLAG_LOW_PRECISION_BUFFERS  (1 << 8)    /**< Use 32-bit HDR formats like R11G11B10F for intermediate color buffers instead of full 16-bit floats. Saves memory and bandwidth. */

/**
 * @brief Bitfield type used to specify rendering layers for 3D objects.
 *
 * This type is used by `R3D_Mesh` and `R3D_Sprite` objects to indicate
 * which rendering layer(s) they belong to. Active layers are controlled
 * globally via the functions:
 * 
 * - void R3D_EnableLayers(R3D_Layer bitfield);
 * - void R3D_DisableLayers(R3D_Layer bitfield);
 *
 * A mesh or sprite will be rendered if at least one of its assigned layers is active.
 * Assigning a value of 0 to an object's layer (the default) means the object
 * will always be rendered on screen.
 *
 * For simplicity, 16 layers are defined in this header, but the maximum number
 * of layers depends on the number of bits in `unsigned int` on the target platform.
 */
typedef unsigned int R3D_Layer;

#define R3D_LAYER_01    (1 << 0)
#define R3D_LAYER_02    (1 << 1)
#define R3D_LAYER_03    (1 << 2)
#define R3D_LAYER_04    (1 << 3)
#define R3D_LAYER_05    (1 << 4)
#define R3D_LAYER_06    (1 << 5)
#define R3D_LAYER_07    (1 << 6)
#define R3D_LAYER_08    (1 << 7)
#define R3D_LAYER_09    (1 << 8)
#define R3D_LAYER_10    (1 << 9)
#define R3D_LAYER_11    (1 << 10)
#define R3D_LAYER_12    (1 << 11)
#define R3D_LAYER_13    (1 << 12)
#define R3D_LAYER_14    (1 << 13)
#define R3D_LAYER_15    (1 << 14)
#define R3D_LAYER_16    (1 << 15)

/**
 * @brief Blend modes for rendering.
 *
 * Defines common blending modes used in 3D rendering to combine source and destination colors.
 * @note The blend mode is applied only if you are in forward rendering mode or auto-detect mode.
 */
typedef enum R3D_BlendMode {
    R3D_BLEND_OPAQUE,               ///< No blending, the source color fully replaces the destination color.
    R3D_BLEND_ALPHA,                ///< Alpha blending: source color is blended with the destination based on alpha value.
    R3D_BLEND_ADDITIVE,             ///< Additive blending: source color is added to the destination, making bright effects.
    R3D_BLEND_MULTIPLY,             ///< Multiply blending: source color is multiplied with the destination, darkening the image.
    R3D_BLEND_PREMULTIPLIED_ALPHA   ///< Premultiplied alpha blending: source color is blended with the destination assuming the source color is already multiplied by its alpha.
} R3D_BlendMode;

/**
 * @brief Face culling modes for a mesh.
 *
 * Specifies which faces of a geometry are discarded during rendering based on their winding order.
 */
typedef enum R3D_CullMode {
    R3D_CULL_NONE,   ///< No culling; all faces are rendered.
    R3D_CULL_BACK,   ///< Cull back-facing polygons (faces with clockwise winding order).
    R3D_CULL_FRONT   ///< Cull front-facing polygons (faces with counter-clockwise winding order).
} R3D_CullMode;

/**
 * @brief Shadow casting modes for objects.
 *
 * Controls how an object interacts with the shadow mapping system.
 * These modes determine whether the object contributes to shadows,
 * and if so, whether it is also rendered in the main pass.
 */
typedef enum R3D_ShadowCastMode {
    R3D_SHADOW_CAST_ON_AUTO,            ///< The object casts shadows; the faces used are determined by the material's culling mode.
    R3D_SHADOW_CAST_ON_DOUBLE_SIDED,    ///< The object casts shadows with both front and back faces, ignoring face culling.
    R3D_SHADOW_CAST_ON_FRONT_SIDE,      ///< The object casts shadows with only front faces, culling back faces.
    R3D_SHADOW_CAST_ON_BACK_SIDE,       ///< The object casts shadows with only back faces, culling front faces.
    R3D_SHADOW_CAST_ONLY_AUTO,          ///< The object only casts shadows; the faces used are determined by the material's culling mode.
    R3D_SHADOW_CAST_ONLY_DOUBLE_SIDED,  ///< The object only casts shadows with both front and back faces, ignoring face culling.
    R3D_SHADOW_CAST_ONLY_FRONT_SIDE,    ///< The object only casts shadows with only front faces, culling back faces.
    R3D_SHADOW_CAST_ONLY_BACK_SIDE,     ///< The object only casts shadows with only back faces, culling front faces.
    R3D_SHADOW_CAST_DISABLED            ///< The object does not cast shadows at all.
} R3D_ShadowCastMode;

typedef enum R3D_DepthMode {
    R3D_DEPTH_READ_WRITE,       ///< Enable depth testing and writing to the depth buffer.
    R3D_DEPTH_READ_ONLY,        ///< Enable depth testing but disable writing to the depth buffer.
    R3D_DEPTH_DISABLED          ///< Disable depth testing and writing to the depth buffer.
} R3D_DepthMode;

/**
 * @brief Defines billboard modes for 3D objects.
 *
 * This enumeration defines how a 3D object aligns itself relative to the camera.
 * It provides options to disable billboarding or to enable specific modes of alignment.
 */
typedef enum R3D_BillboardMode {
    R3D_BILLBOARD_DISABLED,     ///< Billboarding is disabled; the object retains its original orientation.
    R3D_BILLBOARD_FRONT,        ///< Full billboarding; the object fully faces the camera, rotating on all axes.
    R3D_BILLBOARD_Y_AXIS        /**< Y-axis constrained billboarding; the object rotates only around the Y-axis,
                                     keeping its "up" orientation fixed. This is suitable for upright objects like characters or signs. */
} R3D_BillboardMode;

/**
 * @brief Types of lights supported by the rendering engine.
 *
 * Each light type has different behaviors and use cases.
 */
typedef enum R3D_LightType {
    R3D_LIGHT_DIR,                      ///< Directional light, affects the entire scene with parallel rays.
    R3D_LIGHT_SPOT,                     ///< Spot light, emits light in a cone shape.
    R3D_LIGHT_OMNI                      ///< Omni light, emits light in all directions from a single point.
} R3D_LightType;

/**
 * @brief Modes for updating shadow maps.
 *
 * Determines how often the shadow maps are refreshed.
 */
typedef enum R3D_ShadowUpdateMode {
    R3D_SHADOW_UPDATE_MANUAL,           ///< Shadow maps update only when explicitly requested.
    R3D_SHADOW_UPDATE_INTERVAL,         ///< Shadow maps update at defined time intervals.
    R3D_SHADOW_UPDATE_CONTINUOUS        ///< Shadow maps update every frame for real-time accuracy.
} R3D_ShadowUpdateMode;

/**
 * @brief Bloom effect modes.
 *
 * Specifies different post-processing bloom techniques that can be applied
 * to the rendered scene. Bloom effects enhance the appearance of bright areas
 * by simulating light bleeding, contributing to a more cinematic and realistic look.
 */
 typedef enum R3D_Bloom {
    R3D_BLOOM_DISABLED,     ///< Bloom effect is disabled. The scene is rendered without any glow enhancement.
    R3D_BLOOM_MIX,          ///< Blends the bloom effect with the original scene using linear interpolation (Lerp).
    R3D_BLOOM_ADDITIVE,     ///< Adds the bloom effect additively to the scene, intensifying bright regions.
    R3D_BLOOM_SCREEN        ///< Combines the scene and bloom using screen blending, which brightens highlights
} R3D_Bloom;

/**
 * @brief Fog effect modes.
 *
 * Determines how fog is applied to the scene, affecting depth perception and atmosphere.
 */
typedef enum R3D_Fog {
    R3D_FOG_DISABLED, ///< Fog effect is disabled.
    R3D_FOG_LINEAR,   ///< Fog density increases linearly with distance from the camera.
    R3D_FOG_EXP2,     ///< Exponential fog (exp2), where density increases exponentially with distance.
    R3D_FOG_EXP       ///< Exponential fog, similar to EXP2 but with a different rate of increase.
} R3D_Fog;

/**
 * @brief Tone mapping modes.
 *
 * Controls how high dynamic range (HDR) colors are mapped to low dynamic range (LDR) for display.
 */
typedef enum R3D_Tonemap {
    R3D_TONEMAP_LINEAR,   ///< Simple linear mapping of HDR values.
    R3D_TONEMAP_REINHARD, ///< Reinhard tone mapping, a balanced method for compressing HDR values.
    R3D_TONEMAP_FILMIC,   ///< Filmic tone mapping, mimicking the response of photographic film.
    R3D_TONEMAP_ACES,     ///< ACES tone mapping, a high-quality cinematic rendering technique.
    R3D_TONEMAP_AGX,      ///< AGX tone mapping, a modern technique designed to preserve both highlight and shadow details for HDR rendering.
    R3D_TONEMAP_COUNT     ///< Number of tone mapping modes (used internally)
} R3D_Tonemap;

/**
 * @brief Depth of field effect modes.
 *
 * Controls how depth of field is applied to the scene, affecting the focus and blur of objects.
 */
typedef enum R3D_Dof {
    R3D_DOF_DISABLED, ///< Depth of field effect is disabled.
    R3D_DOF_ENABLED,  ///< Depth of field effect is enabled.
} R3D_Dof;

/**
 * @brief Animation Update modes.
 *
 * Controls wether to allow external animation matrices
 */
typedef enum R3D_AnimMode {
    R3D_ANIM_INTERNAL,         ///< Default animation solution
    R3D_ANIM_CUSTOM,           ///< User supplied matrices via R3D_Model::boneOverride 
} R3D_AnimMode;

// --------------------------------------------
//                   TYPES
// --------------------------------------------

/**
 * @brief Represents a vertex and all its attributes for a mesh.
 */
typedef struct R3D_Vertex {
    Vector3 position;       /**< The 3D position of the vertex in object space. */
    Vector2 texcoord;       /**< The 2D texture coordinates (UV) for mapping textures. */
    Vector3 normal;         /**< The normal vector used for lighting calculations. */
    Vector4 color;          /**< Vertex color, typically RGBA. */
    Vector4 tangent;        /**< The tangent vector, used in normal mapping (often with a handedness in w). */
    int boneIds[4];         /**< Indices of up to 4 bones that influence this vertex (for GPU skinning). */
    float weights[4];       /**< Corresponding bone weights (should sum to 1.0). Defines the influence of each bone. */
} R3D_Vertex;

/**
 * @brief Represents a mesh with its geometry data and GPU buffers.
 *
 * Contains vertex/index data, GPU buffer handles, and bounding volume.
 */
typedef struct R3D_Mesh {

    R3D_Vertex* vertices;                 /**< Pointer to the array of vertices. */
    unsigned int* indices;                /**< Pointer to the array of indices. */

    int vertexCount;                      /**< Number of vertices. */
    int indexCount;                       /**< Number of indices. */

    unsigned int vbo;                     /**< Vertex Buffer Object (GPU handle). */
    unsigned int ebo;                     /**< Element Buffer Object (GPU handle). */
    unsigned int vao;                     /**< Vertex Array Object (GPU handle). */

    Matrix* boneMatrices;                 /**< Cached animation matrices for all passes. */
    int boneCount;                        /**< Number of bones (and matrices) that affect the mesh. */

    R3D_ShadowCastMode shadowCastMode;    /**< Shadow casting mode for the mesh. */
    R3D_DepthMode depthMode;              /**< Depth testing mode for the mesh. */

    BoundingBox aabb;                     /**< Axis-Aligned Bounding Box in local space. */

    R3D_Layer layers;                     /**< Bitfield indicating the rendering layer(s) this object belongs to. 
                                               A value of 0 means the object is always rendered. */

} R3D_Mesh;

/**
 * @brief Represents a material with textures, parameters, and rendering modes.
 *
 * Combines multiple texture maps and settings used during shading.
 */
typedef struct R3D_Material {

    struct R3D_MapAlbedo {
        Texture2D texture;      /**< Albedo (base color) texture. */
        Color color;            /**< Albedo color multiplier. */
    } albedo;

    struct R3D_MapEmission {
        Texture2D texture;      /**< Emission texture. */
        Color color;            /**< Emission color. */
        float energy;           /**< Emission energy multiplier. */
    } emission;

    struct R3D_MapNormal {
        Texture2D texture;      /**< Normal map texture. */
        float scale;            /**< Normal scale. */
    } normal;

    struct R3D_MapORM {
        Texture2D texture;      /**< Combined Occlusion-Roughness-Metalness texture. */
        float occlusion;        /**< Occlusion multiplier. */
        float roughness;        /**< Roughness multiplier. */
        float metalness;        /**< Metalness multiplier. */
    } orm;

    R3D_BlendMode blendMode;              /**< Blend mode used for rendering the material. */
    R3D_CullMode cullMode;                /**< Face culling mode used for the material. */

    R3D_BillboardMode billboardMode;      /**< Billboard mode applied to the object. */

    Vector2 uvOffset;                     /**< UV offset applied to the texture coordinates.
                                           *  For models, this can be set manually.
                                           *  For sprites, this value is overridden automatically.
                                           */

    Vector2 uvScale;                      /**< UV scale factor applied to the texture coordinates.
                                           *  For models, this can be set manually.
                                           *  For sprites, this value is overridden automatically.
                                           */

    float alphaCutoff;          /**< Alpha threshold below which fragments are discarded. */

} R3D_Material;

/**
 * @brief Represents a skeletal animation for a model.
 *
 * This structure holds the animation data for a skinned model,
 * including per-frame bone transformation poses.
 */
typedef struct R3D_ModelAnimation {

    int boneCount;                  /**< Number of bones in the skeleton affected by this animation. */
    int frameCount;                 /**< Total number of frames in the animation sequence. */

    BoneInfo* bones;                /**< Array of bone metadata (name, parent index, etc.) defining the skeleton hierarchy. */

    Matrix** frameGlobalPoses;      /**< 2D array [frame][bone]. Global bone matrices (relative to model space). */
    Transform** frameLocalPoses;    /**< 2D array [frame][bone]. Local bone transforms (TRS relative to parent). */

    char name[32];                  /**< Name identifier for the animation (e.g., "Walk", "Jump"). */

} R3D_ModelAnimation;

/**
 * @brief Represents a complete 3D model with meshes and materials.
 *
 * Contains multiple meshes and their associated materials, along with bounding information.
 */
typedef struct R3D_Model {

    R3D_Mesh* meshes;               /**< Array of meshes composing the model. */
    R3D_Material* materials;        /**< Array of materials used by the model. */
    int* meshMaterials;             /**< Array of material indices, one per mesh. */

    int meshCount;                  /**< Number of meshes. */
    int materialCount;              /**< Number of materials. */

    BoundingBox aabb;               /**< Axis-Aligned Bounding Box encompassing the whole model. */

    Matrix* boneOffsets;            /**< Array of offset (inverse bind) matrices, one per bone.
                                         Transforms mesh-space vertices to bone space. Used in skinning. */
    R3D_AnimMode animationMode;
    Matrix* boneOverride;           /**< Array of Matrices we'll use if we have it instead of internal calculations, Used in skinning. */

    BoneInfo* bones;                /**< Bones information (skeleton). Defines the hierarchy and names of bones. */
    int boneCount;                  /**< Number of bones. */

    const R3D_ModelAnimation* anim; /**< Pointer to the currently assigned animation for this model (optional). */
    int animFrame;                  /**< Current animation frame index. Used for sampling bone poses from the animation. */

} R3D_Model;

/**
 * @brief Represents a unique identifier for a light in R3D.
 *
 * This ID is used to reference a specific light when calling R3D lighting functions.
 */
typedef unsigned int R3D_Light;

/**
 * @brief Structure representing a skybox and its related textures for lighting.
 *
 * This structure contains textures used for rendering a skybox, as well as
 * precomputed lighting textures used for image-based lighting (IBL).
 */
typedef struct R3D_Skybox {
    TextureCubemap cubemap;  ///< The skybox cubemap texture for the background and reflections.
    Texture2D irradiance;    ///< The irradiance cubemap for diffuse ambient lighting.
    Texture2D prefilter;     ///< The prefiltered cubemap for specular reflections with mipmaps.
} R3D_Skybox;

/**
 * @brief Represents a 3D sprite with billboard rendering and animation support.
 *
 * This structure defines a 3D sprite, which by default is rendered as a billboard around the Y-axis.
 * The sprite supports frame-based animations and can be configured to use various billboard modes.
 *
 * @warning The shadow mode does not handle transparency. If shadows are enabled, the entire quad will be rendered in the shadow map,
 * potentially causing undesired visual artifacts for semi-transparent sprites.
 */
typedef struct R3D_Sprite {
    R3D_Material material;                 ///< The material used for rendering the sprite, including its texture and shading properties.
    R3D_ShadowCastMode shadowCastMode;     ///< The shadow casting mode for the sprite.
    float currentFrame;                    ///< The current animation frame, represented as a floating-point value to allow smooth interpolation.
    Vector2 frameSize;                     ///< The size of a single animation frame, in texture coordinates (width and height).
    int xFrameCount;                       ///< The number of frames along the horizontal (X) axis of the texture.
    int yFrameCount;                       ///< The number of frames along the vertical (Y) axis of the texture.
    R3D_Layer layers;                      /**< Bitfield indicating the rendering layer(s) this object belongs to. 
                                                A value of 0 means the object is always rendered. */
} R3D_Sprite;

/**
 * @brief Represents a keyframe in an interpolation curve.
 *
 * A keyframe contains two values: the time at which the keyframe occurs and the value of the interpolation at that time.
 * The time is normalized between 0.0 and 1.0, where 0.0 represents the start of the curve and 1.0 represents the end.
 */
typedef struct R3D_Keyframe {
    float time;             ///< Normalized time of the keyframe, ranging from 0.0 to 1.0.
    float value;            ///< The value of the interpolation at this keyframe.
} R3D_Keyframe;

/**
 * @brief Represents an interpolation curve composed of keyframes.
 *
 * This structure contains an array of keyframes and metadata about the array, such as the current number of keyframes
 * and the allocated capacity. The keyframes define a curve that can be used for smooth interpolation between values
 * over a normalized time range (0.0 to 1.0).
 */
typedef struct R3D_InterpolationCurve {
    R3D_Keyframe* keyframes;    ///< Dynamic array of keyframes defining the interpolation curve.
    unsigned int capacity;      ///< Allocated size of the keyframes array.
    unsigned int count;         ///< Current number of keyframes in the array.
} R3D_InterpolationCurve;

/**
 * @struct R3D_Particle
 * @brief Represents a particle in a 3D particle system, with properties
 *        such as position, velocity, rotation, and color modulation.
 */
typedef struct R3D_Particle {

    float lifetime;                 ///< Duration of the particle's existence in seconds.

    Matrix transform;               ///< The particle's current transformation matrix in 3D space.

    Vector3 position;               ///< The current position of the particle in 3D space.
    Vector3 rotation;               ///< The current rotation of the particle in 3D space (Euler angles).
    Vector3 scale;                  ///< The current scale of the particle in 3D space.
    Color color;                    ///< The current color of the particle, representing its color modulation.

    Vector3 velocity;               ///< The current velocity of the particle in 3D space.
    Vector3 angularVelocity;        ///< The current angular velocity of the particle in radians (Euler angles).

    Vector3 baseScale;              ///< The initial scale of the particle in 3D space.
    Vector3 baseVelocity;           ///< The initial velocity of the particle in 3D space.
    Vector3 baseAngularVelocity;    ///< The initial angular velocity of the particle in radians (Euler angles).
    unsigned char baseOpacity;      ///< The initial opacity of the particle, ranging from 0 (fully transparent) to 255 (fully opaque).

} R3D_Particle;

/**
 * @brief Represents a CPU-based particle system with various properties and settings.
 *
 * This structure contains configuration data for a particle system, such as mesh information, initial properties,
 * curves for controlling properties over time, and settings for shadow casting, emission rate, and more.
 */
typedef struct R3D_ParticleSystem {

    R3D_Particle* particles;            ///< Pointer to the array of particles in the system.
    int capacity;                       ///< The maximum number of particles the system can manage.
    int count;                          ///< The current number of active particles in the system.

    Vector3 position;                   ///< The initial position of the particle system. Default: (0, 0, 0).
    Vector3 gravity;                    ///< The gravity applied to the particles. Default: (0, -9.81, 0).

    Vector3 initialScale;               ///< The initial scale of the particles. Default: (1, 1, 1).
    float scaleVariance;                ///< The variance in particle scale. Default: 0.0f.

    Vector3 initialRotation;            ///< The initial rotation of the particles in Euler angles (degrees). Default: (0, 0, 0).
    Vector3 rotationVariance;           ///< The variance in particle rotation in Euler angles (degrees). Default: (0, 0, 0).

    Color initialColor;                 ///< The initial color of the particles. Default: WHITE.
    Color colorVariance;                ///< The variance in particle color. Default: BLANK.

    Vector3 initialVelocity;            ///< The initial velocity of the particles. Default: (0, 0, 0).
    Vector3 velocityVariance;           ///< The variance in particle velocity. Default: (0, 0, 0).

    Vector3 initialAngularVelocity;     ///< The initial angular velocity of the particles in Euler angles (degrees). Default: (0, 0, 0).
    Vector3 angularVelocityVariance;    ///< The variance in angular velocity. Default: (0, 0, 0).

    float lifetime;                     ///< The lifetime of the particles in seconds. Default: 1.0f.
    float lifetimeVariance;             ///< The variance in lifetime in seconds. Default: 0.0f.

    float emissionTimer;                ///< Use to control automatic emission, should not be modified manually.
    float emissionRate;                 ///< The rate of particle emission in particles per second. Default: 10.0f.
    float spreadAngle;                  ///< The angle of propagation of the particles in a cone (degrees). Default: 0.0f.

    R3D_InterpolationCurve* scaleOverLifetime;              ///< Curve controlling the scale evolution of the particles over their lifetime. Default: NULL.
    R3D_InterpolationCurve* speedOverLifetime;              ///< Curve controlling the speed evolution of the particles over their lifetime. Default: NULL.
    R3D_InterpolationCurve* opacityOverLifetime;            ///< Curve controlling the opacity evolution of the particles over their lifetime. Default: NULL.
    R3D_InterpolationCurve* angularVelocityOverLifetime;    ///< Curve controlling the angular velocity evolution of the particles over their lifetime. Default: NULL.

    BoundingBox aabb;                   ///< For frustum culling. Defaults to a large AABB; compute manually via `R3D_CalculateParticleSystemBoundingBox` after setup.

    bool autoEmission;                  /**< Indicates whether particle emission is automatic when calling `R3D_UpdateParticleSystem`.
                                         *   If false, emission is manual using `R3D_EmitParticle`. Default: true.
                                         */

} R3D_ParticleSystem;


/* === Extern C guard === */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @defgroup Core Core Functions
 * @{
 */

// --------------------------------------------
// CORE: Init And Config Functions
// --------------------------------------------

/**
 * @brief Initializes the rendering engine.
 * 
 * This function sets up the internal rendering system with the provided resolution 
 * and state flags, which define the internal behavior. These flags can be modified
 * later via R3D_SetState.
 * 
 * @param resWidth Width of the internal resolution.
 * @param resHeight Height of the internal resolution.
 * @param flags Flags indicating internal behavior (modifiable via R3D_SetState).
 */
R3DAPI void R3D_Init(int resWidth, int resHeight, unsigned int flags);

/**
 * @brief Closes the rendering engine and deallocates all resources.
 * 
 * This function shuts down the rendering system and frees all allocated memory, 
 * including the resources associated with the created lights.
 */
R3DAPI void R3D_Close(void);

/**
 * @brief Checks if a specific internal state flag is set.
 * 
 * @param flag The state flag to check.
 * @return True if the flag is set, false otherwise.
 */
R3DAPI bool R3D_HasState(unsigned int flag);

/**
 * @brief Sets internal state flags for the rendering engine.
 * 
 * This function modifies the behavior of the rendering engine by setting one or more 
 * state flags. Flags can be later cleared with R3D_ClearState.
 * 
 * @param flags The flags to set.
 */
R3DAPI void R3D_SetState(unsigned int flags);

/**
 * @brief Clears specific internal state flags.
 * 
 * This function clears one or more previously set state flags, modifying the 
 * behavior of the rendering engine accordingly.
 * 
 * @param flags The flags to clear.
 */
R3DAPI void R3D_ClearState(unsigned int flags);

/**
 * @brief Gets the current internal resolution.
 * 
 * This function retrieves the current internal resolution being used by the 
 * rendering engine.
 * 
 * @param width Pointer to store the width of the internal resolution.
 * @param height Pointer to store the height of the internal resolution.
 */
R3DAPI void R3D_GetResolution(int* width, int* height);

/**
 * @brief Updates the internal resolution.
 * 
 * This function changes the internal resolution of the rendering engine. Note that 
 * this process destroys and recreates all framebuffers, which may be a slow operation.
 * 
 * @param width The new width for the internal resolution.
 * @param height The new height for the internal resolution.
 * 
 * @warning This function may be slow due to the destruction and recreation of framebuffers.
 */
R3DAPI void R3D_UpdateResolution(int width, int height);

/**
 * @brief Defines the bounds of the scene for directional light calculations.
 * 
 * This function sets the scene bounds used to determine which areas should be illuminated 
 * by directional lights. It is the user's responsibility to calculate and provide the 
 * correct bounds.
 * 
 * @param sceneBounds The bounding box defining the scene's dimensions.
 */
R3DAPI void R3D_SetSceneBounds(BoundingBox sceneBounds);

/**
 * @brief Sets the default texture filtering mode.
 * 
 * This function defines the default texture filter that will be applied to all subsequently 
 * loaded textures, including those used in materials, sprites, and other resources.
 * 
 * If a trilinear or anisotropic filter is selected, mipmaps will be automatically generated 
 * for the textures, but they will not be generated when using nearest or bilinear filtering.
 *
 * The default texture filter mode is `TEXTURE_FILTER_TRILINEAR`.
 * 
 * @param filter The texture filtering mode to be applied by default.
 */
R3DAPI void R3D_SetTextureFilter(TextureFilter filter);

/**
 * @brief Get the currently active global rendering layers.
 *
 * Returns the bitfield representing the currently active layers in the renderer.
 * By default, the internal active layers are set to 0, which means that any
 * non-zero layer assigned to an object will NOT be rendered unless explicitly
 * activated.
 *
 * @return R3D_Layer Bitfield of active layers.
 */
R3DAPI R3D_Layer R3D_GetActiveLayers(void);

/**
 * @brief Set the active global rendering layers.
 *
 * Replaces the current set of active layers with the given bitfield.
 *
 * @param layers Bitfield representing the layers to activate.
 */
R3DAPI void R3D_SetActiveLayers(R3D_Layer layers);

/**
 * @brief Enable one or more layers without affecting other active layers.
 *
 * This function sets the bits in the global active layers corresponding to
 * the bits in the provided bitfield. Layers already active remain active.
 *
 * @param bitfield Bitfield representing one or more layers to enable.
 */
R3DAPI void R3D_EnableLayers(R3D_Layer bitfield);

/**
 * @brief Disable one or more layers without affecting other active layers.
 *
 * This function clears the bits in the global active layers corresponding to
 * the bits in the provided bitfield. Layers not included in the bitfield
 * remain unchanged.
 *
 * @param bitfield Bitfield representing one or more layers to disable.
 */
R3DAPI void R3D_DisableLayers(R3D_Layer bitfield);

// --------------------------------------------
// CORE: Drawing Functions
// --------------------------------------------

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
 * @brief Draws a sprite at a specified position.
 *
 * This function renders a sprite in 3D space at the given position.
 * It supports negative scaling to flip the sprite.
 *
 * @param sprite A pointer to the sprite to render.
 * @param position The position to place the sprite at.
 */
R3DAPI void R3D_DrawSprite(const R3D_Sprite* sprite, Vector3 position);

/**
 * @brief Draws a sprite with size and rotation options.
 *
 * This function allows rendering a sprite with a specified size and rotation.
 * It supports negative size values for flipping the sprite.
 *
 * @param sprite A pointer to the sprite to render.
 * @param position The position to place the sprite at.
 * @param size The size of the sprite (negative values flip the sprite).
 * @param rotation The rotation angle in degrees.
 */
R3DAPI void R3D_DrawSpriteEx(const R3D_Sprite* sprite, Vector3 position, Vector2 size, float rotation);

/**
 * @brief Draws a sprite with full transformation control.
 *
 * This function provides advanced transformation options, allowing
 * customization of size, rotation axis, and rotation angle.
 * It supports all billboard modes, or can be drawn without billboarding.
 *
 * @param sprite A pointer to the sprite to render.
 * @param position The position to place the sprite at.
 * @param size The size of the sprite (negative values flip the sprite).
 * @param rotationAxis The axis around which the sprite rotates.
 * @param rotationAngle The angle to rotate the sprite around the given axis.
 */
R3DAPI void R3D_DrawSpritePro(const R3D_Sprite* sprite, Vector3 position, Vector2 size, Vector3 rotationAxis, float rotationAngle);

/**
 * @brief Draws a 3D sprite with instancing support.
 * 
 * This function renders a 3D sprite multiple times with different transformation matrices 
 * for each instance.
 * 
 * @param sprite A pointer to the sprite to render. Cannot be NULL.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawSpriteInstanced(const R3D_Sprite* sprite, const Matrix* instanceTransforms, int instanceCount);

/**
 * @brief Draws a 3D sprite with instancing support and different colors per instance.
 * 
 * This function renders a 3D sprite multiple times with different transformation matrices 
 * and different colors for each instance.
 * 
 * @param sprite A pointer to the sprite to render. Cannot be NULL.
 * @param instanceTransforms Array of transformation matrices for each instance. Cannot be NULL.
 * @param instanceColors Array of colors for each instance. Can be NULL if no per-instance colors are needed.
 * @param instanceCount The number of instances to render. Must be greater than 0.
 */
R3DAPI void R3D_DrawSpriteInstancedEx(const R3D_Sprite* sprite, const Matrix* instanceTransforms, const Color* instanceColors, int instanceCount);

/**
 * @brief Draws a 3D sprite with instancing support, a global transformation, and different colors per instance.
 *
 * This function renders a 3D sprite multiple times using instancing, with a global transformation
 * applied to all instances, and individual transformation matrices and colors for each instance.
 * Each instance can have its own position, rotation, scale, and color while sharing the same sprite.
 *
 * @param sprite A pointer to the sprite to render. Cannot be NULL.
 * @param globalAabb Optional bounding box encompassing all instances, in local space. Used for frustum culling.
 *                   Can be NULL to disable culling. Will be transformed by the global matrix if provided.
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
R3DAPI void R3D_DrawSpriteInstancedPro(const R3D_Sprite* sprite, const BoundingBox* globalAabb, Matrix globalTransform,
                                       const Matrix* instanceTransforms, int transformsStride,
                                       const Color* instanceColors, int colorsStride,
                                       int instanceCount);

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

/** @} */ // end of Core

/**
 * @defgroup Model Model Functions
 * @{
 */

// --------------------------------------------
// MODEL: Mesh Functions
// --------------------------------------------

/**
 * @brief Generate a polygon mesh with specified number of sides.
 *
 * Creates a regular polygon mesh centered at the origin in the XY plane.
 * The polygon is generated with vertices evenly distributed around a circle.
 *
 * @param sides Number of sides for the polygon (minimum 3).
 * @param radius Radius of the circumscribed circle.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated polygon mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshPoly(int sides, float radius, bool upload);

/**
 * @brief Generate a plane mesh with specified dimensions and resolution.
 *
 * Creates a flat plane mesh in the XZ plane, centered at the origin.
 * The mesh can be subdivided for higher resolution or displacement mapping.
 *
 * @param width Width of the plane along the X axis.
 * @param length Length of the plane along the Z axis.
 * @param resX Number of subdivisions along the X axis.
 * @param resZ Number of subdivisions along the Z axis.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated plane mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshPlane(float width, float length, int resX, int resZ, bool upload);

/**
 * @brief Generate a cube mesh with specified dimensions.
 *
 * Creates a cube mesh centered at the origin with the specified width, height, and length.
 * Each face consists of two triangles with proper normals and texture coordinates.
 *
 * @param width Width of the cube along the X axis.
 * @param height Height of the cube along the Y axis.
 * @param length Length of the cube along the Z axis.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cube mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCube(float width, float height, float length, bool upload);

/**
 * @brief Generate a sphere mesh with specified parameters.
 *
 * Creates a UV sphere mesh centered at the origin using latitude-longitude subdivision.
 * Higher ring and slice counts produce smoother spheres but with more vertices.
 *
 * @param radius Radius of the sphere.
 * @param rings Number of horizontal rings (latitude divisions).
 * @param slices Number of vertical slices (longitude divisions).
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated sphere mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshSphere(float radius, int rings, int slices, bool upload);

/**
 * @brief Generate a hemisphere mesh with specified parameters.
 *
 * Creates a half-sphere mesh (dome) centered at the origin, extending upward in the Y axis.
 * Uses the same UV sphere generation technique as R3D_GenMeshSphere but only the upper half.
 *
 * @param radius Radius of the hemisphere.
 * @param rings Number of horizontal rings (latitude divisions).
 * @param slices Number of vertical slices (longitude divisions).
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated hemisphere mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshHemiSphere(float radius, int rings, int slices, bool upload);

/**
 * @brief Generate a cylinder mesh with specified parameters.
 *
 * Creates a cylinder mesh centered at the origin, extending along the Y axis.
 * The cylinder includes both top and bottom caps and smooth side surfaces.
 *
 * @param radius Radius of the cylinder base.
 * @param height Height of the cylinder along the Y axis.
 * @param slices Number of radial subdivisions around the cylinder.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cylinder mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCylinder(float radius, float height, int slices, bool upload);

/**
 * @brief Generate a cone mesh with specified parameters.
 *
 * Creates a cone mesh with its base centered at the origin and apex pointing upward along the Y axis.
 * The cone includes a circular base and smooth tapered sides.
 *
 * @param radius Radius of the cone base.
 * @param height Height of the cone along the Y axis.
 * @param slices Number of radial subdivisions around the cone base.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cone mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCone(float radius, float height, int slices, bool upload);

/**
 * @brief Generate a torus mesh with specified parameters.
 *
 * Creates a torus (donut shape) mesh centered at the origin in the XZ plane.
 * The torus is defined by a major radius (distance from center to tube center)
 * and a minor radius (tube thickness).
 *
 * @param radius Major radius of the torus (distance from center to tube center).
 * @param size Minor radius of the torus (tube thickness/radius).
 * @param radSeg Number of segments around the major radius.
 * @param sides Number of sides around the tube cross-section.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated torus mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshTorus(float radius, float size, int radSeg, int sides, bool upload);

/**
 * @brief Generate a trefoil knot mesh with specified parameters.
 *
 * Creates a trefoil knot mesh, which is a mathematical knot shape.
 * Similar to a torus but with a twisted, knotted topology.
 *
 * @param radius Major radius of the knot.
 * @param size Minor radius (tube thickness) of the knot.
 * @param radSeg Number of segments around the major radius.
 * @param sides Number of sides around the tube cross-section.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated trefoil knot mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshKnot(float radius, float size, int radSeg, int sides, bool upload);

/**
 * @brief Generate a terrain mesh from a heightmap image.
 *
 * Creates a terrain mesh by interpreting the brightness values of a heightmap image
 * as height values. The resulting mesh represents a 3D terrain surface.
 *
 * @param heightmap Image containing height data (grayscale values represent elevation).
 * @param size 3D vector defining the terrain dimensions (width, max height, depth).
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated heightmap terrain mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshHeightmap(Image heightmap, Vector3 size, bool upload);

/**
 * @brief Generate a voxel-style mesh from a cubicmap image.
 *
 * Creates a mesh composed of cubes based on a cubicmap image, where each pixel
 * represents the presence or absence of a cube at that position. Useful for
 * creating voxel-based or block-based geometry.
 *
 * @param cubicmap Image where pixel values determine cube placement.
 * @param cubeSize 3D vector defining the size of each individual cube.
 * @param upload If true, automatically uploads the mesh to GPU memory.
 *
 * @return Generated cubicmap mesh structure.
 */
R3DAPI R3D_Mesh R3D_GenMeshCubicmap(Image cubicmap, Vector3 cubeSize, bool upload);

/**
 * @brief Free mesh data from both RAM and VRAM.
 *
 * Releases all memory associated with a mesh, including vertex data in RAM
 * and GPU buffers (VAO, VBO, EBO) if the mesh was uploaded to VRAM.
 * After calling this function, the mesh should not be used.
 *
 * @param mesh Pointer to the mesh structure to be freed.
 */
R3DAPI void R3D_UnloadMesh(const R3D_Mesh* mesh);

/**
 * @brief Upload a mesh to GPU memory.
 *
 * This function uploads a mesh's vertex and (optional) index data to the GPU.
 * It creates and configures a VAO, VBO, and optionally an EBO if indices are provided.
 * All vertex attributes are interleaved in a single VBO.
 *
 * This function must only be called once per mesh. For updates, use R3D_UpdateMesh().
 *
 * @param mesh Pointer to the mesh structure containing vertex and index data.
 * @param dynamic If true, allocates buffers with GL_DYNAMIC_DRAW for later updates.
 * If false, uses GL_STATIC_DRAW for optimized static meshes.
 *
 * @return true if upload succeeded, false on error (e.g. invalid input or already uploaded).
 */
R3DAPI bool R3D_UploadMesh(R3D_Mesh* mesh, bool dynamic);

/**
 * @brief Update an already uploaded mesh on the GPU.
 *
 * This function updates the GPU-side data of a mesh previously uploaded with R3D_UploadMesh().
 * It replaces the vertex buffer contents using glBufferSubData.
 * If index data is present, it also updates or creates the index buffer (EBO).
 *
 * This function assumes the mesh was uploaded with the `dynamic` flag set to true.
 *
 * @param mesh Pointer to the mesh structure with updated vertex and/or index data.
 *
 * @return true if update succeeded, false on error (e.g. mesh not uploaded or invalid data).
 */
R3DAPI bool R3D_UpdateMesh(R3D_Mesh* mesh);

/**
 * @brief Recalculate the bounding box of a mesh.
 *
 * Computes and updates the axis-aligned bounding box (AABB) of the mesh
 * by examining all vertex positions. This is useful after mesh deformation
 * or when the bounding box needs to be refreshed.
 *
 * @param mesh Pointer to the mesh structure whose bounding box will be updated.
 */
R3DAPI void R3D_UpdateMeshBoundingBox(R3D_Mesh* mesh);

// --------------------------------------------
// MODEL: Material Functions
// --------------------------------------------

/**
 * @brief Get the default material configuration.
 *
 * Returns a default material with standard properties and default textures.
 * This material can be used as a fallback or starting point for custom materials.
 *
 * @return Default material structure with standard properties.
 */
R3DAPI R3D_Material R3D_GetDefaultMaterial(void);

/**
 * @brief Unload a material and its associated textures.
 *
 * Frees all memory associated with a material, including its textures.
 * This function will unload all textures that are not default textures.
 *
 * @warning Only call this function if you are certain that the textures
 * are not shared with other materials or objects, as this will permanently
 * free the texture data.
 *
 * @param material Pointer to the material structure to be unloaded.
 */
R3DAPI void R3D_UnloadMaterial(const R3D_Material* material);

// --------------------------------------------
// MODEL: Model Functions
// --------------------------------------------

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
 *
 * @return Loaded model structure containing meshes and materials.
 *
 * @note External dependencies (e.g., textures or linked resources) are not supported.
 *       The model data must be fully self-contained. Use embedded formats like .glb to ensure compatibility.
 */
R3DAPI R3D_Model R3D_LoadModelFromMemory(const void* data, unsigned int size);

/**
 * @brief Create a model from a single mesh.
 *
 * Creates a model structure containing a single mesh with a default material.
 * This is useful for procedurally generated meshes or simple geometry.
 *
 * @warning The model's bounding box calculation assumes that the mesh's
 * bounding box has already been computed. Call R3D_UpdateMeshBoundingBox()
 * on the mesh before using this function if needed.
 *
 * @param mesh Pointer to the mesh to be wrapped in a model structure.
 *
 * @return Model structure containing the specified mesh.
 */
R3DAPI R3D_Model R3D_LoadModelFromMesh(const R3D_Mesh* mesh);

/**
 * @brief Unload a model and optionally its materials.
 *
 * Frees all memory associated with a model, including its meshes.
 * Materials can be optionally unloaded as well.
 *
 * @param model Pointer to the model structure to be unloaded.
 * @param unloadMaterials If true, also unloads all materials associated with the model.
 * Set to false if textures are still being used elsewhere to avoid freeing shared resources.
 */
R3DAPI void R3D_UnloadModel(const R3D_Model* model, bool unloadMaterials);

/**
 * @brief Update the bounding box of a model.
 *
 * Recalculates the axis-aligned bounding box (AABB) of the entire model
 * by examining all meshes within the model. Optionally updates individual
 * mesh bounding boxes as well.
 *
 * @param model Pointer to the model structure whose bounding box will be updated.
 * @param updateMeshBoundingBoxes If true, also updates the bounding box of each
 * individual mesh within the model before calculating the model's overall bounding box.
 */
R3DAPI void R3D_UpdateModelBoundingBox(R3D_Model* model, bool updateMeshBoundingBoxes);

/**
 * @brief Loads model animations from a supported file format (e.g., GLTF, IQM).
 *
 * This function parses animation data from the given model file and returns an array
 * of R3D_ModelAnimation structs. The caller is responsible for freeing the returned data
 * using R3D_UnloadModelAnimations().
 *
 * @param filePath Path to the model file containing animation(s).
 * @param animCount Pointer to an integer that will receive the number of animations loaded.
 * @param targetFrameRate Desired frame rate (FPS) to sample the animation at. For example, 30 or 60.
 * @return Pointer to a dynamically allocated array of R3D_ModelAnimation. NULL on failure.
 */
R3DAPI R3D_ModelAnimation* R3D_LoadModelAnimations(const char* filePath, int* animCount, int targetFrameRate);

/**
 * @brief Loads model animations from memory data in a supported format (e.g., GLTF, IQM).
 *
 * This function parses animation data from the given memory buffer and returns an array
 * of R3D_ModelAnimation structs. The caller is responsible for freeing the returned data
 * using R3D_UnloadModelAnimations().
 *
 * @param data Pointer to the model data in memory.
 * @param size Size of the data buffer in bytes.
 * @param animCount Pointer to an integer that will receive the number of animations loaded.
 * @param targetFrameRate Desired frame rate (FPS) to sample the animation at. For example, 30 or 60.
 * @return Pointer to a dynamically allocated array of R3D_ModelAnimation. NULL on failure.
 */
R3DAPI R3D_ModelAnimation* R3D_LoadModelAnimationsFromMemory(const void* data, unsigned int size, int* animCount, int targetFrameRate);

/**
 * @brief Frees memory allocated for model animations.
 *
 * This should be called after you're done using animations loaded via R3D_LoadModelAnimations().
 *
 * @param animations Pointer to the animation array to free.
 * @param animCount Number of animations in the array.
 */
R3DAPI void R3D_UnloadModelAnimations(R3D_ModelAnimation* animations, int animCount);

/**
 * @brief Finds and returns a pointer to a named animation within the array.
 *
 * Searches the given array of animations for one that matches the specified name.
 *
 * @param animations Array of animations to search.
 * @param animCount Number of animations in the array.
 * @param name Name of the animation to find (case-sensitive).
 * @return Pointer to the matching animation, or NULL if not found.
 */
R3DAPI R3D_ModelAnimation* R3D_GetModelAnimation(R3D_ModelAnimation* animations, int animCount, const char* name);

/**
 * @brief Logs the names of all animations in the array (for debugging or inspection).
 *
 * Prints the animation names (and possibly other info) to the standard output or debug console.
 *
 * @param animations Array of animations to list.
 * @param animCount Number of animations in the array.
 */
R3DAPI void R3D_ListModelAnimations(R3D_ModelAnimation* animations, int animCount);

/**
 * @brief Sets the scaling factor applied to models on loading.
 *
 * The functions sets the scaling factor to be used when loading models. This value
 * is only applied to models loaded after this value is set.
 *
 * @param value Scaling factor to be used (i.e. 0.01 for meters to centimeters).
 */
R3DAPI void R3D_SetModelImportScale(float value);

/** @} */ // end of Model

/**
 * @defgroup Lighting Lighting Functions
 * @{
 */

// --------------------------------------------
// LIGHTING: Lights Config Functions
// --------------------------------------------

/**
 * @brief Creates a new light of the specified type.
 *
 * This function creates a light of the given type. The light must be destroyed
 * manually when no longer needed by calling `R3D_DestroyLight`.
 *
 * @param type The type of light to create (directional, spot or omni-directional).
 * @return The ID of the created light.
 */
R3DAPI R3D_Light R3D_CreateLight(R3D_LightType type);

/**
 * @brief Destroys the specified light.
 *
 * This function deallocates the resources associated with the light and makes
 * the light ID invalid. It must be called after the light is no longer needed.
 *
 * @param id The ID of the light to destroy.
 */
R3DAPI void R3D_DestroyLight(R3D_Light id);

/**
 * @brief Checks if a light exists.
 *
 * This function checks if the specified light ID is valid and if the light exists.
 *
 * @param id The ID of the light to check.
 * @return True if the light exists, false otherwise.
 */
R3DAPI bool R3D_IsLightExist(R3D_Light id);

/**
 * @brief Gets the type of a light.
 *
 * This function returns the type of the specified light (directional, spot or omni-directional).
 *
 * @param id The ID of the light.
 * @return The type of the light.
 */
R3DAPI R3D_LightType R3D_GetLightType(R3D_Light id);

/**
 * @brief Checks if a light is active.
 *
 * This function checks whether the specified light is currently active (enabled or disabled).
 *
 * @param id The ID of the light to check.
 * @return True if the light is active, false otherwise.
 */
R3DAPI bool R3D_IsLightActive(R3D_Light id);

/**
 * @brief Toggles the state of a light (active or inactive).
 *
 * This function toggles the state of the specified light, turning it on if it is off,
 * or off if it is on.
 *
 * @param id The ID of the light to toggle.
 */
R3DAPI void R3D_ToggleLight(R3D_Light id);

/**
 * @brief Sets the active state of a light.
 *
 * This function allows manually turning a light on or off by specifying its active state.
 *
 * @param id The ID of the light to set the active state for.
 * @param active True to activate the light, false to deactivate it.
 */
R3DAPI void R3D_SetLightActive(R3D_Light id, bool active);

/**
 * @brief Gets the color of a light.
 *
 * This function retrieves the color of the specified light as a `Color` structure.
 *
 * @param id The ID of the light.
 * @return The color of the light as a `Color` structure.
 */
R3DAPI Color R3D_GetLightColor(R3D_Light id);

/**
 * @brief Gets the color of a light as a `Vector3`.
 *
 * This function retrieves the color of the specified light as a `Vector3`, where each
 * component (x, y, z) represents the RGB values of the light.
 *
 * @param id The ID of the light.
 * @return The color of the light as a `Vector3`.
 */
R3DAPI Vector3 R3D_GetLightColorV(R3D_Light id);

/**
 * @brief Sets the color of a light.
 *
 * This function sets the color of the specified light using a `Color` structure.
 *
 * @param id The ID of the light.
 * @param color The new color to set for the light.
 */
R3DAPI void R3D_SetLightColor(R3D_Light id, Color color);

/**
 * @brief Sets the color of a light using a `Vector3`.
 *
 * This function sets the color of the specified light using a `Vector3`, where each
 * component (x, y, z) represents the RGB values of the light.
 *
 * @param id The ID of the light.
 * @param color The new color to set for the light as a `Vector3`.
 */
R3DAPI void R3D_SetLightColorV(R3D_Light id, Vector3 color);

/**
 * @brief Gets the position of a light.
 *
 * This function retrieves the position of the specified light.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @return The position of the light as a `Vector3`.
 */
R3DAPI Vector3 R3D_GetLightPosition(R3D_Light id);

/**
 * @brief Sets the position of a light.
 *
 * This function sets the position of the specified light.
 * Only applicable to spot lights or omni-lights.
 *
 * @note Has no effect for directional lights.
 *       If called on a directional light, 
 *       a warning will be logged.
 *
 * @param id The ID of the light.
 * @param position The new position to set for the light.
 */
R3DAPI void R3D_SetLightPosition(R3D_Light id, Vector3 position);

/**
 * @brief Gets the direction of a light.
 *
 * This function retrieves the direction of the specified light.
 * Only applicable to directional lights or spot lights.
 *
 * @param id The ID of the light.
 * @return The direction of the light as a `Vector3`.
 */
R3DAPI Vector3 R3D_GetLightDirection(R3D_Light id);

/**
 * @brief Sets the direction of a light.
 *
 * This function sets the direction of the specified light.
 * Only applicable to directional lights or spot lights.
 *
 * @note Has no effect for omni-directional lights.
 *       If called on an omni-directional light,
 *       a warning will be logged.
 *
 * @param id The ID of the light.
 * @param direction The new direction to set for the light.
 *                  The vector is automatically normalized.
 */
R3DAPI void R3D_SetLightDirection(R3D_Light id, Vector3 direction);

/**
 * @brief Sets the position and direction of a light to look at a target point.
 *
 * This function sets both the position and the direction of the specified light,
 * causing it to "look at" a given target point.
 *
 * @note - For directional lights, only the direction is updated (position is ignored).
 *       - For omni-directional lights, only the position is updated (direction is not calculated).
 *       - For spot lights, both position and direction are set accordingly.
 *       - This function does **not** emit any warning or log message.
 *
 * @param id The ID of the light.
 * @param position The position to set for the light.
 * @param target The point the light should look at.
 */
R3DAPI void R3D_LightLookAt(R3D_Light id, Vector3 position, Vector3 target);

/**
 * @brief Gets the energy level of a light.
 *
 * This function retrieves the energy level (intensity) of the specified light.
 * Energy typically affects the brightness of the light.
 *
 * @param id The ID of the light.
 * @return The energy level of the light.
 */
R3DAPI float R3D_GetLightEnergy(R3D_Light id);

/**
 * @brief Sets the energy level of a light.
 *
 * This function sets the energy (intensity) of the specified light.
 * A higher energy value will result in a brighter light.
 *
 * @param id The ID of the light.
 * @param energy The new energy value to set for the light.
 */
R3DAPI void R3D_SetLightEnergy(R3D_Light id, float energy);

/**
 * @brief Gets the specular intensity of a light.
 *
 * This function retrieves the current specular intensity of the specified light.
 * Specular intensity affects how shiny surfaces appear when reflecting the light.
 *
 * @param id The ID of the light.
 * @return The current specular intensity of the light.
 */
R3DAPI float R3D_GetLightSpecular(R3D_Light id);

/**
 * @brief Sets the specular intensity of a light.
 *
 * This function sets the specular intensity of the specified light.
 * Higher specular values result in stronger and sharper highlights on reflective surfaces.
 *
 * @param id The ID of the light.
 * @param specular The new specular intensity value to set for the light.
 */
R3DAPI void R3D_SetLightSpecular(R3D_Light id, float specular);

/**
 * @brief Gets the range of a light.
 *
 * This function retrieves the range of the specified light, which determines how far the light can affect.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @return The range of the light.
 */
R3DAPI float R3D_GetLightRange(R3D_Light id);

/**
 * @brief Sets the range of a light.
 *
 * This function sets the range of the specified light.
 * The range determines how far the light can illuminate the scene before it fades out.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @param range The new range to set for the light.
 */
R3DAPI void R3D_SetLightRange(R3D_Light id, float range);

/**
 * @brief Gets the attenuation factor of a light.
 *
 * This function retrieves the attenuation factor of the specified light.
 * Attenuation controls how the intensity of a light decreases with distance.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @return The attenuation factor of the light.
 */
R3DAPI float R3D_GetLightAttenuation(R3D_Light id);

/**
 * @brief Sets the attenuation factor of a light.
 *
 * This function sets the attenuation factor of the specified light.
 * A higher attenuation value causes the light to lose intensity more quickly as the distance increases.
 * For a realistic effect, an attenuation factor of 2.0f is typically used.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @param attenuation The new attenuation factor to set for the light.
 */
R3DAPI void R3D_SetLightAttenuation(R3D_Light id, float attenuation);

/**
 * @brief Gets the inner cutoff angle of a spotlight.
 *
 * This function retrieves the inner cutoff angle of a spotlight.
 * The inner cutoff defines the cone of light where the light is at full intensity.
 *
 * @param id The ID of the light.
 * @return The inner cutoff angle in degrees of the spotlight.
 */
R3DAPI float R3D_GetLightInnerCutOff(R3D_Light id);

/**
 * @brief Sets the inner cutoff angle of a spotlight.
 *
 * This function sets the inner cutoff angle of a spotlight.
 * The inner cutoff angle defines the cone where the light is at full intensity.
 * Anything outside this cone starts to fade.
 *
 * @param id The ID of the light.
 * @param degrees The new inner cutoff angle in degrees.
 */
R3DAPI void R3D_SetLightInnerCutOff(R3D_Light id, float degrees);

/**
 * @brief Gets the outer cutoff angle of a spotlight.
 *
 * This function retrieves the outer cutoff angle of a spotlight.
 * The outer cutoff defines the outer boundary of the light's cone, where the light starts to fade.
 *
 * @param id The ID of the light.
 * @return The outer cutoff angle in degrees of the spotlight.
 */
R3DAPI float R3D_GetLightOuterCutOff(R3D_Light id);

/**
 * @brief Sets the outer cutoff angle of a spotlight.
 *
 * This function sets the outer cutoff angle of a spotlight.
 * The outer cutoff defines the boundary of the light's cone where the light intensity starts to gradually decrease.
 *
 * @param id The ID of the light.
 * @param degrees The new outer cutoff angle in degrees.
 */
R3DAPI void R3D_SetLightOuterCutOff(R3D_Light id, float degrees);

// --------------------------------------------
// LIGHTING: Shadow Config Functions
// --------------------------------------------

/**
 * @brief Enables shadow casting for a light and sets the resolution of its shadow map.
 *
 * This function enables shadow casting for a specified light and allocates a shadow map with the specified resolution.
 * Shadows can be rendered from the light based on this shadow map.
 *
 * @param id The ID of the light for which shadows should be enabled.
 * @param resolution The resolution of the shadow map to be used by the light.
 */
R3DAPI void R3D_EnableShadow(R3D_Light id, int resolution);

/**
 * @brief Disables shadow casting for a light and optionally destroys its shadow map.
 *
 * This function disables shadow casting for the specified light and optionally frees the memory
 * used by its shadow map. If `destroyMap` is true, the shadow map will be destroyed, otherwise,
 * the map will be retained but the light will no longer cast shadows.
 *
 * @param id The ID of the light for which shadows should be disabled.
 * @param destroyMap Whether or not to destroy the shadow map associated with the light.
 */
R3DAPI void R3D_DisableShadow(R3D_Light id, bool destroyMap);

/**
 * @brief Checks if shadow casting is enabled for a light.
 *
 * This function checks if shadow casting is currently enabled for the specified light.
 *
 * @param id The ID of the light.
 * @return True if shadow casting is enabled, false otherwise.
 */
R3DAPI bool R3D_IsShadowEnabled(R3D_Light id);

/**
 * @brief Checks if a light has an associated shadow map.
 *
 * This function checks if the specified light has a shadow map allocated for it.
 *
 * @param id The ID of the light.
 * @return True if the light has a shadow map, false otherwise.
 */
R3DAPI bool R3D_HasShadowMap(R3D_Light id);

/**
 * @brief Gets the shadow map update mode of a light.
 *
 * This function retrieves the current mode for updating the shadow map of a light. The mode can be:
 * - Interval: Updates the shadow map at a fixed interval.
 * - Continuous: Updates the shadow map continuously.
 * - Manual: Updates the shadow map manually (via explicit function calls).
 *
 * @param id The ID of the light.
 * @return The shadow map update mode.
 */
R3DAPI R3D_ShadowUpdateMode R3D_GetShadowUpdateMode(R3D_Light id);

/**
 * @brief Sets the shadow map update mode of a light.
 *
 * This function sets the mode for updating the shadow map of the specified light.
 * The update mode controls when and how often the shadow map is refreshed.
 *
 * @param id The ID of the light.
 * @param mode The update mode to set for the shadow map (Interval, Continuous, or Manual).
 */
R3DAPI void R3D_SetShadowUpdateMode(R3D_Light id, R3D_ShadowUpdateMode mode);

/**
 * @brief Gets the frequency of shadow map updates for the interval update mode.
 *
 * This function retrieves the frequency (in milliseconds) at which the shadow map should be updated when
 * the interval update mode is enabled. This function is only relevant if the shadow map update mode is set
 * to "Interval".
 *
 * @param id The ID of the light.
 * @return The frequency in milliseconds at which the shadow map is updated.
 */
R3DAPI int R3D_GetShadowUpdateFrequency(R3D_Light id);

/**
 * @brief Sets the frequency of shadow map updates for the interval update mode.
 *
 * This function sets the frequency (in milliseconds) at which the shadow map should be updated when
 * the interval update mode is enabled. This function is only relevant if the shadow map update mode is set
 * to "Interval".
 *
 * @param id The ID of the light.
 * @param msec The frequency in milliseconds at which to update the shadow map.
 */
R3DAPI void R3D_SetShadowUpdateFrequency(R3D_Light id, int msec);

/**
 * @brief Forces an immediate update of the shadow map during the next rendering pass.
 *
 * This function forces the shadow map of the specified light to be updated during the next call to `R3D_End`.
 * This is primarily used for the manual update mode, but may also work for the interval mode.
 *
 * @param id The ID of the light.
 */
R3DAPI void R3D_UpdateShadowMap(R3D_Light id);

/**
 * @brief Retrieves the softness radius used to simulate penumbra in shadows.
 *
 * The softness is expressed as a sampling radius in texels within the shadow map.
 *
 * @param id The ID of the light.
 * @return The softness radius in texels currently set for the shadow.
 */
R3DAPI float R3D_GetShadowSoftness(R3D_Light id);

/**
 * @brief Sets the softness radius used to simulate penumbra in shadows.
 *
 * This function adjusts the softness of the shadow edges for the specified light.
 * The softness value corresponds to a number of texels in the shadow map, independent
 * of its resolution. Larger values increase the blur radius, resulting in softer,
 * more diffuse shadows, while smaller values yield sharper shadows.
 *
 * @param id The ID of the light.
 * @param softness The softness radius in texels to apply (must be >= 0).
 *
 * @note The softness must be set only after shadows have been enabled for the light,
 *       since the shadow map resolution must be known before the softness can be applied.
 */
R3DAPI void R3D_SetShadowSoftness(R3D_Light id, float softness);

/**
 * @brief Gets the shadow depth bias value.
 */
R3DAPI float R3D_GetShadowDepthBias(R3D_Light id);

/**
 * @brief Sets the shadow depth bias value.
 *
 * A higher bias helps reduce "shadow acne" artifacts
 * (shadows flickering or appearing misaligned on surfaces).
 * Be careful: too large values may cause shadows to look detached
 * or floating away from objects.
 */
R3DAPI void R3D_SetShadowDepthBias(R3D_Light id, float value);

/**
 * @brief Gets the shadow slope bias value.
 */
R3DAPI float R3D_GetShadowSlopeBias(R3D_Light id);

/**
 * @brief Sets the shadow slope bias value.
 *
 * This bias mainly compensates artifacts on surfaces angled
 * relative to the light. It helps prevent shadows from
 * incorrectly appearing or disappearing along object edges.
 */
R3DAPI void R3D_SetShadowSlopeBias(R3D_Light id, float value);

// --------------------------------------------
// LIGHTING: Light Helper Functions
// --------------------------------------------

/**
 * @brief Returns the bounding box encompassing the light's area of influence.
 *
 * This function computes the axis-aligned bounding box (AABB) that encloses the 
 * volume affected by the specified light, based on its type:
 * 
 * - For spotlights, the bounding box encloses the light cone.
 * - For omni-directional lights, it encloses a sphere representing the light's range.
 * - For directional lights, it returns an infinite bounding box to represent global influence.
 *
 * This bounding box is primarily useful for spatial partitioning, culling, or visual debugging.
 * 
 * @param light The light for which to compute the bounding box.
 * 
 * @return A BoundingBox struct that encloses the light's influence volume.
 */
R3DAPI BoundingBox R3D_GetLightBoundingBox(R3D_Light light);

/**
 * @brief Draws the area of influence of the light in 3D space.
 *
 * This function visualizes the area affected by a light in 3D space.
 * It draws the light's influence, such as the cone for spotlights or the volume for omni-lights.
 * This function is only relevant for spotlights and omni-lights.
 * 
 * @note This function should be called while using the default 3D rendering mode of raylib, 
 *       not with R3D's rendering mode. It uses raylib's 3D drawing functions to render the light's shape.
 *
 * @param id The ID of the light.
 */
R3DAPI void R3D_DrawLightShape(R3D_Light id);

/** @} */ // end of Lighting

/**
 * @defgroup Particles Particle Functions
 * @{
 */

// --------------------------------------------
// PARTICLES: Particle System Functions
// --------------------------------------------

/**
 * @brief Loads a particle emitter system for the CPU.
 *
 * This function initializes a particle emitter system on the CPU with a specified maximum
 * number of particles. It prepares the necessary data structures and allocates memory
 * for managing the particles.
 *
 * @param maxParticles The maximum number of particles the system can handle at once.
 *                     This value determines the memory allocation and performance constraints.
 * @return A newly initialized `R3D_ParticleSystem` structure.
 *         The caller is responsible for properly managing and freeing the system when no longer needed.
 */
R3DAPI R3D_ParticleSystem R3D_LoadParticleSystem(int maxParticles);

/**
 * @brief Unloads the particle emitter system and frees allocated memory.
 *
 * This function deallocates the memory used by the particle emitter system and clears the associated resources.
 * It should be called when the particle system is no longer needed to prevent memory leaks.
 *
 * @param system A pointer to the `R3D_ParticleSystem` to be unloaded.
 */
R3DAPI void R3D_UnloadParticleSystem(R3D_ParticleSystem* system);

/**
 * @brief Emits a particle in the particle system.
 *
 * This function triggers the emission of a new particle in the particle system. It handles the logic of adding a new
 * particle to the system and initializing its properties based on the current state of the system.
 *
 * @param system A pointer to the `R3D_ParticleSystemCPU` where the particle will be emitted.
 * @return `true` if the particle was successfully emitted, `false` if the system is at full capacity and cannot emit more particles.
 */
R3DAPI bool R3D_EmitParticle(R3D_ParticleSystem* system);

/**
 * @brief Updates the particle emitter system by advancing particle positions.
 *
 * This function updates the positions and properties of particles in the system based on the elapsed time. It handles
 * simulation of particle movement, gravity, and other physics-based calculations.
 *
 * @param system A pointer to the `R3D_ParticleSystem` to be updated.
 * @param deltaTime The time elapsed since the last update (in seconds).
 */
R3DAPI void R3D_UpdateParticleSystem(R3D_ParticleSystem* system, float deltaTime);

/**
 * @brief Computes and updates the AABB (Axis-Aligned Bounding Box) of a particle system.
 *
 * This function simulates the particle system to estimate the region of space it occupies.
 * It considers particle positions at mid-life and end-of-life to approximate the AABB,
 * which is then stored in the system's `aabb` field. This is useful for enabling frustum culling,
 * especially when the bounds are not known beforehand.
 *
 * @param system Pointer to the `R3D_ParticleSystem` to update.
 */
R3DAPI void R3D_CalculateParticleSystemBoundingBox(R3D_ParticleSystem* system);

/** @} */ // end of Particles

/**
 * @defgroup Sprites Sprite Functions
 * @{
 */

// --------------------------------------------
// SPRITE: Sprite Functions
// --------------------------------------------

/**
 * @brief Load a sprite from a texture.
 *
 * This function creates a `R3D_Sprite` using the provided texture.
 * The texture will be used as the albedo of the sprite's material.
 * The default billboard mode applied to the material is `R3D_BILLBOARD_Y_AXIS`.
 *
 * @warning The lifetime of the provided texture is managed by the caller.
 *
 * @param texture The `Texture2D` to be used for the sprite.
 * @param xFrameCount The number of frames in the horizontal direction.
 * @param yFrameCount The number of frames in the vertical direction.
 *
 * @return A `R3D_Sprite` object initialized with the given texture and frame configuration.
 */
R3DAPI R3D_Sprite R3D_LoadSprite(Texture2D texture, int xFrameCount, int yFrameCount);

/**
 * @brief Unload a sprite and free associated resources.
 *
 * This function releases the resources allocated for a `R3D_Sprite`.
 * It should be called when the sprite is no longer needed.
 *
 * @warning This function only unloads non-default textures from the sprite's material,
 * so make sure these textures are not shared with other material instances elsewhere.
 *
 * @param sprite A pointer to the `R3D_Sprite` to be unloaded.
 */
R3DAPI void R3D_UnloadSprite(const R3D_Sprite* sprite);

/**
 * @brief Updates the animation of a sprite.
 *
 * This function updates the current frame of the sprite's animation based on the provided speed. The animation frames are read from
 * right to left, advancing the cursor to the next row after completing a line.
 *
 * @note The `speed` parameter can be calculated as the number of frames per second multiplied by `GetFrameTime()`.
 *
 * @param sprite A pointer to the `R3D_Sprite` to update.
 * @param speed The speed at which the animation progresses, in frames per second.
 */
R3DAPI void R3D_UpdateSprite(R3D_Sprite* sprite, float speed);

/**
 * @brief Updates the animation of a sprite with specified frame boundaries.
 *
 * This function updates the current frame of the sprite's animation while restricting it between `firstFrame` and `lastFrame`.
 * This is useful for spritesheets containing multiple animations.
 *
 * @note The frames are read from right to left, and the cursor moves to the next row after completing a line.
 * @note The `speed` parameter can be calculated as the number of frames per second multiplied by `GetFrameTime()`.
 *
 * @param sprite A pointer to the `R3D_Sprite` to update.
 * @param firstFrame The first frame of the animation loop.
 * @param lastFrame The last frame of the animation loop.
 * @param speed The speed at which the animation progresses, in frames per second.
 */
R3DAPI void R3D_UpdateSpriteEx(R3D_Sprite* sprite, int firstFrame, int lastFrame, float speed);

/** @} */ // end of Sprites

/**
 * @defgroup Curves Curve Functions
 * @brief The interpolation curves defined in this module are used in the context of particle systems.
 * @{
 */

// --------------------------------------------
// CURVES: Interpolation Curves Functions
// --------------------------------------------

/**
 * @brief Loads an interpolation curve with a specified initial capacity.
 *
 * This function initializes an interpolation curve with the given capacity. The capacity represents the initial size of
 * the memory allocated for the curve. You can add keyframes to the curve using `R3D_AddKeyframe`. If adding a keyframe
 * exceeds the initial capacity, the system will automatically reallocate memory and double the initial capacity.
 *
 * @param capacity The initial capacity (size) of the interpolation curve. This is the number of keyframes that can be added
 *                 before a reallocation occurs.
 * @return An initialized interpolation curve with the specified capacity.
 */
R3DAPI R3D_InterpolationCurve R3D_LoadInterpolationCurve(int capacity);

/**
 * @brief Unloads the interpolation curve and frees the allocated memory.
 *
 * This function deallocates the memory associated with the interpolation curve and clears any keyframes stored in it.
 * It should be called when the curve is no longer needed to avoid memory leaks.
 *
 * @param curve The interpolation curve to be unloaded.
 */
R3DAPI void R3D_UnloadInterpolationCurve(R3D_InterpolationCurve curve);

/**
 * @brief Adds a keyframe to the interpolation curve.
 *
 * This function adds a keyframe to the given interpolation curve at a specific time and value. If the addition of the
 * keyframe requires reallocating memory and the reallocation fails, the previously allocated memory and keyframes are
 * preserved, but the new keyframe is not added.
 *
 * @param curve A pointer to the interpolation curve to which the keyframe will be added.
 * @param time The time at which the keyframe will be added.
 * @param value The value associated with the keyframe.
 * @return `true` if the keyframe was successfully added, or `false` if the reallocation failed.
 */
R3DAPI bool R3D_AddKeyframe(R3D_InterpolationCurve* curve, float time, float value);

/**
 * @brief Evaluates the interpolation curve at a specific time.
 *
 * This function evaluates the value of the interpolation curve at a given time. The curve will interpolate between
 * keyframes based on the time provided.
 *
 * @param curve The interpolation curve to be evaluated.
 * @param time The time at which to evaluate the curve.
 * @return The value of the curve at the specified time.
 */
R3DAPI float R3D_EvaluateCurve(R3D_InterpolationCurve curve, float time);

/** @} */ // end of Curves

/**
 * @defgroup Environment Environment Functions
 * @brief Mainly defines post process control.
 * @{
 */

// --------------------------------------------
// ENVIRONMENT: Background And Ambient
// --------------------------------------------

/**
 * @brief Sets the background color when no skybox is enabled.
 *
 * This function defines the background color to be used when no skybox is active.
 * The color will be used for the clear color of the scene.
 *
 * @param color The color to set as the background color.
 */
R3DAPI void R3D_SetBackgroundColor(Color color);

/**
 * @brief Sets the ambient light color when no skybox is enabled.
 *
 * This function defines the ambient light color to be used when no skybox is active.
 * It affects the overall lighting of the scene when no skybox is present.
 *
 * @param color The color to set for ambient light.
 */
R3DAPI void R3D_SetAmbientColor(Color color);

/**
 * @brief Sets the energy level for ambient light when no skybox is enabled.
 *
 * This function defines the ambient light energy to be used when no skybox is active.
 * Applied multiplicatively to the base ambient color.
 *
 * @param energy The energy to set for ambient light.
 */
R3DAPI void R3D_SetAmbientEnergy(float energy);

/**
 * @brief Enables a skybox for the scene.
 *
 * This function enables a skybox in the scene, replacing the default background with
 * a 3D environment. The skybox is defined by the specified skybox asset.
 *
 * @param skybox The skybox to enable.
 */
R3DAPI void R3D_EnableSkybox(R3D_Skybox skybox);

/**
 * @brief Disables the skybox in the scene.
 *
 * This function disables the skybox, reverting back to the default background
 * color (or no background if none is set). It should be called to remove the skybox
 * from the scene.
 */
R3DAPI void R3D_DisableSkybox(void);

/**
 * @brief Sets the rotation of the skybox.
 *
 * This function allows you to specify the rotation of the skybox along the
 * pitch, yaw, and roll axes, which allows the skybox to be rotated in the scene.
 *
 * @param pitch The rotation angle around the X-axis (in degrees).
 * @param yaw The rotation angle around the Y-axis (in degrees).
 * @param roll The rotation angle around the Z-axis (in degrees).
 */
R3DAPI void R3D_SetSkyboxRotation(float pitch, float yaw, float roll);

/**
 * @brief Gets the current rotation of the skybox.
 *
 * This function returns the current rotation of the skybox as a vector containing
 * the pitch, yaw, and roll values in degrees.
 *
 * @return A vector containing the current pitch, yaw, and roll of the skybox.
 */
R3DAPI Vector3 R3D_GetSkyboxRotation(void);

/**
 * @brief Sets the intensity scaling values used for the environment's skybox.
 *
 * This function controls the intensity of both the rendered skybox as well as
 * the light that is generated from the skybox.
 *
 * @param background The intensity of the skybox rendered as the background.
 *                   A value of 0.0 will disable rendering the skybox but
 *                   allow any generated lighting to still be applied.
 * @param ambient The intensity of ambient light produced by the skybox.
 * @param reflection The intensity of reflections of the skybox in reflective materials.
 */
R3DAPI void R3D_SetSkyboxIntensity(float background, float ambient, float reflection);

/**
 * @brief Gets the intensity scaling values used for the environment's skybox.
 *
 * This function returns the intensity values for the rendered skybox as well
 * the light that is generated from the skybox.
 *
 * @param background Pointer to store the intensity value for the rendered skybox.
 * @param ambient Pointer to store the intensity value for ambient light produced by the skybox.
 * @param reflection Pointer to store the intensity value for reflections from the skybox.
 */
R3DAPI void R3D_GetSkyboxIntensity(float* background, float* ambient, float* reflection);

// --------------------------------------------
// ENVIRONMENT: SSAO Config Functions
// --------------------------------------------

/**
 * @brief Enables or disables Screen Space Ambient Occlusion (SSAO).
 *
 * This function toggles the SSAO effect. When enabled, SSAO enhances the realism
 * of the scene by simulating ambient occlusion, darkening areas where objects
 * are close together or in corners.
 *
 * @param enabled Whether to enable or disable SSAO.
 *
 * Default: false
 */
R3DAPI void R3D_SetSSAO(bool enabled);

/**
 * @brief Gets the current state of SSAO.
 *
 * This function checks if SSAO is currently enabled or disabled.
 *
 * @return True if SSAO is enabled, false otherwise.
 */
R3DAPI bool R3D_GetSSAO(void);

/**
 * @brief Sets the radius for SSAO effect.
 *
 * This function sets the radius used by the SSAO effect to calculate occlusion.
 * A higher value will affect a larger area around each pixel, while a smaller value
 * will create sharper and more localized occlusion.
 *
 * @param value The radius value to set for SSAO.
 *
 * Default: 0.5
 */
R3DAPI void R3D_SetSSAORadius(float value);

/**
 * @brief Gets the current SSAO radius.
 *
 * This function retrieves the current radius value used by the SSAO effect.
 *
 * @return The radius value for SSAO.
 */
R3DAPI float R3D_GetSSAORadius(void);

/**
 * @brief Sets the bias for SSAO effect.
 *
 * This function sets the bias used by the SSAO effect to adjust how much occlusion
 * is applied to the scene. A higher value can reduce artifacts, but may also
 * result in less pronounced ambient occlusion.
 *
 * @param value The bias value for SSAO.
 *
 * Default: 0.025
 */
R3DAPI void R3D_SetSSAOBias(float value);

/**
 * @brief Gets the current SSAO bias.
 *
 * This function retrieves the current bias value used by the SSAO effect.
 *
 * @return The SSAO bias value.
 */
R3DAPI float R3D_GetSSAOBias(void);

/**
 * @brief Sets the number of blur iterations for the SSAO effect.
 *
 * This function sets the number of blur iterations applied to the SSAO effect.
 * By default, one iteration is performed, using a total of 12 samples for the
 * Gaussian blur. Increasing the number of iterations results in a smoother
 * ambient occlusion but may impact performance.
 *
 * @param value The number of blur iterations for SSAO.
 *
 * Default: 1
 */
R3DAPI void R3D_SetSSAOIterations(int value);

/**
 * @brief Gets the current number of blur iterations for the SSAO effect.
 *
 * This function retrieves the current number of blur iterations applied to the SSAO effect.
 *
 * @return The number of blur iterations for SSAO.
 */
R3DAPI int R3D_GetSSAOIterations(void);

/**
 * @brief Sets the intensity multiplier for the SSAO effect.
 *
 * This function sets the the base multiplier used by the SSAO effect.
 * Higher values will result in darker occlusion.
 *
 * @param value The intensity multiplier for SSAO.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetSSAOIntensity(float value);

/**
 * @brief Gets the intensity multiplier for the SSAO effect.
 *
 * This function retrieves the intensity multiplier applied to the SSAO effect.
 *
 * @return The intensity multiplier for SSAO.
 */
R3DAPI float R3D_GetSSAOIntensity(void);

/**
 * @brief Sets the power factor for the SSAO effect.
 *
 * This function sets the exponential distributon applied to the SSAO effect.
 * Higher values will result in darker occlusion with an increasingly sharper
 * falloff compared to the SSAO intensity value.
 *
 * @param value The power factor for SSAO.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetSSAOPower(float value);

/**
 * @brief Gets the power factor used for the SSAO effect.
 *
 * This function retrieves the exponential distributon value applied to the SSAO effect.
 *
 * @return The power factor for SSAO.
 */
R3DAPI float R3D_GetSSAOPower(void);

/**
 * @brief Controls the influence of SSAO on direct lighting.
 *
 * This function sets the amount of direct light attenuation from the SSAO effect.
 * Values greater than 0.0 will apply the SSAO effect to direct lighting with
 * increasing intensity at higher values. This is in addition to the typical
 * application to ambient light only.
 *
 * @param value SSAO effect intensity on direct light, in the range [0.0f, 1.0f].
 *
 * Default: 0.0
 */
R3DAPI void R3D_SetSSAOLightAffect(float value);

/**
 * @brief Gets the current direct lighting effect of SSAO.
 *
 * This function retrieves the value used for direct light attenuation from the SSAO effect.
 *
 * @return SSAO effect intensity on direct light, in the range [0.0f, 1.0f].
 */
R3DAPI float R3D_GetSSAOLightAffect(void);

// --------------------------------------------
// ENVIRONMENT: Bloom Config Functions
// --------------------------------------------

/**
 * @brief Sets the bloom mode.
 *
 * This function configures the bloom effect mode, which determines how the bloom
 * effect is applied to the rendered scene.
 *
 * @param mode The bloom mode to set.
 *
 * Default: R3D_BLOOM_DISABLED
 */
R3DAPI void R3D_SetBloomMode(R3D_Bloom mode);

/**
 * @brief Gets the current bloom mode.
 *
 * This function retrieves the bloom mode currently applied to the scene.
 *
 * @return The current bloom mode.
 */
R3DAPI R3D_Bloom R3D_GetBloomMode(void);

/**
 * @brief Sets the number of mipmap levels used for the bloom effect.
 *
 * This function controls how many mipmap level are generated for use in the bloom effect.
 * More levels will give a smoother and more widely dispersed effect, while less mipmaps
 * can provide a tighter effect. Setting this value to 0 will result in the maximum
 * possible amount of levels to be used. Use of this function will rebuild the
 * mipmaps and may give a one time performance hit.
 *
 * @param value The number of mipmap level to be used for the bloom effect.
 *
 * Default: 7
 */
R3DAPI void R3D_SetBloomLevels(int value);

/**
 * @brief Gets the current amount of mipmap levels used for the bloom effect.
 *
 * This function retrieves the current amount of mipmap levels in use by the bloom effect.
 *
 * @return The number of mipmap level currently used for the bloom effect.
 */
R3DAPI int R3D_GetBloomLevels(void);

/**
 * @brief Sets the bloom intensity.
 *
 * This function controls the strength of the bloom effect. Higher values result
 * in a more intense glow effect on bright areas of the scene.
 *
 * @param value The intensity value for bloom.
 *
 * Default: 0.05
 */
R3DAPI void R3D_SetBloomIntensity(float value);

/**
 * @brief Gets the current bloom intensity.
 *
 * This function retrieves the intensity value of the bloom effect.
 *
 * @return The current bloom intensity.
 */
R3DAPI float R3D_GetBloomIntensity(void);

/**
 * @brief Sets the bloom filter radius.
 *
 * Controls the radius of the blur filter applied during the upscaling stage
 * of the bloom effect. A larger radius results in a wider glow around bright
 * objects, creating a softer and more diffuse bloom. A value of 0 disables 
 * the filtering effect, preserving sharp bloom highlights.
 *
 * @param value The radius of the bloom filter (in pixels).
 *
 * Default: 0
 */
R3DAPI void R3D_SetBloomFilterRadius(int value);

 /**
  * @brief Gets the current bloom filter radius.
  *
  * Retrieves the current radius used for the bloom filter. This value determines
  * how far the glow effect extends around bright areas in the scene.
  *
  * @return The current bloom filter radius.
  */
R3DAPI int R3D_GetBloomFilterRadius(void);

/**
 * @brief Sets the bloom brightness threshold.
 *
 * Controls the brightness cutoff used during the downsampling stage of the
 * bloom effect. If the color channel with the brightest value is below the
 * set threshold the pixel will not be included in the bloom effect.
 *
 * @param value The lowest value to be included the bloom effect (in color value depending on implementation).
 *
 * Default: 0.0
 */
R3DAPI void R3D_SetBloomThreshold(float value);

/**
 * @brief Gets the bloom brightness threshold.
 *
 * Retrieves the current brightness cutoff used for the bloom effect. This value
 * determines if a pixel will be included in the bloom effect based on it's brightness.
 *
 * @return The current bloom brightness cutoff threshold.
 */
R3DAPI float R3D_GetBloomThreshold(void);

/**
 * @brief Sets the bloom brightness threshold's softness.
 *
 * Controls the softness of the cutoff between being include or excluded in the
 * bloom effect. A value of 0 will result in a hard transition between being
 * included or excluded, while larger values will give an increasingly
 * softer transition.
 *
 * @param value The value of of the bloom brightness threshold's softness.
 *
 * Default: 0.5
 */
R3DAPI void R3D_SetBloomSoftThreshold(float value);

/**
 * @brief Gets the current bloom brightness threshold's softness.
 *
 * Retrieves the softness of the brightness cutoff for the bloom effect.
 * This value determines the softness of the transition between being
 * included or excluded in the bloom effect
 *
 * @return The current bloom brightness threshold's softness.
 */
R3DAPI float R3D_GetBloomSoftThreshold(void);

// --------------------------------------------
// ENVIRONMENT: SSR Config Functions
// --------------------------------------------

/**
 * @brief Enable or disable Screen Space Reflections (SSR).
 *
 * @param enabled Set to true to enable SSR, false to disable it.
 *
 * Default: false
 */
R3DAPI void R3D_SetSSR(bool enabled);

/**
 * @brief Check whether Screen Space Reflections (SSR) are enabled.
 *
 * @return true if SSR is enabled, false otherwise.
 */
R3DAPI bool R3D_GetSSR(void);

/**
 * @brief Set the maximum number of ray-marching steps for SSR.
 *
 * @param maxRaySteps The maximum number of steps taken while marching
 *        along the reflection ray. Higher values improve accuracy but
 *        increase GPU cost.
 *
 * Default: 64
 */
R3DAPI void R3D_SetSSRMaxRaySteps(int maxRaySteps);

/**
 * @brief Get the maximum number of ray-marching steps for SSR.
 *
 * @return The maximum ray-marching steps.
 */
R3DAPI int R3D_GetSSRMaxRaySteps(void);

/**
 * @brief Set the number of refinement steps for the binary search phase.
 *
 * @param binarySearchSteps The number of iterations used to refine
 *        the ray-surface intersection point after a hit is detected.
 *        More steps yield a more precise intersection.
 *
 * Default: 8
 */
R3DAPI void R3D_SetSSRBinarySearchSteps(int binarySearchSteps);

/**
 * @brief Get the number of refinement steps for the binary search phase.
 *
 * @return The number of binary search steps.
 */
R3DAPI int R3D_GetSSRBinarySearchSteps(void);

/**
 * @brief Set the maximum ray marching distance in view space units.
 *
 * @param rayMarchLength The maximum distance a reflection ray can travel.
 *        Larger values allow longer reflections but may cause artifacts.
 *
 * Default: 8.0
 */
R3DAPI void R3D_SetSSRRayMarchLength(float rayMarchLength);

/**
 * @brief Get the maximum ray marching distance.
 *
 * @return The maximum ray marching distance.
 */
R3DAPI float R3D_GetSSRRayMarchLength(void);

/**
 * @brief Set the SSR depth thickness tolerance.
 *
 * @param depthThickness The maximum depth difference allowed between
 *        the ray position and the scene depth to consider a valid hit.
 *        Larger values increase tolerance but can cause ghosting.
 *
 * Default: 0.2
 */
R3DAPI void R3D_SetSSRDepthThickness(float depthThickness);

/**
 * @brief Get the SSR depth thickness tolerance.
 *
 * @return The depth thickness value.
 */
R3DAPI float R3D_GetSSRDepthThickness(void);

/**
 * @brief Set the SSR depth tolerance.
 *
 * @param depthTolerance The negative margin allowed when comparing the
 *        ray position against the scene depth. This prevents false negatives
 *        due to floating-point errors or slight inconsistencies in depth
 *        reconstruction.
 *
 * In practice, a hit is accepted if:
 *    -depthTolerance <= depthDiff < depthThickness
 *
 * Smaller values increase strictness but may cause missed intersections,
 * while larger values reduce artifacts but can introduce ghosting.
 *
 * Default: 0.005
 */
R3DAPI void R3D_SetSSRDepthTolerance(float depthTolerance);

/**
 * @brief Get the SSR depth tolerance.
 *
 * @return The depth tolerance value.
 */
R3DAPI float R3D_GetSSRDepthTolerance(void);

/**
 * @brief Set the fade range near the screen edges to reduce artifacts.
 *
 * @param start Normalized distance from the screen center where edge fading begins (0.0–1.0).
 * @param end   Normalized distance where fading is complete (0.0–1.0).
 *
 * Pixels outside this range will have their reflections gradually
 * faded out to avoid hard cutoffs near the borders.
 *
 * Default: start = 0.7, end = 1.0
 */
R3DAPI void R3D_SetSSRScreenEdgeFade(float start, float end);

/**
 * @brief Get the screen edge fade range.
 *
 * @param start Pointer to receive the fade start value.
 * @param end   Pointer to receive the fade end value.
 */
R3DAPI void R3D_GetSSRScreenEdgeFade(float* start, float* end);

// --------------------------------------------
// ENVIRONMENT: Fog Config Functions
// --------------------------------------------

/**
 * @brief Sets the fog mode.
 *
 * This function defines the type of fog effect applied to the scene.
 * Different modes may provide linear, exponential, or volumetric fog effects.
 *
 * @param mode The fog mode to set.
 *
 * Default: R3D_FOG_DISABLED
 */
R3DAPI void R3D_SetFogMode(R3D_Fog mode);

/**
 * @brief Gets the current fog mode.
 *
 * This function retrieves the fog mode currently applied to the scene.
 *
 * @return The current fog mode.
 */
R3DAPI R3D_Fog R3D_GetFogMode(void);

/**
 * @brief Sets the color of the fog.
 *
 * This function defines the color of the fog effect applied to the scene.
 * The fog color blends with objects as they are affected by fog.
 *
 * @param color The color to set for the fog.
 *
 * Default: WHITE
 */
R3DAPI void R3D_SetFogColor(Color color);

/**
 * @brief Gets the current fog color.
 *
 * This function retrieves the color currently used for the fog effect.
 *
 * @return The current fog color.
 */
R3DAPI Color R3D_GetFogColor(void);

/**
 * @brief Sets the start distance of the fog.
 *
 * This function defines the distance from the camera at which fog begins to appear.
 * Objects closer than this distance will not be affected by fog.
 *
 * @param value The start distance for the fog effect.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetFogStart(float value);

/**
 * @brief Gets the current fog start distance.
 *
 * This function retrieves the distance at which the fog begins to be applied.
 *
 * @return The current fog start distance.
 */
R3DAPI float R3D_GetFogStart(void);

/**
 * @brief Sets the end distance of the fog.
 *
 * This function defines the distance from the camera at which fog reaches full intensity.
 * Objects beyond this distance will be completely covered by fog.
 *
 * @param value The end distance for the fog effect.
 *
 * Default: 50.0
 */
R3DAPI void R3D_SetFogEnd(float value);

/**
 * @brief Gets the current fog end distance.
 *
 * This function retrieves the distance at which the fog is fully applied.
 *
 * @return The current fog end distance.
 */
R3DAPI float R3D_GetFogEnd(void);

/**
 * @brief Sets the density of the fog.
 *
 * This function controls how thick the fog appears. Higher values result in
 * denser fog, making objects fade out more quickly.
 *
 * @param value The density of the fog (higher values increase fog thickness).
 *
 * Default: 0.05
 */
R3DAPI void R3D_SetFogDensity(float value);

/**
 * @brief Gets the current fog density.
 *
 * This function retrieves the current density of the fog.
 *
 * @return The current fog density.
 */
R3DAPI float R3D_GetFogDensity(void);

/**
 * @brief Sets how much the fog affects the sky.
 *
 * This function controls the influence of fog on the sky color and visibility. 
 * A higher value makes the fog blend more strongly with the sky, reducing its clarity.
 *
 * @param value The fog effect on the sky, in the range [0.0f, 1.0f] 
 *              (0 = no effect, 1 = maximum blending).
 *
 * Default: 0.5
 */
R3DAPI void R3D_SetFogSkyAffect(float value);

/**
 * @brief Gets the current fog effect on the sky.
 *
 * This function retrieves the current influence of fog on the sky.
 *
 * @return The current fog-sky affect value, in the range [0.0f, 1.0f].
 */
R3DAPI float R3D_GetFogSkyAffect(void);

// --------------------------------------------
// ENVIRONMENT: Depth of Field (DoF) Functions
// --------------------------------------------

/**
 * @brief Enables or disables the depth of field post-process.
 *
 * @param mode The depth of field mode to set.
 *
 * Default: R3D_DOF_DISABLED
 */
R3DAPI void R3D_SetDofMode(R3D_Dof mode);

/**
 * @brief Gets the current depth of field mode.
 *
 * @return The current depth of field mode.
 */
R3DAPI R3D_Dof R3D_GetDofMode(void);

/**
 * @brief Sets the focus point in world space.
 *
 * This function defines the distance (in meters) from the camera where
 * objects will be in perfect focus. Objects closer or farther will be blurred.
 *
 * @param value The focus point distance in meters.
 *
 * Default: 10.0
 */
R3DAPI void R3D_SetDofFocusPoint(float value);

/**
 * @brief Gets the current focus point.
 *
 * @return The focus point distance in meters.
 */
R3DAPI float R3D_GetDofFocusPoint(void);

/**
 * @brief Sets the focus scale.
 *
 * This function controls how shallow the depth of field effect is.
 * Lower values create a shallower depth of field with more blur,
 * while higher values create a deeper depth of field with less blur.
 *
 * @param value The focus scale value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetDofFocusScale(float value);

/**
 * @brief Gets the current focus scale.
 *
 * @return The current focus scale value.
 */
R3DAPI float R3D_GetDofFocusScale(void);

/**
 * @brief Sets the maximum blur size.
 *
 * This function controls the maximum amount of blur applied to out-of-focus
 * areas. This value is similar to the lens aperture size, larger values
 * create more pronounced blur effects.
 *
 * @param value The maximum blur size value.
 *
 * Default: 20.0
 */
R3DAPI void R3D_SetDofMaxBlurSize(float value);

/**
 * @brief Gets the current maximum blur size.
 *
 * @return The current maximum blur size value.
 */
R3DAPI float R3D_GetDofMaxBlurSize(void);

/**
 * @brief Enables or disables depth-of-field debug mode.
 *
 * In debug mode, the scene uses color coding:
 * - Green: near blur
 * - Black: sharp areas
 * - Blue: far blur
 *
 * @param enabled true to enable, false to disable.
 *
 * Default: false
 */
R3DAPI void R3D_SetDofDebugMode(bool enabled);

/**
 * @brief Gets the current debug mode state.
 *
 * @return True if debug mode is enabled, false otherwise.
 */
R3DAPI bool R3D_GetDofDebugMode(void);

// --------------------------------------------
// ENVIRONMENT: Tonemap Config Functions
// --------------------------------------------

/**
 * @brief Sets the tonemapping mode.
 *
 * This function defines the tonemapping algorithm applied to the final rendered image.
 * Different tonemap modes affect color balance, brightness compression, and overall
 * scene appearance.
 *
 * @param mode The tonemap mode to set.
 *
 * Default: R3D_TONEMAP_LINEAR
 */
R3DAPI void R3D_SetTonemapMode(R3D_Tonemap mode);

/**
 * @brief Gets the current tonemapping mode.
 *
 * This function retrieves the tonemap mode currently applied to the scene.
 *
 * @return The current tonemap mode.
 */
R3DAPI R3D_Tonemap R3D_GetTonemapMode(void);

/**
 * @brief Sets the exposure level for tonemapping.
 *
 * This function adjusts the exposure level used in tonemapping, affecting
 * the overall brightness of the rendered scene.
 *
 * @param value The exposure value (higher values make the scene brighter).
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetTonemapExposure(float value);

/**
 * @brief Gets the current tonemap exposure level.
 *
 * This function retrieves the current exposure setting used in tonemapping.
 *
 * @return The current tonemap exposure value.
 */
R3DAPI float R3D_GetTonemapExposure(void);

/**
 * @brief Sets the white point for tonemapping.
 *
 * This function defines the reference white level, which determines how bright
 * areas of the scene are mapped to the final output.
 *
 * @param value The white point value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetTonemapWhite(float value);

/**
 * @brief Gets the current tonemap white point.
 *
 * This function retrieves the white point setting used in tonemapping.
 *
 * @return The current tonemap white value.
 */
R3DAPI float R3D_GetTonemapWhite(void);

// --------------------------------------------
// ENVIRONMENT: Color Adjustment Functions
// --------------------------------------------

/**
 * @brief Sets the global brightness adjustment.
 *
 * This function controls the brightness of the final rendered image.
 * Higher values make the image brighter, while lower values darken it.
 *
 * @param value The brightness adjustment value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetBrightness(float value);

/**
 * @brief Gets the current brightness level.
 *
 * This function retrieves the brightness setting applied to the scene.
 *
 * @return The current brightness value.
 */
R3DAPI float R3D_GetBrightness(void);

/**
 * @brief Sets the global contrast adjustment.
 *
 * This function controls the contrast of the final rendered image.
 * Higher values increase the difference between dark and bright areas.
 *
 * @param value The contrast adjustment value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetContrast(float value);

/**
 * @brief Gets the current contrast level.
 *
 * This function retrieves the contrast setting applied to the scene.
 *
 * @return The current contrast value.
 */
R3DAPI float R3D_GetContrast(void);

/**
 * @brief Sets the global saturation adjustment.
 *
 * This function controls the color intensity of the final rendered image.
 * Higher values make colors more vibrant, while lower values desaturate them.
 *
 * @param value The saturation adjustment value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetSaturation(float value);

/**
 * @brief Gets the current saturation level.
 *
 * This function retrieves the saturation setting applied to the scene.
 *
 * @return The current saturation value.
 */
R3DAPI float R3D_GetSaturation(void);

/** @} */ // end of Environment

/**
 * @defgroup Skybox Skybox Functions
 * @{
 */

// --------------------------------------------
// SKYBOX: Skybox Loading Functions
// --------------------------------------------

/**
 * @brief Loads a skybox from a texture file.
 *
 * This function loads a skybox cubemap from a texture file using a specified cubemap layout.
 * The layout defines how the six faces of the cubemap are arranged within the texture.
 *
 * @param filePath The path to the texture file.
 * @param layout The cubemap layout format.
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkybox(const char* filePath, CubemapLayout layout);

/**
 * @brief Loads a skybox from an image in memory.
 *
 * This function loads a skybox cubemap from an image already loaded in memory,
 * using a specified cubemap layout to map the six faces.
 *
 * @param image The source image in memory.
 * @param layout The cubemap layout format.
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkyboxFromMemory(Image image, CubemapLayout layout);

/**
 * @brief Loads a skybox from a panorama texture file.
 *
 * This function loads a skybox from a panorama (equirectangular) texture file,
 * and converts it into a cubemap with the specified resolution.
 *
 * @param filePath The path to the panorama texture file.
 * @param size The resolution of the generated cubemap (e.g., 512, 1024).
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkyboxPanorama(const char* filePath, int size);

/**
 * @brief Loads a skybox from a panorama image in memory.
 *
 * This function loads a skybox from a panorama (equirectangular) image already loaded in memory,
 * and converts it into a cubemap with the specified resolution.
 *
 * @param image The panorama image in memory.
 * @param size The resolution of the generated cubemap (e.g., 512, 1024).
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkyboxPanoramaFromMemory(Image image, int size);

/**
 * @brief Unloads a skybox and frees its resources.
 *
 * This function removes a previously loaded skybox from memory.
 * It should be called when the skybox is no longer needed to prevent memory leaks.
 *
 * @param sky The skybox to unload.
 */
R3DAPI void R3D_UnloadSkybox(R3D_Skybox sky);

/** @} */ // end of Skybox

/**
 * @defgroup Culling Culling Functions
 * @brief Defines manual culling functions, note that r3d has also an automatic culling system.
 * @{
 */

// --------------------------------------------
// CULLING: Frustum Test Functions
// --------------------------------------------

/**
 * @brief Checks if a point is inside the view frustum.
 *
 * Tests whether a 3D point lies within the camera's frustum by checking against all six planes.
 * Call this only between `R3D_Begin` and `R3D_End`.
 *
 * Useful when automatic frustum culling is disabled and you're using a custom spatial structure
 * (e.g., octree, BVH, etc.).
 *
 * @param position The 3D point to test.
 * @return `true` if inside the frustum, `false` otherwise.
 *
 * @note This performs an exact plane-point test. Slower than bounding box tests.
 * @warning Frustum culling may incorrectly discard objects casting visible shadows.
 * @todo Improve shadow-aware culling in future versions.
 *
 * @see R3D_IsPointInFrustumBoundingBox()
 */
R3DAPI bool R3D_IsPointInFrustum(Vector3 position);

/**
 * @brief Checks if a sphere is inside the view frustum.
 *
 * Tests whether a sphere intersects the camera's frustum using plane-sphere tests.
 * Call this only between `R3D_Begin` and `R3D_End`.
 *
 * Useful when managing visibility manually.
 *
 * @param position The center of the sphere.
 * @param radius The sphere's radius (must be positive).
 * @return `true` if at least partially inside the frustum, `false` otherwise.
 *
 * @note More accurate but slower than bounding box approximations.
 * @warning May cause visual issues with shadow casters being culled too early.
 * @todo Add support for shadow-aware visibility.
 *
 * @see R3D_IsSphereInFrustumBoundingBox()
 */
R3DAPI bool R3D_IsSphereInFrustum(Vector3 position, float radius);

/**
 * @brief Checks if an AABB is inside the view frustum.
 *
 * Determines whether an axis-aligned bounding box intersects the frustum.
 * Call between `R3D_Begin` and `R3D_End`.
 *
 * For use in custom culling strategies or spatial partitioning systems.
 *
 * @param aabb The bounding box to test.
 * @return `true` if at least partially inside the frustum, `false` otherwise.
 *
 * @note Exact but more costly than AABB pre-tests.
 * @warning May prematurely cull objects casting visible shadows.
 * @todo Add support for light-aware visibility tests.
 *
 * @see R3D_IsAABBInFrustumBoundingBox()
 */
R3DAPI bool R3D_IsAABBInFrustum(BoundingBox aabb);

/**
 * @brief Checks if an OBB is inside the view frustum.
 *
 * Tests an oriented bounding box (transformed AABB) for frustum intersection.
 * Must be called between `R3D_Begin` and `R3D_End`.
 *
 * Use this for objects with transformations when doing manual culling.
 *
 * @param aabb Local-space bounding box.
 * @param transform World-space transform matrix.
 * @return `true` if the transformed box intersects the frustum, `false` otherwise.
 *
 * @note More expensive than AABB checks due to matrix operations.
 * @warning May incorrectly cull shadow casters.
 * @todo Consider shadow-aware culling improvements.
 *
 * @see R3D_IsAABBInFrustum()
 */
R3DAPI bool R3D_IsOBBInFrustum(BoundingBox aabb, Matrix transform);

/**
 * @brief Fast pre-filtering test for point inside frustum bounding box.
 *
 * Performs an AABB check using the frustum's bounding volume.
 * Useful for quick rejection before precise tests.
 *
 * @param position The 3D point to test.
 * @return `true` if inside the frustum AABB, `false` otherwise.
 *
 * @note May return false positives, never false negatives.
 * @warning Only checks against a loose AABB, not actual frustum planes.
 * @see R3D_IsPointInFrustum()
 */
R3DAPI bool R3D_IsPointInFrustumBoundingBox(Vector3 position);

/**
 * @brief Fast pre-filtering test for sphere inside frustum bounding box.
 *
 * Performs a quick check using the frustum's AABB to approximate intersection.
 *
 * @param position The center of the sphere.
 * @param radius Radius of the sphere.
 * @return `true` if possibly intersecting the frustum AABB, `false` otherwise.
 *
 * @note Faster but less accurate than full frustum testing.
 * @warning May produce false positives.
 * @see R3D_IsSphereInFrustum()
 */
R3DAPI bool R3D_IsSphereInFrustumBoundingBox(Vector3 position, float radius);

/**
 * @brief Fast pre-filtering test for AABB inside frustum bounding box.
 *
 * Performs a bounding box vs bounding box intersection to quickly eliminate non-visible objects.
 * Useful as an initial coarse check before calling full frustum tests.
 *
 * @param aabb The bounding box to test.
 * @return `true` if intersecting the frustum AABB, `false` otherwise.
 *
 * @note False positives possible, but never false negatives.
 * @warning Does not use actual frustum planes.
 * @note No OBB variant exists due to computational cost.
 * @see R3D_IsAABBInFrustum()
 */
R3DAPI bool R3D_IsAABBInFrustumBoundingBox(BoundingBox aabb);

/** @} */ // end of Culling

/**
 * @defgroup Utils Utility Functions
 * @brief Defines some utility functions, including access to internal data, useful for adding additional effects.
 * @{
 */

// --------------------------------------------
// UTILS: Default Texture Retrieval Functions
// --------------------------------------------

/**
 * @brief Retrieves a default white texture.
 *
 * This texture is fully white (1,1,1,1), useful for default material properties.
 *
 * @return A white texture.
 */
R3DAPI Texture2D R3D_GetWhiteTexture(void);

/**
 * @brief Retrieves a default black texture.
 *
 * This texture is fully black (0,0,0,1), useful for masking or default values.
 *
 * @return A black texture.
 */
R3DAPI Texture2D R3D_GetBlackTexture(void);

/**
 * @brief Retrieves a default normal map texture.
 *
 * This texture represents a neutral normal map (0.5, 0.5, 1.0), which applies no normal variation.
 *
 * @return A neutral normal texture.
 */
R3DAPI Texture2D R3D_GetNormalTexture(void);

// --------------------------------------------
// UTILS: Render Texture Retrieval Functions
// --------------------------------------------

/**
 * @brief Retrieves the final scene color buffer.
 *
 * This texture stores the final rendered scene as a 24-bit RGB buffer.
 *
 * @return The final color buffer texture.
 */
R3DAPI Texture2D R3D_GetBufferColor(void);

/**
 * @brief Retrieves the buffer containing the scene's normal data.
 *
 * This texture stores octahedral-compressed normals using two 16-bit per-channel RG components.
 *
 * @note You can find the decoding functions in the embedded shaders, such as 'screen/lighting.fs.glsl'.
 *
 * @return The normal buffer texture.
 */
R3DAPI Texture2D R3D_GetBufferNormal(void);

/**
 * @brief Retrieves the final depth buffer.
 *
 * This texture contains the depth stored in 24 bits and a stencil buffer where each value is 0 or 1, indicating the presence of geometry.
 * It is useful for post-processing effects outside of R3D.
 *
 * @note If you modify the texture parameters to sample the stencil instead of the depth,
 * make sure to reset the parameters afterward.
 *
 * @return The final depth buffer texture.
 */
R3DAPI Texture2D R3D_GetBufferDepth(void);

// --------------------------------------------
// UTILS: Camera Matrices Retrieval Functions
// --------------------------------------------

/**
 * @brief Retrieves the view matrix.
 *
 * This matrix represents the camera's transformation from world space to view space.
 * It is updated at the last call to 'R3D_Begin'.
 *
 * @return The current view matrix.
 */
R3DAPI Matrix R3D_GetMatrixView(void);

/**
 * @brief Retrieves the inverse view matrix.
 *
 * This matrix transforms coordinates from view space back to world space.
 * It is updated at the last call to 'R3D_Begin'.
 *
 * @return The current inverse view matrix.
 */
R3DAPI Matrix R3D_GetMatrixInvView(void);

/**
 * @brief Retrieves the projection matrix.
 *
 * This matrix defines the transformation from view space to clip space.
 * It is updated at the last call to 'R3D_Begin'.
 *
 * @return The current projection matrix.
 */
R3DAPI Matrix R3D_GetMatrixProjection(void);

/**
 * @brief Retrieves the inverse projection matrix.
 *
 * This matrix transforms coordinates from clip space back to view space.
 * It is updated at the last call to 'R3D_Begin'.
 *
 * @return The current inverse projection matrix.
 */
R3DAPI Matrix R3D_GetMatrixInvProjection(void);

// --------------------------------------------
// UTILS: Debug Buffer Rendering Functions
// --------------------------------------------

/**
 * @brief Renders the internal albedo buffer to the screen.
 *
 * This function displays the albedo (diffuse) buffer as a 2D texture.
 * It must be called outside of `R3D_Begin` and `R3D_End`.
 *
 * @param x X position to draw the buffer.
 * @param y Y position to draw the buffer.
 * @param w Width of the drawn buffer.
 * @param h Height of the drawn buffer.
 */
R3DAPI void R3D_DrawBufferAlbedo(float x, float y, float w, float h);

/**
 * @brief Renders the internal emission buffer to the screen.
 *
 * Displays the emission buffer, which contains emissive lighting data.
 * Must be called outside of `R3D_Begin` and `R3D_End`.
 *
 * @param x X position to draw the buffer.
 * @param y Y position to draw the buffer.
 * @param w Width of the drawn buffer.
 * @param h Height of the drawn buffer.
 */
R3DAPI void R3D_DrawBufferEmission(float x, float y, float w, float h);

/**
 * @brief Renders the internal normal buffer to the screen.
 *
 * Displays the normal buffer, showing world-space normal data as colors.
 * Must be called outside of `R3D_Begin` and `R3D_End`.
 *
 * @param x X position to draw the buffer.
 * @param y Y position to draw the buffer.
 * @param w Width of the drawn buffer.
 * @param h Height of the drawn buffer.
 */
R3DAPI void R3D_DrawBufferNormal(float x, float y, float w, float h);

/**
 * @brief Renders the ORM (Occlusion, Roughness, Metalness) buffer to the screen.
 *
 * Displays the ORM buffer, where each channel stores different material properties:
 * - Red: Ambient occlusion
 * - Green: Roughness
 * - Blue: Metalness
 *
 * Must be called outside of `R3D_Begin` and `R3D_End`.
 *
 * @param x X position to draw the buffer.
 * @param y Y position to draw the buffer.
 * @param w Width of the drawn buffer.
 * @param h Height of the drawn buffer.
 */
R3DAPI void R3D_DrawBufferORM(float x, float y, float w, float h);

/**
 * @brief Renders the SSAO (Screen Space Ambient Occlusion) buffer to the screen.
 *
 * Displays the SSAO buffer, showing ambient occlusion data in grayscale.
 * Must be called outside of `R3D_Begin` and `R3D_End`.
 *
 * @param x X position to draw the buffer.
 * @param y Y position to draw the buffer.
 * @param w Width of the drawn buffer.
 * @param h Height of the drawn buffer.
 */
R3DAPI void R3D_DrawBufferSSAO(float x, float y, float w, float h);

/**
 * @brief Renders the bloom buffer to the screen.
 *
 * Displays the bloom effect buffer, showing the extracted bright areas after blur processing.
 * Must be called outside of `R3D_Begin` and `R3D_End`.
 *
 * @param x X position to draw the buffer.
 * @param y Y position to draw the buffer.
 * @param w Width of the drawn buffer.
 * @param h Height of the drawn buffer.
 */
R3DAPI void R3D_DrawBufferBloom(float x, float y, float w, float h);

/** @} */ // end of Utils

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // R3D_H
