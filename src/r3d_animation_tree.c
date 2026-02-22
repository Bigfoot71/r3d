/* r3d_animation_tree.c -- R3D Animation Tree Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <stdlib.h>
#include <string.h>

#include "./modules/r3d_animtree.h"

// ========================================
// PUBLIC API
// ========================================

R3D_AnimationTree R3D_LoadAnimationTree(R3D_AnimationPlayer player, int maxSize) {
    return R3D_LoadAnimationTreePro(player, maxSize, -1, NULL, NULL);
}

R3D_AnimationTree R3D_LoadAnimationTreeEx(R3D_AnimationPlayer player, int maxSize, int rootBone) {
    return R3D_LoadAnimationTreePro(player, maxSize, rootBone, NULL, NULL);
}

R3D_AnimationTree R3D_LoadAnimationTreePro(R3D_AnimationPlayer player, int maxSize, int rootBone,
                                           R3D_AnimationTreeCallback updateCallback,
                                           void* updateUserData) {
    R3D_AnimationTree tree = {0};
    tree.player          = player;
    tree.nodePool        = RL_MALLOC(maxSize * sizeof(*tree.nodePool));
    tree.nodePoolMaxSize = maxSize;
    tree.rootBone        = rootBone;
    tree.updateCallback  = updateCallback;
    tree.updateUserData  = updateUserData;
    return tree;
}

void R3D_UnloadAnimationTree(R3D_AnimationTree tree) {
    int poolSize = tree.nodePoolSize;
    for(int i = 0; i < poolSize; i++) {
        R3D_AnimationTreeNode node = tree.nodePool[i];
        r3d_animtree_delete(node);
        RL_FREE(node.base);
    }
    RL_FREE(tree.nodePool);
}

void R3D_UpdateAnimationTree(R3D_AnimationTree* tree, float dt) {
    R3D_UpdateAnimationTreeEx(tree, dt, NULL, NULL);
}

void R3D_UpdateAnimationTreeEx(R3D_AnimationTree* tree, float dt,
                               Transform* rootMotion, Transform* rootDistance) {
    r3d_animtree_update(tree, dt, rootMotion, rootDistance);
}

void R3D_AddRootAnimationNode(R3D_AnimationTree* tree, R3D_AnimationTreeNode* node) {
    tree->rootNode = node;
}

bool R3D_AddAnimationNode(R3D_AnimationTreeNode* parent, R3D_AnimationTreeNode* node,
                          unsigned int inputIdx) {
    switch(parent->base->type) {
    case R3D_ANIMTREE_ANIM:
    case R3D_ANIMTREE_STM:
    case R3D_ANIMTREE_STM_X:  return false;
    case R3D_ANIMTREE_BLEND2: return r3d_animtree_blend2_add(parent->bln2, *node, inputIdx);
    case R3D_ANIMTREE_ADD2:   return r3d_animtree_add2_add(parent->add2, *node, inputIdx);
    case R3D_ANIMTREE_SWITCH: return r3d_animtree_switch_add(parent->swch, *node, inputIdx);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to add animation node: invalid parent type %d",
                     parent->base->type);
    }
    return false;
}

R3D_AnimationTreeNode* R3D_CreateAnimationNode(R3D_AnimationTree* tree,
                                               R3D_AnimationNodeParams params) {
    return r3d_animtree_anim_create(tree, params);
}

R3D_AnimationTreeNode* R3D_CreateBlend2Node(R3D_AnimationTree* tree,
                                            R3D_Blend2NodeParams params) {
    return r3d_animtree_blend2_create(tree, params);
}

R3D_AnimationTreeNode* R3D_CreateAdd2Node(R3D_AnimationTree* tree,
                                          R3D_Add2NodeParams params) {
    return r3d_animtree_add2_create(tree, params);
}

R3D_AnimationTreeNode* R3D_CreateSwitchNode(R3D_AnimationTree* tree, unsigned int inputCount,
                                            R3D_SwitchNodeParams params) {
    return r3d_animtree_switch_create(tree, inputCount, params);
}

R3D_AnimationTreeNode* R3D_CreateStmNode(R3D_AnimationTree* tree, unsigned int statesCount,
                                         unsigned int edgesCount) {
    return r3d_animtree_stm_create(tree, statesCount, edgesCount, false);
}

R3D_AnimationTreeNode* R3D_CreateStmNodeEx(R3D_AnimationTree* tree, unsigned int statesCount,
                                           unsigned int edgesCount, bool enableTravel) {
    return r3d_animtree_stm_create(tree, statesCount, edgesCount, enableTravel);
}

R3D_AnimationTreeNode* R3D_CreateStmXNode(R3D_AnimationTree* tree,
                                          R3D_AnimationTreeNode* nestedNode) {
    return r3d_animtree_stm_x_create(tree, *nestedNode);
}

R3D_AnimationStmIndex R3D_CreateStmNodeState(R3D_AnimationTreeNode* stmNode,
                                             R3D_AnimationTreeNode* stateNode,
                                             unsigned int outEdgesCount) {
    return r3d_amintree_state_create(stmNode->stm, *stateNode, outEdgesCount);
}

R3D_AnimationStmIndex R3D_CreateStmNodeEdge(R3D_AnimationTreeNode* stmNode,
                                            R3D_AnimationStmIndex beginStateIdx,
                                            R3D_AnimationStmIndex endStateIdx,
                                            R3D_StmEdgeParams params) {
    return r3d_amintree_edge_create(stmNode->stm, beginStateIdx, endStateIdx, params);
}

void R3D_SetAnimationNodeParams(R3D_AnimationTreeNode* node, R3D_AnimationNodeParams params) {
    node->anim->params = params;
}

R3D_AnimationNodeParams R3D_GetAnimationNodeParams(R3D_AnimationTreeNode* node) {
    return node->anim->params;
}

void R3D_SetBlend2NodeParams(R3D_AnimationTreeNode* node, R3D_Blend2NodeParams params) {
    node->bln2->params = params;
}

R3D_Blend2NodeParams R3D_GetBlend2NodeParams(R3D_AnimationTreeNode* node) {
    return node->bln2->params;
}

void R3D_SetAdd2NodeParams(R3D_AnimationTreeNode* node, R3D_Add2NodeParams params) {
    node->add2->params = params;
}

R3D_Add2NodeParams R3D_GetAdd2NodeParams(R3D_AnimationTreeNode* node) {
    return node->add2->params;
}

void R3D_SetSwitchNodeParams(R3D_AnimationTreeNode* node, R3D_SwitchNodeParams params) {
    node->swch->params = params;
}

R3D_SwitchNodeParams R3D_GetSwitchNodeParams(R3D_AnimationTreeNode* node) {
    return node->swch->params;
}

void R3D_SetStmNodeEdgeParams(R3D_AnimationTreeNode* node, R3D_AnimationStmIndex edgeIndex,
                              R3D_StmEdgeParams params) {
    node->stm->edge_list[edgeIndex].params = params;
}

R3D_StmEdgeParams R3D_GetStmNodeEdgeParams(R3D_AnimationTreeNode* node,
                                           R3D_AnimationStmIndex edgeIndex) {
    return node->stm->edge_list[edgeIndex].params;
}

void R3D_TravelToStmState(R3D_AnimationTreeNode* node, R3D_AnimationStmIndex targetStateIdx) {
    r3d_animtree_travel(node->stm, targetStateIdx);
}

R3D_BoneMask R3D_ComputeBoneMask(const R3D_Skeleton* skeleton, const char* boneNames[],
                                 unsigned int boneNameCount) {
    const R3D_BoneInfo* bones  = skeleton->bones;
    const int           bCount = skeleton->boneCount;
    R3D_BoneMask        bMask  = {.boneCount = bCount};
    if(bCount > sizeof(bMask.mask) * 8) {
        R3D_TRACELOG(LOG_WARNING, "Failed to compute bone mask: max bone count exceeded (%d)",
                     sizeof(bMask.mask) * 8);
        return (R3D_BoneMask){0};
    }

    int maskBits = sizeof(bMask.mask[0]) * 8;
    for(int i = 0; i < boneNameCount; i++) {
        const char* name = boneNames[i];

        bool found = false;
        for(int bIdx = 0; bIdx < bCount; bIdx++)
            if(!strncmp(name, bones[bIdx].name, sizeof(bones[bIdx].name))) {
                int part = bIdx / maskBits;
                int bit  = bIdx % maskBits;
                bMask.mask[part] |= 1 << bit;

                found = true;
                break;
            }
        if(!found) {
            R3D_TRACELOG(LOG_WARNING, "Failed to compute bone mask: bone \"%s\" not found",
                         name);
            return (R3D_BoneMask){0};
        }
    }
    return bMask;
}

// EOF
