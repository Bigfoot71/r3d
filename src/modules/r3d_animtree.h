/* r3d_animtree.h -- Internal R3D animation tree module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_ANIMTREE_H
#define R3D_MODULE_ANIMTREE_H

#include <r3d_config.h>
#include <r3d/r3d_animation_tree.h>

// ========================================
// TREE NODE TYPES
// ========================================

enum r3d_animtree_type_e {
    R3D_ANIMTREE_ANIM = 1,
    R3D_ANIMTREE_BLEND2,
    R3D_ANIMTREE_ADD2,
    R3D_ANIMTREE_SWITCH,
    R3D_ANIMTREE_STM,
    R3D_ANIMTREE_STM_X
};

typedef unsigned int               r3d_animtree_type_t;
typedef struct r3d_animtree_base   r3d_animtree_base_t;
typedef struct r3d_animtree_anim   r3d_animtree_anim_t;
typedef struct r3d_animtree_blend2 r3d_animtree_blend2_t;
typedef struct r3d_animtree_add2   r3d_animtree_add2_t;
typedef struct r3d_animtree_switch r3d_animtree_switch_t;
typedef struct r3d_animtree_stm    r3d_animtree_stm_t;
typedef struct r3d_animtree_stm_x  r3d_animtree_stm_x_t;

union r3d_animtree_node {
    r3d_animtree_base_t*   base;
    r3d_animtree_anim_t*   anim;
    r3d_animtree_blend2_t* bln2;
    r3d_animtree_add2_t*   add2;
    r3d_animtree_switch_t* swch;
    r3d_animtree_stm_t*    stm;
    r3d_animtree_stm_x_t*  stmx;
};

// ========================================
// STATE MACHINE STRUCTURES
// ========================================

typedef struct {
    R3D_AnimationStmIndex beg_idx;
    R3D_AnimationStmIndex end_idx;
    float                 end_weight;
    R3D_StmEdgeParams     params;
} r3d_stmedge_t;

typedef struct {
    unsigned int    out_cnt;
    unsigned int    max_out;
    r3d_stmedge_t** out_list;
    r3d_stmedge_t*  active_in;
} r3d_stmstate_t;

typedef struct {
    bool  yes;
    float when;
} r3d_stmvisit_t;

// ========================================
// TREE NODE STRUCTURES
// ========================================

struct r3d_animtree_base {
    r3d_animtree_type_t type;
};

struct r3d_animtree_anim {
    r3d_animtree_type_t     type;
    const R3D_Animation*    animation;
    R3D_AnimationNodeParams params;
    struct {
        Transform last;
        Transform rest_0;
        Transform rest_n;
        int       loops;
    } root;
};

struct r3d_animtree_blend2 {
    r3d_animtree_type_t   type;
    R3D_AnimationTreeNode in_main;
    R3D_AnimationTreeNode in_blend;
    R3D_Blend2NodeParams  params;
};

struct r3d_animtree_add2 {
    r3d_animtree_type_t   type;
    R3D_AnimationTreeNode in_main;
    R3D_AnimationTreeNode in_add;
    R3D_Add2NodeParams    params;
};

struct r3d_animtree_switch {
    r3d_animtree_type_t    type;
    R3D_AnimationTreeNode* in_list;
    float*                 in_weights;
    unsigned int           in_cnt;
    unsigned int           prev_in;
    float                  weights_isum;
    R3D_SwitchNodeParams   params;
};

struct r3d_animtree_stm {
    r3d_animtree_type_t    type;
    unsigned int           states_cnt;
    unsigned int           edges_cnt;
    unsigned int           max_states;
    unsigned int           max_edges;
    R3D_AnimationStmIndex  active_idx;
    R3D_AnimationTreeNode* node_list;
    r3d_stmedge_t*         edge_list;
    r3d_stmstate_t*        state_list;
    r3d_stmvisit_t*        visit_list;
    struct {
        r3d_stmedge_t** edges;
        unsigned int    idx;
        unsigned int    len;
        r3d_stmedge_t** open;
        r3d_stmedge_t** next;
        bool*           mark;
    } path;
};

struct r3d_animtree_stm_x {
    r3d_animtree_type_t   type;
    R3D_AnimationTreeNode nested;
};

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_animtree_blend2_add(r3d_animtree_blend2_t* parent, R3D_AnimationTreeNode anode,
                             unsigned int in_idx);

bool r3d_animtree_add2_add(r3d_animtree_add2_t* parent, R3D_AnimationTreeNode anode,
                           unsigned int in_idx);

bool r3d_animtree_switch_add(r3d_animtree_switch_t* parent, R3D_AnimationTreeNode anode,
                             unsigned int in_idx);

R3D_AnimationTreeNode* r3d_animtree_anim_create(R3D_AnimationTree* atree,
                                                R3D_AnimationNodeParams params);

R3D_AnimationTreeNode* r3d_animtree_blend2_create(R3D_AnimationTree* atree,
                                                  R3D_Blend2NodeParams params);

R3D_AnimationTreeNode* r3d_animtree_add2_create(R3D_AnimationTree* atree,
                                                R3D_Add2NodeParams params);

R3D_AnimationTreeNode* r3d_animtree_switch_create(R3D_AnimationTree* atree,
                                                  unsigned int in_cnt,
                                                  R3D_SwitchNodeParams params);

R3D_AnimationTreeNode* r3d_animtree_stm_create(R3D_AnimationTree* atree,
                                               unsigned int states_cnt,
                                               unsigned int edges_cnt,
                                               bool travel);

R3D_AnimationTreeNode* r3d_animtree_stm_x_create(R3D_AnimationTree* atree,
                                                 R3D_AnimationTreeNode nested);

R3D_AnimationStmIndex r3d_amintree_state_create(r3d_animtree_stm_t* node,
                                                R3D_AnimationTreeNode anode,
                                                unsigned int edges_cnt);

R3D_AnimationStmIndex r3d_amintree_edge_create(r3d_animtree_stm_t* node,
                                               R3D_AnimationStmIndex beg_idx,
                                               R3D_AnimationStmIndex end_idx,
                                               R3D_StmEdgeParams params);

void r3d_animtree_delete(R3D_AnimationTreeNode anode);

void r3d_animtree_update(R3D_AnimationTree* atree, float elapsed_time,
                         Transform* root_motion, Transform* root_distance);

void r3d_animtree_travel(r3d_animtree_stm_t* node, R3D_AnimationStmIndex target_idx);

#endif // R3D_MODULE_ANIMTREE_H
// EOF
