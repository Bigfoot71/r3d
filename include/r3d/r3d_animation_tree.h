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

typedef union R3D_AnimationTreeNode R3D_AnimationTreeNode;

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
    int32_t mask[8];    ///< Bit mask buffer for maximum of 256 bones (32bits * 8).
    int boneCount;      ///< Actual bones count.
} R3D_BoneMask;

/**
 * @brief Parameters for animation tree node Animation.
 *
 * Animation is a leaf node, holding the R3D_Animation structure.
 */
typedef struct {
    char name[32];                          ///< Animation name (null-terminated string).
    R3D_AnimationState state;               ///< Animation state.
    bool looper;                            ///< Flag to control if the animation is considered done when looped; yes when true.

    R3D_AnimationNodeCallback evalCallback; ///< Callback function to receive and modify animation transformation before been used.
    void* evalUserData;                     ///< Optional user data pointer passed to the callback.
} R3D_AnimationNodeParams;

/**
 * @brief Parameters for animation tree node Blend2.
 *
 * Blend2 node blends channels of any two animation nodes together, with respecting optional bone mask.
 */
typedef struct {
    R3D_BoneMask* boneMask; ///< Pointer to bone mask structure, can be NULL; calculated by R3D_ComputeBoneMask().
    float blend;            ///< Blend weight value, can be in interval from 0.0 to 1.0.
} R3D_Blend2NodeParams;

/**
 * @brief Parameters for animation tree node Add2.
 *
 * Add2 node adds channels of any two animation nodes together, with respecting optional bone mask.
 */
typedef struct {
    R3D_BoneMask* boneMask; ///< Pointer to bone mask structure, can be NULL; calculated by R3D_ComputeBoneMask().
    float weight;           ///< Add weight value, can be in interval from 0.0 to 1.0.
} R3D_Add2NodeParams;

/**
 * @brief Parameters for animation tree node Switch.
 *
 * Switch node allows instant or blended/faded transition between any animation nodes connected to inputs.
 */
typedef struct {
    bool synced;                ///< Flag to control input animation nodes synchronization; activated input is reset when false.
    unsigned int activeInput;   ///< Active input zero-based index.
    float xFadeTime;            ///< Animation nodes cross fade blending time, in seconds.
} R3D_SwitchNodeParams;

/**
 * @brief Parameters for animation state machine edge.
 */
typedef struct {
    R3D_StmEdgeMode mode;           ///< Operation mode.
    R3D_StmEdgeStatus status;       ///< Current travel status.
    R3D_StmEdgeStatus nextStatus;   ///< Travel status used after machine traversed thru this edge with status set to R3D_STM_EDGE_ONCE.
    float xFadeTime;                ///< Cross fade blending time between connected animation nodes, in seconds.
} R3D_StmEdgeParams;

/**
 * @brief Manages a tree structure of animation nodes.
 *
 * Animation tree allows to define complex logic for switching and blending animations of
 * associated animation player. It supports 5 different animation node types: Animation, Blend2, Add2,
 * Switch and State Machine. It also fully supports root motion and bone masking in Blend2/Add2.
 */
typedef struct {
    R3D_AnimationPlayer player;             ///< Animation player and skeleton used by all animation nodes.
    R3D_AnimationTreeNode* rootNode;        ///< Pointer to root animation node of the tree.
    R3D_AnimationTreeNode* nodePool;        ///< Animation node pool of size nodePoolMaxSize.
    unsigned int nodePoolSize;              ///< Current animation node pool size.
    unsigned int nodePoolMaxSize;           ///< Maximum number of animation nodes, defined during load.
    int rootBone;                           ///< Optional root bone index, -1 if not defined.

    R3D_AnimationTreeCallback updateCallback;   ///< Callback function to receive and modify final animation transformation.
    void* updateUserData;                       ///< Optional user data pointer passed to the callback.
} R3D_AnimationTree;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates an animation tree using given animation player.
 *
 * Allocates memory for animation nodes pool.
 *
 * @param player Animation player used by the animation tree.
 * @param maxSize Size of the animation nodes pool; maximum number of animation nodes in the tree.
 * @return Newly created animation tree, or a zeroed struct on failure.
 */
R3DAPI R3D_AnimationTree R3D_LoadAnimationTree(R3D_AnimationPlayer player, int maxSize);

/**
 * @brief Creates an animation tree using given animation player, with optional root motion support.
 *
 * Allocates memory for animation nodes pool and sets root bone index for root motion.
 *
 * @param player Animation player used by the animation tree.
 * @param maxSize Size of the animation nodes pool; maximum number of animation nodes in the tree.
 * @param rootBone Root bone index; set -1 to disable root motion.
 * @return Newly created animation tree, or a zeroed struct on failure.
 */
R3DAPI R3D_AnimationTree R3D_LoadAnimationTreeEx(R3D_AnimationPlayer player, int maxSize, int rootBone);

/**
 * @brief Creates an animation tree using given animation player, with optional root motion support and callback.
 *
 * Allocates memory for animation nodes pool, sets root bone index and update callback.
 *
 * @param player Animation player used by the animation tree.
 * @param maxSize Size of the animation nodes pool; maximum number of animation nodes in the tree.
 * @param rootBone Root bone index; set -1 to disable root motion.
 * @param updateCallback Callback function pointer to receive and modify final animation transformation, can be NULL.
 * @param updateUserdata User data pointer passed to the callback, can be NULL.
 * @return Newly created animation tree, or a zeroed struct on failure.
 */
R3DAPI R3D_AnimationTree R3D_LoadAnimationTreePro(R3D_AnimationPlayer player, int maxSize, int rootBone,
                                                  R3D_AnimationTreeCallback updateCallback, void* updateUserData);

/**
 * @brief Releases all resources used by an animation tree, including all animation nodes.
 *
 * @param tree Animation tree to unload.
 */
R3DAPI void R3D_UnloadAnimationTree(R3D_AnimationTree tree);

/**
 * @brief Updates the animation tree: calculates blended pose, sets and uploads the pose thru associated animation player.
 *
 * @param tree Animation tree.
 * @param dt Delta time in seconds.
 */
R3DAPI void R3D_UpdateAnimationTree(R3D_AnimationTree* tree, float dt);

/**
 * @brief Updates the animation tree: calculates blended pose, sets and uploads the pose thru associated animation player.
 *
 * Set the rootMotion and/or rootDistance pointers to get root motion information.
 * Divide rootMotion translation vector with dt to get root bone speed.
 *
 * @param tree Animation tree.
 * @param dt Delta time in seconds.
 * @param rootMotion Pointer to root bone motion transformation.
 * @param rootDistance Pointer to transformation containing root bone distance from rest pose.
 */
R3DAPI void R3D_UpdateAnimationTreeEx(R3D_AnimationTree* tree, float dt,
                                      Transform* rootMotion, Transform* rootDistance);

/**
 * @brief Sets root animation node of the animation tree.
 *
 * @param tree Animation tree.
 * @param node Root animation node.
 */
R3DAPI void R3D_AddRootAnimationNode(R3D_AnimationTree* tree, R3D_AnimationTreeNode* node);

/**
 * @brief Connects two animation nodes of animation tree hierarchy.
 *
 * @param parent Parent animation node.
 * @param node Child animation node.
 * @param inputIndex Index of the parent node input used for connection.
 * @return True on success; false if parent node or inputIndex is invalid.
 */
R3DAPI bool R3D_AddAnimationNode(R3D_AnimationTreeNode* parent, R3D_AnimationTreeNode* node, unsigned int inputIndex);

/**
 * @brief Creates animation node of type Animation.
 *
 * Animation is a leaf node, holding the R3D_Animation structure.
 *
 * @param tree Animation tree.
 * @param params Animation node initial parameters.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded or animation name is not found.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateAnimationNode(R3D_AnimationTree* tree, R3D_AnimationNodeParams params);

/**
 * @brief Creates animation node of type Blend2.
 *
 * Blend2 node blends channels of any two animation nodes together, with respecting optional bone mask.
 *
 * @param tree Animation tree.
 * @param params Blend2 node initial parameters.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateBlend2Node(R3D_AnimationTree* tree, R3D_Blend2NodeParams params);

/**
 * @brief Creates animation node of type Add2.
 *
 * Add2 node adds channels of any two animation nodes together, with respecting optional bone mask.
 *
 * @param tree Animation tree.
 * @param params Add2 node initial parameters.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateAdd2Node(R3D_AnimationTree* tree, R3D_Add2NodeParams params);

/**
 * @brief Creates animation node of type Switch.
 *
 * Switch node allows instant or blended/faded transition between any animation nodes connected to inputs.
 *
 * @param tree Animation tree.
 * @param inputCount Number of available inputs.
 * @param params Switch node initial parameters.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateSwitchNode(R3D_AnimationTree* tree, unsigned int inputCount,
                                                   R3D_SwitchNodeParams params);

/**
 * @brief Creates animation node of type State Machine (Stm).
 *
 * Stm node allows to create a state machine graph of any animation nodes.
 *
 * @param tree Animation tree.
 * @param statesCount Maximum number of states in the state machine.
 * @param edgesCount Maximum number of edges in the state machine.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateStmNode(R3D_AnimationTree* tree,
                                                unsigned int statesCount,
                                                unsigned int edgesCount);

/**
 * @brief Creates animation node of type State Machine (Stm), with option to disable travel feature (enabled by default).
 *
 * @param tree Animation tree.
 * @param statesCount Maximum number of states in the state machine.
 * @param edgesCount Maximum number of edges in the state machine.
 * @param enableTravel Flag to enable or disable travel feature; enabled when set true.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateStmNodeEx(R3D_AnimationTree* tree,
                                                  unsigned int statesCount,
                                                  unsigned int edgesCount,
                                                  bool enableTravel);

/**
 * @brief Creates animation node of type State Machine Stop/Done (StmX).
 *
 * StmX is a helper animation node, that allows to stop state machine evaluation in current update.
 * Sets animation node done status of the state machine to true.
 *
 * @param tree Animation tree.
 * @param nestedNode Actual animation node with animation channels.
 * @return Pointer to created animation node; NULL if maximum number of nodes was exceeded.
 */
R3DAPI R3D_AnimationTreeNode* R3D_CreateStmXNode(R3D_AnimationTree* tree,
                                                 R3D_AnimationTreeNode* nestedNode);

/**
 * @brief Creates state in a State Machine animation node.
 *
 * @param stmNode Animation node of type State Machine.
 * @param stateNode Associated animation node of the state.
 * @param outEdgesCount Number of output edges of the state.
 * @return Index of created state; -1 if maximum number of states was exceeded.
 */
R3DAPI R3D_AnimationStmIndex R3D_CreateStmNodeState(R3D_AnimationTreeNode* stmNode,
                                                    R3D_AnimationTreeNode* stateNode,
                                                    unsigned int outEdgesCount);

/**
 * @brief Creates edge in a State Machine animation node.
 *
 * @param stmNode Animation node of type State Machine.
 * @param beginStateIndex Index of state connected to the edge beginning.
 * @param endStateIndex Index of state connected to the edge end.
 * @param params Edge initial parameters.
 * @return Index of created edge; -1 if maximum number of edges was exceeded.
 */
R3DAPI R3D_AnimationStmIndex R3D_CreateStmNodeEdge(R3D_AnimationTreeNode* stmNode,
                                                   R3D_AnimationStmIndex beginStateIndex,
                                                   R3D_AnimationStmIndex endStateIndex,
                                                   R3D_StmEdgeParams params);

/**
 * @brief Sets parameters of animation node Animation.
 *
 * @param node Animation node of type Animation.
 * @param params New parameters.
 */
R3DAPI void R3D_SetAnimationNodeParams(R3D_AnimationTreeNode* node,
                                       R3D_AnimationNodeParams params);

/**
 * @brief Gets parameters of animation node Animation.
 *
 * @param node Animation node of type Animation.
 * @return Current parameters.
 */
R3DAPI R3D_AnimationNodeParams R3D_GetAnimationNodeParams(R3D_AnimationTreeNode* node);

/**
 * @brief Sets parameters of animation node Blend2.
 *
 * @param node Animation node of type Blend2.
 * @param params New parameters.
 */
R3DAPI void R3D_SetBlend2NodeParams(R3D_AnimationTreeNode* node,
                                    R3D_Blend2NodeParams params);

/**
 * @brief Gets parameters of animation node Blend2.
 *
 * @param node Animation node of type Blend2.
 * @return Current parameters.
 */
R3DAPI R3D_Blend2NodeParams R3D_GetBlend2NodeParams(R3D_AnimationTreeNode* node);

/**
 * @brief Sets parameters of animation node Add2.
 *
 * @param node Animation node of type Add2.
 * @param params New parameters.
 */
R3DAPI void R3D_SetAdd2NodeParams(R3D_AnimationTreeNode* node,
                                  R3D_Add2NodeParams params);

/**
 * @brief Gets parameters of animation node Add2.
 *
 * @param node Animation node of type Add2.
 * @return Current parameters.
 */
R3DAPI R3D_Add2NodeParams R3D_GetAdd2NodeParams(R3D_AnimationTreeNode* node);

/**
 * @brief Sets parameters of animation node Switch.
 *
 * @param node Animation node of type Switch.
 * @param params New parameters.
 */
R3DAPI void R3D_SetSwitchNodeParams(R3D_AnimationTreeNode* node,
                                    R3D_SwitchNodeParams params);

/**
 * @brief Gets parameters of animation node Switch.
 *
 * @param node Animation node of type Switch.
 * @return Current parameters.
 */
R3DAPI R3D_SwitchNodeParams R3D_GetSwitchNodeParams(R3D_AnimationTreeNode* node);

/**
 * @brief Sets parameters of State Machine edge.
 *
 * @param node Animation node of type State Machine (Stm).
 * @param edgeIndex Index of the State Machine edge.
 * @param params New parameters of the edge.
 */
R3DAPI void R3D_SetStmNodeEdgeParams(R3D_AnimationTreeNode* node,
                                     R3D_AnimationStmIndex edgeIndex,
                                     R3D_StmEdgeParams params);

/**
 * @brief Gets parameters of State Machine edge.
 *
 * @param node Animation node of type State Machine.
 * @param edgeIndex Index of the State Machine edge.
 * @return Current parameters of the edge.
 */
R3DAPI R3D_StmEdgeParams R3D_GetStmNodeEdgeParams(R3D_AnimationTreeNode* node,
                                                  R3D_AnimationStmIndex edgeIndex);

/**
 * @brief Sets travel path inside State Machine, from current state to target.
 *
 * If travel path is not found, target is set as current state instantly (teleport).
 *
 * @param node Animation node of type State Machine.
 * @param targetStateIndex Index of the targeted state.
 */
R3DAPI void R3D_TravelToStmState(R3D_AnimationTreeNode* node,
                                 R3D_AnimationStmIndex targetStateIndex);

/**
 * @brief Computes bone mask from list of bone names.
 *
 * Only listed bones will be included in evaluation of animation node with this bone mask.
 * Can be used in Blend2 and Add2 animation nodes.
 *
 * @param skeleton Skeleton of associated animation player.
 * @param boneNames Array of pointers to null-terminated strings with bone names.
 * @param boneNamesCount Count of strings in boneNames array.
 * @return Calculated bone mask, or zeroed structure on failure.
 */
R3DAPI R3D_BoneMask R3D_ComputeBoneMask(const R3D_Skeleton* skeleton,
                                        const char* boneNames[],
                                        unsigned int boneNameCount);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of AnimationTree

#endif // R3D_ANIMATION_TREE_H
