/* r3d_animation_tree.h -- R3D Animation Tree Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_ANIMATION_TREE_H
#define R3D_ANIMATION_TREE_H

#include <r3d/r3d_animation_player.h>

/**
 * @defgroup AnimationTree
 * @{
 */

// ========================================
// FORWARD DECLARATIONS
// ========================================

typedef union r3d_animtree_node R3D_AnimationTreeNode;

typedef int R3D_AnimationStmIndex;

// ========================================
// CALLBACKS TYPES
// ========================================

/**
 * @brief Callback type for manipulating with animation before been used by the animation tree.
 *
 * @param animation Pointer to the processed animation.
 * @param state Current animation state.
 * @param boneIndex Index of the processed bone.
 * @param out Transformation of the processed bone.
 * @param userData Optional user-defined data passed when the callback was registered.
 */
typedef void (*R3D_AnimationNodeCallback)(const R3D_Animation* animation, R3D_AnimationState state,
                                          int boneIndex, Transform* out, void* userData);

/**
 * @brief Callback type for manipulating with final animation.
 *
 * @param player Pointer to the animation player used by the animation tree.
 * @param boneIndex Index of the processed bone.
 * @param out Transformation of the processed bone.
 * @param userData Optional user-defined data passed when the callback was registered.
 */
typedef void (*R3D_AnimationTreeCallback)(const R3D_AnimationPlayer* player, int boneIndex,
                                          Transform* out, void* userData);

// ========================================
// ENUM TYPES
// ========================================

/**
 * @brief Types of operation modes for state machine edge.
 */
typedef enum  {
    R3D_STM_EDGE_INSTANT = 0, ///< Switch to next state instantly, with respecting cross fade time.
    R3D_STM_EDGE_ONDONE       ///< Switch to next state when associated animation is done or looped with looper parameter set true.
} R3D_StmEdgeMode;

/**
 * @brief Types of travel status for state machine edge.
 */
typedef enum {
    R3D_STM_EDGE_ON = 0, ///< Edge is traversable by travel function.
    R3D_STM_EDGE_AUTO,   ///< Edge is traversable automatically and by travel function.
    R3D_STM_EDGE_ONCE,   ///< Edge is traversable automatically and by travel function, buy only once; edge status changes to nextStatus when traversed.
    R3D_STM_EDGE_OFF     ///< Edge is not traversable.
} R3D_StmEdgeStatus;

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Bone mask for Blend2 and Add2 animation nodes.
 *
 * Bone mask structure, can by created by R3D_ComputeBoneMask.
 */
typedef struct {
    int32_t mask[8];   ///< Bit mask buffer for maximum of 256 bones (32bits * 8).
    int     boneCount; ///< Actual bones count.
} R3D_BoneMask;

/**
 * @brief Parameters for animation tree node Animation.
 *
 * Animation is a leaf node, holding the R3D_Animation structure.
 */
typedef struct {
    char               name[32]; ///< Animation name (null-terminated string).
    R3D_AnimationState state;    ///< Animation state.
    bool               looper;   ///< Flag to control if the animation is considered done when looped; yes when true.

    R3D_AnimationNodeCallback evalCallback; ///< Callback function to receive and modify animation transformation before been used.
    void*                     evalUserData; ///< Optional user data pointer passed to the callback.
} R3D_AnimationNodeParams;

/**
 * @brief Parameters for animation tree node Blend2.
 *
 * Blend2 node blends channels of any two animation nodes together, with respecting optional bone mask.
 */
typedef struct {
    R3D_BoneMask*  boneMask; ///< Pointer to bone mask structure, can be NULL.
    float          blend;    ///< Blend weight value, can be in interval from 0.0 to 1.0.
} R3D_Blend2NodeParams;

/**
 * @brief Parameters for animation tree node Add2.
 *
 * Add2 node adds channels of any two animation nodes together, with respecting optional bone mask.
 */
typedef struct {
    R3D_BoneMask*  boneMask; ///< Pointer to bone mask structure, can be NULL.
    float          weight;   ///< Add weight value, can be in interval from 0.0 to 1.0.
} R3D_Add2NodeParams;

/**
 * @brief Parameters for animation tree node Switch.
 *
 * Switch node allows instant or blended/faded transition between any animation nodes defined as inputs.
 */
typedef struct {
    bool         synced;      ///< Flag to control input animation nodes synchronization; activated input is reset when false.
    unsigned int activeInput; ///< Active input zero-based index.
    float        xFadeTime;   ///< Animation nodes cross fade blending time, in seconds.
} R3D_SwitchNodeParams;

/**
 * @brief Parameters for animation state machine edge.
 */
typedef struct {
    R3D_StmEdgeMode   mode;          ///< Operation mode.
    R3D_StmEdgeStatus currentStatus; ///< Current travel status.
    R3D_StmEdgeStatus nextStatus;    ///< Travel status used after machine traversed thru this edge with currentStatus set to R3D_STM_EDGE_ONCE.
    float             xFadeTime;     ///< Cross fade blending time between connected animation nodes, in seconds.
} R3D_StmEdgeParams;

/**
 * @brief Manages tree structure of animation and state machine nodes.
 *
 * The animation tree allows to define complex logic for switching and blending animations of
 * associated animation player. It supports 5 different animation nodes: Animation, Blend2, Add2,
 * Switch and State Machine. It also fully supports root motion and bone masking in Blend2/Add2.
 */
typedef struct {
    R3D_AnimationPlayer    player;          ///< Animation player and skeleton used by all animation nodes.
    R3D_AnimationTreeNode* rootNode;        ///< Pointer to root animation node of the tree.
    R3D_AnimationTreeNode* nodePool;        ///< Animation node pool of size nodePoolMaxSize.
    unsigned int           nodePoolSize;    ///< Current animation node pool size.
    unsigned int           nodePoolMaxSize; ///< Maximum number of animation nodes, defined during load.
    int                    rootBone;        ///< Optional root bone index, -1 if not defined.

    R3D_AnimationTreeCallback updateCallback; ///< Callback function to receive and modify final animation transformation.
    void*                     updateUserData; ///< Optional user data pointer passed to the callback.
} R3D_AnimationTree;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

R3DAPI R3D_AnimationTree R3D_LoadAnimationTree(R3D_AnimationPlayer player,
                                               int maxSize);

R3DAPI R3D_AnimationTree R3D_LoadAnimationTreeEx(R3D_AnimationPlayer player,
                                                 int maxSize, int rootBone);

R3DAPI R3D_AnimationTree R3D_LoadAnimationTreePro(R3D_AnimationPlayer player,
                                                  int maxSize, int rootBone,
                                                  R3D_AnimationTreeCallback updateCallback,
                                                  void* updateUserData);

R3DAPI void R3D_UnloadAnimationTree(R3D_AnimationTree tree);



R3DAPI void R3D_UpdateAnimationTree(R3D_AnimationTree* tree, float dt);

R3DAPI void R3D_UpdateAnimationTreeEx(R3D_AnimationTree* tree, float dt,
                                      Transform* rootMotion, Transform* rootDistance);


R3DAPI void R3D_AddRootAnimationNode(R3D_AnimationTree* tree,
                                     R3D_AnimationTreeNode* node);

R3DAPI bool R3D_AddAnimationNode(R3D_AnimationTreeNode* parent,
                                 R3D_AnimationTreeNode* node,
                                 unsigned int inputIndex);


R3DAPI R3D_AnimationTreeNode* R3D_CreateAnimationNode(R3D_AnimationTree* tree,
                                                      R3D_AnimationNodeParams params);

R3DAPI R3D_AnimationTreeNode* R3D_CreateBlend2Node(R3D_AnimationTree* tree,
                                                   R3D_Blend2NodeParams params);

R3DAPI R3D_AnimationTreeNode* R3D_CreateAdd2Node(R3D_AnimationTree* tree,
                                                 R3D_Add2NodeParams params);

R3DAPI R3D_AnimationTreeNode* R3D_CreateSwitchNode(R3D_AnimationTree* tree,
                                                   unsigned int inputCount,
                                                   R3D_SwitchNodeParams params);

R3DAPI R3D_AnimationTreeNode* R3D_CreateStmNode(R3D_AnimationTree* tree,
                                                unsigned int statesCount,
                                                unsigned int edgesCount);

R3DAPI R3D_AnimationTreeNode* R3D_CreateStmNodeEx(R3D_AnimationTree* tree,
                                                  unsigned int statesCount,
                                                  unsigned int edgesCount,
                                                  bool enableTravel);

R3DAPI R3D_AnimationTreeNode* R3D_CreateStmXNode(R3D_AnimationTree* tree,
                                                 R3D_AnimationTreeNode* nestedNode);

R3DAPI R3D_AnimationStmIndex R3D_CreateStmNodeState(R3D_AnimationTreeNode* stmNode,
                                                    R3D_AnimationTreeNode* stateNode,
                                                    unsigned int outEdgesCount);

R3DAPI R3D_AnimationStmIndex R3D_CreateStmNodeEdge(R3D_AnimationTreeNode* stmNode,
                                                   R3D_AnimationStmIndex beginStateIndex,
                                                   R3D_AnimationStmIndex endStateIndex,
                                                   R3D_StmEdgeParams params);


R3DAPI void R3D_SetAnimationNodeParams(R3D_AnimationTreeNode* node,
                                       R3D_AnimationNodeParams params);

R3DAPI R3D_AnimationNodeParams R3D_GetAnimationNodeParams(R3D_AnimationTreeNode* node);

R3DAPI void R3D_SetBlend2NodeParams(R3D_AnimationTreeNode* node,
                                    R3D_Blend2NodeParams params);

R3DAPI R3D_Blend2NodeParams R3D_GetBlend2NodeParams(R3D_AnimationTreeNode* node);

R3DAPI void R3D_SetAdd2NodeParams(R3D_AnimationTreeNode* node,
                                  R3D_Add2NodeParams params);

R3DAPI R3D_Add2NodeParams R3D_GetAdd2NodeParams(R3D_AnimationTreeNode* node);

R3DAPI void R3D_SetSwitchNodeParams(R3D_AnimationTreeNode* node,
                                    R3D_SwitchNodeParams params);

R3DAPI R3D_SwitchNodeParams R3D_GetSwitchNodeParams(R3D_AnimationTreeNode* node);

R3DAPI void R3D_SetStmNodeEdgeParams(R3D_AnimationTreeNode* node,
                                     R3D_AnimationStmIndex edgeIndex,
                                     R3D_StmEdgeParams params);

R3DAPI R3D_StmEdgeParams R3D_GetStmNodeEdgeParams(R3D_AnimationTreeNode* node,
                                                  R3D_AnimationStmIndex edgeIndex);

R3DAPI void R3D_TravelToStmState(R3D_AnimationTreeNode* node,
                                 R3D_AnimationStmIndex targetStateIndex);

R3DAPI R3D_BoneMask R3D_ComputeBoneMask(const R3D_Skeleton* skeleton,
                                        const char* boneNames[],
                                        unsigned int boneNameCount);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of AnimationTree

#endif // R3D_ANIMATION_TREE_H
// EOF
