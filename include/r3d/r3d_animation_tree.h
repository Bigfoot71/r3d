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

typedef void (*R3D_AnimationNodeCallback)(const R3D_Animation* animation, R3D_AnimationState state,
                                          int boneIdx, Transform* out, void* userData);

typedef void (*R3D_AnimationTreeCallback)(const R3D_AnimationPlayer* player, int boneIdx,
                                          Transform* out, void* userData);

// ========================================
// ENUM TYPES
// ========================================

typedef enum  {
    R3D_STM_EDGE_INSTANT = 0,
    R3D_STM_EDGE_ONDONE
} R3D_StmEdgeMode;

typedef enum {
    R3D_STM_EDGE_ON = 0,
    R3D_STM_EDGE_AUTO,
    R3D_STM_EDGE_ONCE,
    R3D_STM_EDGE_OFF
} R3D_StmEdgeStatus;

// ========================================
// STRUCTS TYPES
// ========================================

typedef struct {
    int32_t mask[8];
    int     boneCount;
} R3D_BoneMask;

typedef struct {
    char               name[32];
    R3D_AnimationState state;
    bool               looper;

    R3D_AnimationNodeCallback evalCallback;
    void*                     evalUserData;
} R3D_AnimationNodeParams;

typedef struct {
    R3D_BoneMask*  boneMask;
    float          blend;
} R3D_Blend2NodeParams;

typedef struct {
    R3D_BoneMask*  boneMask;
    float          weight;
} R3D_Add2NodeParams;

typedef struct {
    bool         synced;
    unsigned int activeInput;
    float        xFadeTime;
} R3D_SwitchNodeParams;

typedef struct {
    R3D_StmEdgeMode   mode;
    R3D_StmEdgeStatus currentStatus;
    R3D_StmEdgeStatus nextStatus;
    float             xFadeTime;
} R3D_StmEdgeParams;

typedef struct {
    R3D_AnimationPlayer    player;
    R3D_AnimationTreeNode* rootNode;
    R3D_AnimationTreeNode* nodePool;
    unsigned int           nodePoolSize;
    unsigned int           nodePoolMaxSize;
    int                    rootBone;

    R3D_AnimationTreeCallback updateCallback;
    void*                     updateUserData;
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
                                 unsigned int inputIdx);


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
                                                   R3D_AnimationStmIndex beginStateIdx,
                                                   R3D_AnimationStmIndex endStateIdx,
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
                                 R3D_AnimationStmIndex targetStateIdx);

R3DAPI R3D_BoneMask R3D_ComputeBoneMask(const R3D_Skeleton* skeleton,
                                        const char* boneNames[],
                                        unsigned int boneNameCount);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of AnimationTree

#endif // R3D_ANIMATION_TREE_H
// EOF
