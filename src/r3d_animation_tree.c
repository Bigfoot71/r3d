/* r3d_animation_tree.c -- R3D Animation Tree Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation_tree.h>
#include <r3d_config.h>
#include <stdlib.h>
#include <string.h>

#include "./common/r3d_anim.h"

// ========================================
// TREE NODE TYPES
// ========================================

enum r3d_animtree_type {
    R3D_ANIMTREE_ANIM = 1,
    R3D_ANIMTREE_BLEND2,
    R3D_ANIMTREE_ADD2,
    R3D_ANIMTREE_SWITCH,
    R3D_ANIMTREE_STM,
    R3D_ANIMTREE_STM_X
};

typedef enum   r3d_animtree_type   r3d_animtree_type_t;
typedef struct r3d_animtree_base   r3d_animtree_base_t;
typedef struct r3d_animtree_anim   r3d_animtree_anim_t;
typedef struct r3d_animtree_blend2 r3d_animtree_blend2_t;
typedef struct r3d_animtree_add2   r3d_animtree_add2_t;
typedef struct r3d_animtree_switch r3d_animtree_switch_t;
typedef struct r3d_animtree_stm    r3d_animtree_stm_t;
typedef struct r3d_animtree_stm_x  r3d_animtree_stm_x_t;

union R3D_AnimationTreeNode {
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
    R3D_AnimationStmIndex beginIdx;
    R3D_AnimationStmIndex endIdx;
    float endWeight;
    R3D_StmEdgeParams params;
} r3d_stmedge_t;

typedef struct {
    unsigned int outCount;
    unsigned int maxOut;
    r3d_stmedge_t** outList;
    r3d_stmedge_t* activeIn;
} r3d_stmstate_t;

typedef struct {
    bool yes;
    float when;
} r3d_stmvisit_t;

// ========================================
// TREE NODE STRUCTURES
// ========================================

struct r3d_animtree_base {
    r3d_animtree_type_t type;
};

struct r3d_animtree_anim {
    r3d_animtree_type_t type;
    const R3D_Animation* animation;
    R3D_AnimationNodeParams params;
    struct {
        Transform last;
        Transform rest0;
        Transform restN;
        int loops;
    } root;
};

struct r3d_animtree_blend2 {
    r3d_animtree_type_t type;
    R3D_AnimationTreeNode inMain;
    R3D_AnimationTreeNode inBlend;
    R3D_Blend2NodeParams params;
};

struct r3d_animtree_add2 {
    r3d_animtree_type_t type;
    R3D_AnimationTreeNode inMain;
    R3D_AnimationTreeNode inAdd;
    R3D_Add2NodeParams params;
};

struct r3d_animtree_switch {
    r3d_animtree_type_t type;
    R3D_AnimationTreeNode* inList;
    float* inWeights;
    unsigned int inCount;
    unsigned int prevIn;
    float weightsInvSum;
    R3D_SwitchNodeParams params;
};

struct r3d_animtree_stm {
    r3d_animtree_type_t type;
    unsigned int statesCount;
    unsigned int edgesCount;
    unsigned int maxStates;
    unsigned int maxEdges;
    R3D_AnimationStmIndex activeIdx;
    R3D_AnimationTreeNode* nodeList;
    r3d_stmedge_t* edgeList;
    r3d_stmstate_t* stateList;
    r3d_stmvisit_t* visitList;
    struct {
        r3d_stmedge_t** edges;
        unsigned int idx;
        unsigned int len;
        r3d_stmedge_t** open;
        r3d_stmedge_t** next;
        bool* mark;
    } path;
};

struct r3d_animtree_stm_x {
    r3d_animtree_type_t type;
    R3D_AnimationTreeNode nested;
};

// ========================================
// TREE UPDATE/EVAL SUPPORT INFO STRUCTURES
// ========================================

typedef struct {
    bool anodeDone;
    float xFade;
    float consumedTime;
} upinfo_t;

typedef struct {
    Transform motion;
    Transform distance;
} rminfo_t;

// ========================================
// TREE NODE COMPUTE FUNCTION PROTOTYPES
// ========================================

static bool anode_update(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                         float elapsedTime, upinfo_t* info);

static bool anode_eval(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                       int boneIdx, Transform* out, rminfo_t* info);

static void anode_reset(R3D_AnimationTreeNode anode);

// ========================================
// BONE FUNCTIONS
// ========================================

static bool is_root_bone(const R3D_AnimationTree* atree, int boneIdx)
{
    return atree->rootBone == boneIdx;
}

static bool valid_root_bone(int boneIdx)
{
    return boneIdx >= 0;
}

static bool masked_bone(const R3D_BoneMask* bmask, int boneIdx)
{
    int maskb = (sizeof(bmask->mask[0]) * 8);
    int part = boneIdx / maskb;
    int bit = boneIdx % maskb;
    return (bmask->mask[part] & (1 << bit)) != 0;
}

// ========================================
// ANIMATION STATE MACHINE
// ========================================

static r3d_stmedge_t* stm_find_edge(const r3d_animtree_stm_t* node, const r3d_stmstate_t* state)
{
    unsigned int pathLen = node->path.len;
    unsigned int pathIdx = node->path.idx;

    if (pathIdx < pathLen) {
        return node->path.edges[pathIdx];
    }
    else {
        unsigned int edgesCount = state->outCount;
        for (int edgeIdx = 0; edgeIdx < edgesCount; edgeIdx++) {
            r3d_stmedge_t* edge = state->outList[edgeIdx];
            R3D_StmEdgeStatus status = edge->params.status;
            bool open = (status == R3D_STM_EDGE_AUTO || status == R3D_STM_EDGE_ONCE);
            if (open) return edge;
        }
    }

    return NULL;
}

static bool stm_update_edge(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                            r3d_stmedge_t* edge, float elapsedTime, float* consumedTime,
                            bool* done)
{
    float xFade = edge->params.xFadeTime;
    bool  doXFade = (xFade > elapsedTime);

    if (doXFade) {
        float wInc = Remap(elapsedTime, 0.0f, xFade, 0.0f, 1.0f);
        edge->endWeight += wInc;

        float wClamp = CLAMP(edge->endWeight, 0.0f, 1.0f);
        float wDelta = edge->endWeight - wClamp;
        edge->endWeight = wClamp;
        *consumedTime = (wInc > 0.0f ? elapsedTime * (1.0f - wDelta/wInc) : elapsedTime);
        *done = FloatEquals(edge->endWeight, 1.0f);
    }
    else {
        edge->endWeight = 1.0f;
        *consumedTime = xFade;
        *done = true;
    }

    if (*done) return true;

    return anode_update(atree, node->nodeList[edge->beginIdx], elapsedTime, NULL);
}

static bool stm_next_state(r3d_animtree_stm_t* node, r3d_stmedge_t* edge,
                           bool edgeDone, bool nodeDone, R3D_AnimationStmIndex* nextIdx)
{
    R3D_StmEdgeMode mode = edge->params.mode;

    bool ready = ((mode == R3D_STM_EDGE_INSTANT && edgeDone) ||
                  (mode == R3D_STM_EDGE_ONDONE  && nodeDone));

    if (!ready) return false;

    R3D_AnimationStmIndex endIdx = edge->endIdx;
    r3d_stmstate_t* state = &node->stateList[endIdx];
    R3D_AnimationTreeNode anode = node->nodeList[endIdx];

    state->activeIn = edge;
    anode_reset(anode);

    edge->endWeight = 0.0f;
    if (edge->params.status == R3D_STM_EDGE_ONCE) {
        edge->params.status = edge->params.nextStatus;
    }

    *nextIdx = endIdx;

    return true;
}

static bool stm_update_state(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                             float elapsedTime, float* consumedTime,
                             R3D_AnimationStmIndex* activeIdx, bool* doNext, bool* stmDone)
{
    R3D_AnimationTreeNode* nodeList = node->nodeList;
    r3d_stmstate_t* state = &node->stateList[*activeIdx];

    bool edgeDone = true;
    float edgeTime = 0.0f;
    r3d_stmedge_t* activeEdge = state->activeIn;

    if (activeEdge) {
        if (!stm_update_edge(atree, node, activeEdge, elapsedTime, &edgeTime, &edgeDone)) {
            return false;
        }
    }

    if (edgeDone) state->activeIn = NULL;

    r3d_stmedge_t* nextEdge = stm_find_edge(node, state);
    upinfo_t nodeInfo = {.xFade = (nextEdge ? nextEdge->params.xFadeTime : 0.0f)};
    if (!anode_update(atree, nodeList[*activeIdx], elapsedTime, &nodeInfo)) {
        return false;
    }

    bool nodeDone = edgeDone && nodeInfo.anodeDone;
    bool isNext = (nextEdge ? stm_next_state(node, nextEdge, edgeDone, nodeDone, activeIdx) : false);

    *stmDone = (edgeDone && (nodeList[*activeIdx].base->type == R3D_ANIMTREE_STM_X));
    *doNext = nodeDone && isNext && !*stmDone;
    *consumedTime = MAX(edgeTime, nodeInfo.consumedTime);

    return true;
}

static int expand_path(r3d_animtree_stm_t* node, r3d_stmedge_t** edges, r3d_stmedge_t** next,
                       int nextCount, unsigned int pathLen, const r3d_stmstate_t* state)
{
    unsigned int maxOpenPaths = node->maxEdges;
    unsigned int maxPathLen = node->maxStates;
    unsigned int outCount = state->outCount;

    int added = 0;
    for (int edgeIdx = 0; edgeIdx < outCount; edgeIdx++) {
        r3d_stmedge_t* edge = state->outList[edgeIdx];

        bool closed = edge->params.status == R3D_STM_EDGE_OFF;
        if (closed) continue;

        if (nextCount < maxOpenPaths) {
            memcpy(&next[nextCount*maxPathLen], edges, pathLen * sizeof(r3d_stmedge_t*));
            next[nextCount*maxPathLen + pathLen] = edge;
            nextCount += 1;
            added += 1;
        }
        else {
            R3D_TRACELOG(LOG_WARNING, "Failed to find path: max open paths count exceeded (%d)", maxOpenPaths);
            break;
        }
    }
    return added;
}

static bool stm_find_path(r3d_animtree_stm_t* node, R3D_AnimationStmIndex targetIdx)
{
    unsigned int maxPathLen = node->maxStates;
    r3d_stmedge_t** openPaths = node->path.open;
    r3d_stmedge_t** nextPaths = node->path.next;
    bool* marked = node->path.mark;

    memset(marked, false, node->statesCount * sizeof(*node->path.mark));
    marked[node->activeIdx] = true;

    int pathsCount = expand_path(
        node, openPaths, openPaths, 0, 0,
        &node->stateList[node->activeIdx]
    );

    unsigned int pathLen = 1;
    while(pathsCount > 0) {
        int nextCount = 0;
        for (int pIdx = 0; pIdx < pathsCount; pIdx++) {
            R3D_AnimationStmIndex stateIdx = openPaths[pIdx*maxPathLen + pathLen-1]->endIdx;
            if (stateIdx == targetIdx) {
                memcpy(node->path.edges, &openPaths[pIdx*maxPathLen], pathLen * sizeof(r3d_stmedge_t*));
                node->path.idx = 0;
                node->path.len = pathLen;
                return true;
            }
            if (marked[stateIdx]) continue;
            else marked[stateIdx] = true;

            nextCount += expand_path(
                node, &openPaths[pIdx*maxPathLen], nextPaths,
                nextCount, pathLen, &node->stateList[stateIdx]
            );
        }

        if (pathLen < maxPathLen) {
            pathLen += 1;
        }
        else {
            R3D_TRACELOG(LOG_WARNING, "Failed to find path: max path lenght exceeded (%d)", maxPathLen);
            break;
        }

        memcpy(openPaths, nextPaths, maxPathLen * nextCount * sizeof(r3d_stmedge_t*));
        pathsCount = nextCount;
    }

    return false;
}

// ========================================
// TREE NODE CREATE FUNCTION
// ========================================

static R3D_AnimationTreeNode* anode_create(R3D_AnimationTree* atree, r3d_animtree_type_t type, size_t node_size)
{
    R3D_AnimationTreeNode* node;

    unsigned int poolSize = atree->nodePoolSize;
    if (poolSize < atree->nodePoolMaxSize) {
        node = &atree->nodePool[poolSize];
        node->base = RL_CALLOC(1, node_size);
        node->base->type = type;
    }
    else {
        return NULL;
    }

    atree->nodePoolSize += 1;
    return node;
}

// ========================================
// TREE NODE RESET FUNCTIONS
// ========================================

static void anode_reset_anim(r3d_animtree_anim_t* node)
{
    const R3D_Animation* a = node->animation;
    R3D_AnimationState*  s = &node->params.state;

    float duration = a->duration / a->ticksPerSecond;
    s->currentTime = (s->speed >= 0.0f) ? 0.0f : duration;
}

static void anode_reset_blend2(r3d_animtree_blend2_t* node)
{
    anode_reset(node->inMain);
    anode_reset(node->inBlend);
}

static void anode_reset_add2(r3d_animtree_add2_t* node)
{
    anode_reset(node->inMain);
    anode_reset(node->inAdd);
}

static void anode_reset_switch(r3d_animtree_switch_t* node)
{
    unsigned char inCount  = node->inCount;
    unsigned char activeIn = node->params.activeInput;

    for (int i = 0; i < inCount; i++) {
        node->inWeights[i] = 0.0f;
    }
    node->inWeights[activeIn] = 1.0f;

    for (int i = 0; i < inCount; i++) {
        anode_reset(node->inList[i]);
    }
}

static void anode_reset_stm(r3d_animtree_stm_t* node)
{
    node->activeIdx = 0;
    node->path.idx = 0;
    node->path.len = 0;

    r3d_stmedge_t* edge = node->stateList[0].activeIn;
    if (edge) edge->endWeight = 0.0f;

    anode_reset(node->nodeList[0]);
}

// other stm reset version: reset only active node+state
/*
static void anode_reset_stm(r3d_animtree_stm_t* node)
{
    unsigned int   activeIdx = node->activeIdx;
    r3d_stmedge_t* edge    = node->stateList[activeIdx].activeIn;
    if (edge) edge->endWeight = 0.0f;

    anode_reset(node->nodeList[activeIdx]);
}
*/

static void anode_reset_stm_x(r3d_animtree_stm_x_t* node)
{
    anode_reset(node->nested);
}

static void anode_reset(R3D_AnimationTreeNode anode)
{
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:   return anode_reset_anim(anode.anim);
    case R3D_ANIMTREE_BLEND2: return anode_reset_blend2(anode.bln2);
    case R3D_ANIMTREE_ADD2:   return anode_reset_add2(anode.add2);
    case R3D_ANIMTREE_SWITCH: return anode_reset_switch(anode.swch);
    case R3D_ANIMTREE_STM:    return anode_reset_stm(anode.stm);
    case R3D_ANIMTREE_STM_X:  return anode_reset_stm_x(anode.stmx);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to reset node: invalid type %d", anode.base->type);
        break;
    }
}

// ========================================
// TREE NODE UPDATE FUNCTIONS
// ========================================

static bool anode_update_anim(const R3D_AnimationTree* atree, r3d_animtree_anim_t* node, float elapsedTime, upinfo_t* info)
{
    const R3D_Animation* a = node->animation;

    R3D_AnimationState* s = &node->params.state;
    if (!s->play) {
        if (info) {
            *info = (upinfo_t) {
                .anodeDone = true,
                .xFade = info->xFade,
                .consumedTime = 0.0f
            };
        }
        return true;
    }

    float speed = s->speed;
    float duration = a->duration / a->ticksPerSecond;
    float tInc = speed * elapsedTime;
    float tCur = s->currentTime + tInc;

    s->currentTime = tCur;

    bool cross = ((speed < 0.0f && tCur <= 0.0f) || (speed > 0.0f && tCur >= duration));

    if (cross) {
        if ((s->play = s->loop)) {
            s->currentTime -= copysignf(duration, speed);
        }
        else {
            float tClamp = CLAMP(s->currentTime, 0.0f, duration);
            float tDelta = s->currentTime - tClamp;
            elapsedTime = (!FloatEquals(tInc, 0.0f) ? elapsedTime * (1.0f - tDelta/tInc) : 0.0f);
            s->currentTime = tClamp;
        }
        node->root.loops = (int)(elapsedTime / duration);
    }
    else {
        node->root.loops = -1;
    }

    if (info) {
        float xFade = info->xFade;
        float durXFade = CLAMP(duration - xFade, 0.0f, duration);
        bool  crossXFade = ((speed < 0.0f && tCur <= xFade) || (speed > 0.0f && tCur >= durXFade));
        *info = (upinfo_t) {
            .anodeDone = crossXFade ? node->params.looper : false,
            .xFade = xFade,
            .consumedTime = !FloatEquals(speed, 0.0f) ? elapsedTime : 0.0f
        };
    }

    return true;
}

static bool anode_update_blend2(const R3D_AnimationTree* atree, r3d_animtree_blend2_t* node, float elapsedTime, upinfo_t* info)
{
    return (
        anode_update(atree, node->inMain, elapsedTime, info) &&
        anode_update(atree, node->inBlend, elapsedTime, NULL)
    );
}

static bool anode_update_add2(const R3D_AnimationTree* atree, r3d_animtree_add2_t* node, float elapsedTime, upinfo_t* info)
{
    return (
        anode_update(atree, node->inMain, elapsedTime, info) &&
        anode_update(atree, node->inAdd, elapsedTime, NULL)
    );
}

static bool anode_update_switch(const R3D_AnimationTree* atree, r3d_animtree_switch_t* node, float elapsedTime, upinfo_t* info)
{
    unsigned int inCount = node->inCount;
    unsigned int activeIn = node->params.activeInput;
    if (activeIn >= inCount) {
        R3D_TRACELOG(LOG_WARNING, "Failed to update switch: active input %d out of range (%d)", activeIn, inCount);
        return false;
    }

    bool reset = (activeIn == node->prevIn) ? false : !node->params.synced;
    if (reset) anode_reset(node->inList[activeIn]);

    for (int i = 0; i < inCount; i++) {
        if (!anode_update(atree, node->inList[i], elapsedTime, NULL)) {
            return false;
        }
    }
    node->prevIn = activeIn;

    float xFade = node->params.xFadeTime;
    bool noXFade = (xFade <= elapsedTime);

    if (noXFade) {
        for (int i = 0; i < inCount; i++) {
            node->inWeights[i] = 0.0f;
        }
        node->inWeights[activeIn] = 1.0f;
    }
    else {
        float w_fade = Remap(elapsedTime, 0.0f, xFade, 0.0f, 1.0f);
        for (int i = 0; i < inCount; i++) {
            float w_sign = (i == activeIn) ? 1.0f : -1.0f;
            node->inWeights[i] = CLAMP(node->inWeights[i] + w_sign*w_fade, 0.0f, 1.0f);
        }
    }

    float wSum = 0.0f;
    for (int i = 0; i < inCount; i++) {
        wSum += node->inWeights[i];
    }
    node->weightsInvSum = 1.0f / wSum;

    return true;
}

static bool anode_update_stm(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node, float elapsedTime, upinfo_t* info)
{
    R3D_AnimationStmIndex activeIdx = node->activeIdx;

    r3d_stmvisit_t* visited = node->visitList;
    memset(visited, 0, node->statesCount * sizeof(*node->visitList));

    float startTime = elapsedTime;
    bool doNext = true;
    bool stmDone = false;

    while(doNext)
    {
        if (visited[activeIdx].yes && FloatEquals(visited[activeIdx].when, elapsedTime)) {
            R3D_TRACELOG(LOG_WARNING, "Failed to update stm: cycle detected, aborted");
            return false;
        }
        visited[activeIdx] = (r3d_stmvisit_t){.yes = true, .when = elapsedTime};

        float consumedTime;
        if (!stm_update_state(atree, node, elapsedTime, &consumedTime, &activeIdx, &doNext, &stmDone)) {
            return false;
        }
        elapsedTime -= consumedTime;

        if (doNext) {
            unsigned int pathLen = node->path.len;
            unsigned int pathIdx = node->path.idx;
            if (pathIdx < pathLen) node->path.idx += 1;
        }

        if (FloatEquals(elapsedTime, 0.0f)) {
            break;
        }

        if (elapsedTime < 0.0f) {
            R3D_TRACELOG(LOG_WARNING, "Failed to update stm: incorrect time calculation (%f)", elapsedTime);
            return false;
        }
    }

    node->activeIdx = activeIdx;

    if (info) {
        *info = (upinfo_t) {
            .anodeDone = stmDone,
            .consumedTime = startTime - elapsedTime
        };
    }

    return true;
}

static bool anode_update_stm_x(const R3D_AnimationTree* atree, r3d_animtree_stm_x_t* node, float elapsedTime, upinfo_t* info)
{
    return anode_update(atree, node->nested, elapsedTime, info);
}

static bool anode_update(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode, float elapsedTime, upinfo_t* info)
{
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:   return anode_update_anim(atree, anode.anim, elapsedTime, info);
    case R3D_ANIMTREE_BLEND2: return anode_update_blend2(atree, anode.bln2, elapsedTime, info);
    case R3D_ANIMTREE_ADD2:   return anode_update_add2(atree, anode.add2, elapsedTime, info);
    case R3D_ANIMTREE_SWITCH: return anode_update_switch(atree, anode.swch, elapsedTime, info);
    case R3D_ANIMTREE_STM:    return anode_update_stm(atree, anode.stm, elapsedTime, info);
    case R3D_ANIMTREE_STM_X:  return anode_update_stm_x(atree, anode.stmx, elapsedTime, info);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to update animation tree: invalid node type %d", anode.base->type);
        break;
    }
    return false;
}

// ========================================
// TREE NODE EVAL FUNCTIONS
// ========================================

static bool anode_eval_anim(const R3D_AnimationTree* atree, r3d_animtree_anim_t* node,
                            int boneIdx, Transform* out, rminfo_t* info)
{
    const R3D_Animation* anim = node->animation;
    const R3D_AnimationState state = node->params.state;
    const R3D_AnimationChannel* channel = r3d_anim_channel_find(anim, boneIdx);

    *out = !channel ? (Transform){0}
        : r3d_anim_channel_lerp(channel, state.currentTime * anim->ticksPerSecond, NULL, NULL);

    if (node->params.evalCallback) {
        node->params.evalCallback(anim, state, boneIdx, out, node->params.evalUserData);
    }

    if (!is_root_bone(atree, boneIdx)) {
        return true;
    }

    if (info) {
        Transform motion = {0};
        const int loops = node->root.loops;

        if (loops > 0) {
            motion = r3d_anim_transform_scale(
                r3d_anim_transform_subtr(node->root.restN, node->root.rest0),
                (float)loops
            );
        }

        if (loops >= 0) {
            const bool forward = state.speed > 0.0f;
            const Transform rest0 = forward ? node->root.rest0 : node->root.restN;
            const Transform restN = forward ? node->root.restN : node->root.rest0;
            const Transform split = r3d_anim_transform_add(
                r3d_anim_transform_subtr(restN, node->root.last),
                r3d_anim_transform_subtr(*out, rest0)
            );
            motion = r3d_anim_transform_add(motion, split);
            motion.rotation = QuaternionNormalize(motion.rotation);
            info->motion = motion;
        }
        else {
            info->motion = r3d_anim_transform_subtr(*out, node->root.last);
        }

        info->distance = r3d_anim_transform_subtr(*out, node->root.rest0);
    }

    node->root.last = *out;

    return true;
}

static bool anode_eval_blend2(const R3D_AnimationTree* atree, r3d_animtree_blend2_t* node,
                              int boneIdx, Transform* out, rminfo_t* info)
{
    const R3D_BoneMask* bmask = node->params.boneMask;
    const bool doBlend = !bmask || masked_bone(bmask, boneIdx);
    const bool isRm = info && is_root_bone(atree, boneIdx);

    rminfo_t rm[2] = {0};
    Transform in[2] = {0};

    bool success = anode_eval(atree, node->inMain, boneIdx, &in[0], isRm ? &rm[0] : NULL);
    if (!success) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval blend2 node");
        return false;
    }

    if (doBlend) {
        success = anode_eval(atree, node->inBlend, boneIdx, &in[1], isRm ? &rm[1] : NULL);
        if (!success) {
            R3D_TRACELOG(LOG_WARNING, "Failed to eval blend2 node");
            return false;
        }
    }

    const float w = CLAMP(node->params.blend, 0.0f, 1.0f);
    *out = doBlend ? r3d_anim_transform_lerp(in[0], in[1], w) : in[0];

    if (isRm) {
        *info = doBlend ? (rminfo_t){
            .motion = r3d_anim_transform_lerp(rm[0].motion, rm[1].motion, w),
            .distance = r3d_anim_transform_lerp(rm[0].distance, rm[1].distance, w)
        } : rm[0];
    }

    return true;
}

static bool anode_eval_add2(const R3D_AnimationTree* atree, r3d_animtree_add2_t* node,
                            int boneIdx, Transform* out, rminfo_t* info)
{
    const R3D_BoneMask* bmask = node->params.boneMask;
    const bool doAdd = !bmask || masked_bone(bmask, boneIdx);
    const bool isRm = info && is_root_bone(atree, boneIdx);

    rminfo_t rm[2] = {0};
    Transform in[2] = {0};

    bool success = anode_eval(atree, node->inMain, boneIdx, &in[0], isRm ? &rm[0] : NULL);
    if (!success) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval add2 node");
        return false;
    }

    if (doAdd) {
        success = anode_eval(atree, node->inAdd, boneIdx, &in[1], isRm ? &rm[1] : NULL);
        if (!success) {
            R3D_TRACELOG(LOG_WARNING, "Failed to eval add2 node");
            return false;
        }
    }

    const float w = CLAMP(node->params.weight, 0.0f, 1.0f);
    *out = doAdd ? r3d_anim_transform_add_v(in[0], in[1], w) : in[0];

    if (isRm) {
        *info = doAdd ? (rminfo_t){
            .motion = r3d_anim_transform_lerp(rm[0].motion, rm[1].motion, w),
            .distance = r3d_anim_transform_lerp(rm[0].distance, rm[1].distance, w)
        } : rm[0];
    }

    return true;
}

static bool anode_eval_switch(const R3D_AnimationTree* atree, r3d_animtree_switch_t* node,
                              int boneIdx, Transform* out, rminfo_t* info)
{
    const bool isRm = info && is_root_bone(atree, boneIdx);
    const float wInvSum = node->weightsInvSum;

    rminfo_t rm = {0};
    Transform result = {0};

    for (int i = 0; i < node->inCount; i++)
    {
        const float w = node->inWeights[i] * wInvSum;
        if (FloatEquals(w, 0.0f)) continue;

        rminfo_t inRm = {0};
        Transform inTr = {0};

        bool success = anode_eval(atree, node->inList[i], boneIdx, &inTr, isRm ? &inRm : NULL);
        if (!success) {
            R3D_TRACELOG(LOG_WARNING, "Failed to eval switch node: input %d failed", i);
            return false;
        }

        result = r3d_anim_transform_addx_v(result, inTr, w);

        if (isRm) {
            rm.motion = r3d_anim_transform_addx_v(rm.motion, inRm.motion, w);
            rm.distance = r3d_anim_transform_addx_v(rm.distance, inRm.distance, w);
        }
    }

    *out = result;
    if (isRm) *info = rm;

    return true;
}

static bool anode_eval_stm(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                           int boneIdx, Transform* out, rminfo_t* info)
{
    const R3D_AnimationStmIndex activeIdx = node->activeIdx;
    const bool isRm = info && is_root_bone(atree, boneIdx);

    rminfo_t activeRm = {0};
    Transform activeTr = {0};

    bool success = anode_eval(atree, node->nodeList[activeIdx], boneIdx, &activeTr, isRm ? &activeRm : NULL);
    if (!success) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval stm state %d", activeIdx);
        return false;
    }

    const r3d_stmedge_t* edge = node->stateList[activeIdx].activeIn;

    if (!edge) {
        *out = activeTr;
        if (isRm) *info = activeRm;
        return true;
    }

    rminfo_t edgeRm = {0};
    Transform edgeTr = {0};

    success = anode_eval(atree, node->nodeList[edge->beginIdx], boneIdx, &edgeTr, isRm ? &edgeRm : NULL);
    if (!success) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval stm state %d", edge->beginIdx);
        return false;
    }

    const float endWeight = CLAMP(edge->endWeight, 0.0f, 1.0f);
    *out = r3d_anim_transform_lerp(edgeTr, activeTr, endWeight);

    if (isRm) {
        *info = (rminfo_t){
            .motion = r3d_anim_transform_lerp(edgeRm.motion, activeRm.motion, endWeight),
            .distance = r3d_anim_transform_lerp(edgeRm.distance, activeRm.distance, endWeight)
        };
    }

    return true;
}

static bool anode_eval_stm_x(const R3D_AnimationTree* atree, r3d_animtree_stm_x_t* node,
                             int boneIdx, Transform* out, rminfo_t* info)
{
    return anode_eval(atree, node->nested, boneIdx, out, info);
}

static bool anode_eval(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                       int boneIdx, Transform* out, rminfo_t* info)
{
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:   return anode_eval_anim(atree, anode.anim, boneIdx, out, info);
    case R3D_ANIMTREE_BLEND2: return anode_eval_blend2(atree, anode.bln2, boneIdx, out, info);
    case R3D_ANIMTREE_ADD2:   return anode_eval_add2(atree, anode.add2, boneIdx, out, info);
    case R3D_ANIMTREE_SWITCH: return anode_eval_switch(atree, anode.swch, boneIdx, out, info);
    case R3D_ANIMTREE_STM:    return anode_eval_stm(atree, anode.stm, boneIdx, out, info);
    case R3D_ANIMTREE_STM_X:  return anode_eval_stm_x(atree, anode.stmx, boneIdx, out, info);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to eval animation tree: invalid node type %d", anode.base->type);
        break;
    }
    return false;
}

// ========================================
// INTERNAL ANIMATION TREE FUNCTIONS
// ========================================

static bool atree_blend2_add(r3d_animtree_blend2_t* parent, R3D_AnimationTreeNode anode, unsigned int in_idx)
{
    switch(in_idx) {
    case 0: parent->inMain = anode; return true;
    case 1: parent->inBlend = anode; return true;
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to add node into blend2: invalid input index %d", in_idx);
        break;
    };
    return false;
}

static bool atree_add2_add(r3d_animtree_add2_t* parent, R3D_AnimationTreeNode anode, unsigned int in_idx)
{
    switch(in_idx) {
    case 0: parent->inMain = anode; return true;
    case 1: parent->inAdd = anode; return true;
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to add node into add2: invalid input index %d", in_idx);
    };
    return false;
}

static bool atree_switch_add(r3d_animtree_switch_t* parent, R3D_AnimationTreeNode anode, unsigned int in_idx)
{
    if (in_idx < parent->inCount) {
        parent->inList[in_idx] = anode;
        return true;
    }
    R3D_TRACELOG(LOG_WARNING, "Failed to add node into switch: invalid input index %d", in_idx);
    return false;
}

static R3D_AnimationTreeNode* atree_anim_create(R3D_AnimationTree* atree, R3D_AnimationNodeParams params)
{
    const R3D_Animation* a = R3D_GetAnimation(atree->player.animLib, params.name);
    if (!a) {
        R3D_TRACELOG(LOG_WARNING, "Failed to create animation node: animation \"%s\" not found", params.name);
        return NULL;
    }
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_ANIM, sizeof(r3d_animtree_anim_t));
    if (!anode) return NULL;

    r3d_animtree_anim_t* anim = anode->anim;
    anim->animation = a;
    anim->params = params;

    int boneIdx = atree->rootBone;
    if (valid_root_bone(boneIdx)) {
        const R3D_AnimationState* s = &anim->params.state;
        const R3D_AnimationChannel* c = r3d_anim_channel_find(a, boneIdx);
        if (c) anim->root.last = r3d_anim_channel_lerp(
            c, s->currentTime * a->ticksPerSecond,
            &anim->root.rest0, &anim->root.restN
        );
        anim->root.loops = -1;
    }

    return anode;
}

static R3D_AnimationTreeNode* atree_blend2_create(R3D_AnimationTree* atree, R3D_Blend2NodeParams params)
{
    R3D_AnimationTreeNode* anode = anode_create(
        atree, R3D_ANIMTREE_BLEND2,
        sizeof(r3d_animtree_blend2_t)
    );
    if (!anode) return NULL;

    anode->bln2->params = params;
    return anode;
}

static R3D_AnimationTreeNode* atree_add2_create(R3D_AnimationTree* atree, R3D_Add2NodeParams params)
{
    R3D_AnimationTreeNode* anode = anode_create(
        atree, R3D_ANIMTREE_ADD2,
        sizeof(r3d_animtree_add2_t)
    );
    if (!anode) return NULL;

    anode->add2->params = params;
    return anode;
}

static R3D_AnimationTreeNode* atree_switch_create(R3D_AnimationTree* atree, unsigned int inCount, R3D_SwitchNodeParams params)
{
    R3D_AnimationTreeNode* anode = anode_create(
        atree, R3D_ANIMTREE_SWITCH,
        sizeof(r3d_animtree_switch_t)
    );
    if (!anode) return NULL;

    r3d_animtree_switch_t* swch = anode->swch;
    swch->inList = RL_MALLOC(inCount * sizeof(*swch->inList));
    swch->inWeights = RL_CALLOC(inCount, sizeof(*swch->inWeights));
    swch->inCount = inCount;
    swch->params = params;

    swch->inWeights[params.activeInput] = 1.0f;
    return anode;
}

static R3D_AnimationTreeNode* atree_stm_create(R3D_AnimationTree* atree, unsigned int statesCount, unsigned int edgesCount, bool travel)
{
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_STM, sizeof(r3d_animtree_stm_t));
    if (!anode) return NULL;

    r3d_animtree_stm_t* stm = anode->stm;
    stm->nodeList = RL_MALLOC(statesCount * sizeof(*stm->nodeList));
    stm->edgeList = RL_MALLOC(edgesCount * sizeof(*stm->edgeList));
    stm->stateList = RL_MALLOC(statesCount * sizeof(*stm->stateList));
    stm->visitList = RL_MALLOC(statesCount * sizeof(*stm->visitList));
    stm->maxStates = statesCount;
    stm->maxEdges = edgesCount;
    if (travel) {
        stm->path.edges = RL_MALLOC(statesCount * sizeof(*stm->path.edges));
        stm->path.open = RL_MALLOC(edgesCount * statesCount * sizeof(*stm->path.open));
        stm->path.next = RL_MALLOC(edgesCount * statesCount * sizeof(*stm->path.next));
        stm->path.mark = RL_MALLOC(statesCount * sizeof(*stm->path.mark));
    }
    return anode;
}

static R3D_AnimationTreeNode* atree_stm_x_create(R3D_AnimationTree* atree, R3D_AnimationTreeNode nested)
{
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_STM_X, sizeof(r3d_animtree_stm_x_t));
    if (!anode) return NULL;

    anode->stmx->nested = nested;
    return anode;
}

static R3D_AnimationStmIndex atree_state_create(r3d_animtree_stm_t* node, R3D_AnimationTreeNode anode, unsigned int edgesCount)
{
    R3D_AnimationStmIndex nextIdx = node->statesCount;
    if (nextIdx >= node->maxStates) return -1;

    r3d_stmstate_t* state = &node->stateList[nextIdx];
    *state = (r3d_stmstate_t){
        .outList = (edgesCount > 0 ? RL_MALLOC(edgesCount * sizeof(*state->outList)) : NULL),
        .outCount = 0,
        .maxOut = edgesCount,
        .activeIn = NULL
    };

    // TODO: Is this reset needed? It sets correct currentTime for animation with negative speed,
    // but may be dangerous for uncomplete animation nodes, like Switch with not yet connected input
    // anode_reset(anode);
    node->nodeList[nextIdx] = anode;
    node->statesCount += 1;

    return nextIdx;
}

static R3D_AnimationStmIndex atree_edge_create(r3d_animtree_stm_t* node, R3D_AnimationStmIndex beginIdx,
                                               R3D_AnimationStmIndex endIdx, R3D_StmEdgeParams params)
{
    R3D_AnimationStmIndex nextIdx = node->edgesCount;
    if (nextIdx >= node->maxEdges) return -1;

    r3d_stmedge_t* edge = &node->edgeList[nextIdx];
    *edge = (r3d_stmedge_t) {
        .beginIdx = beginIdx,
        .endIdx = endIdx,
        .endWeight = 0.0f,
        .params = params
    };

    r3d_stmstate_t* beginState = &node->stateList[beginIdx];
    unsigned int outCount = beginState->outCount;
    if (outCount >= beginState->maxOut) return -1;
    beginState->outList[outCount] = edge;
    beginState->outCount += 1;

    node->edgesCount += 1;
    return nextIdx;
}

static void atree_delete(R3D_AnimationTreeNode anode)
{
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:
    case R3D_ANIMTREE_BLEND2:
    case R3D_ANIMTREE_ADD2:
    case R3D_ANIMTREE_STM_X:
        return;
    case R3D_ANIMTREE_SWITCH:
        RL_FREE(anode.swch->inList);
        RL_FREE(anode.swch->inWeights);
        return;
    case R3D_ANIMTREE_STM:
        for (int i = 0; i < anode.stm->statesCount; i++) {
            if (anode.stm->stateList[i].outList) {
                RL_FREE(anode.stm->stateList[i].outList);
            }
        }
        RL_FREE(anode.stm->nodeList);
        RL_FREE(anode.stm->edgeList);
        RL_FREE(anode.stm->stateList);
        RL_FREE(anode.stm->visitList);
        if (anode.stm->path.edges) RL_FREE(anode.stm->path.edges);
        if (anode.stm->path.open) RL_FREE(anode.stm->path.open);
        if (anode.stm->path.next) RL_FREE(anode.stm->path.next);
        if (anode.stm->path.mark) RL_FREE(anode.stm->path.mark);
        return;
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to delete node: invalid type %d", anode.base->type);
        break;
    }
}

static void atree_update(R3D_AnimationTree* atree, float elapsedTime,
                         Transform* rootMotion, Transform* rootDistance)
{
    if (elapsedTime < 0.0f) return;

    R3D_AnimationPlayer* player = &atree->player;
    const int boneCount = player->skeleton.boneCount;

    bool success = anode_update(atree, *atree->rootNode, elapsedTime, NULL);
    if (!success) goto failure;

    for (int boneIdx = 0; boneIdx < boneCount; boneIdx++)
    {
        const bool isRootBone = is_root_bone(atree, boneIdx);
        rminfo_t rmInfo = {0};
        Transform out = {0};

        success = anode_eval(atree, *atree->rootNode, boneIdx, &out, isRootBone ? &rmInfo : NULL);
        if (!success) goto failure;

        if (isRootBone) {
            if (rootMotion) *rootMotion = rmInfo.motion;
            if (rootDistance) *rootDistance = rmInfo.distance;
            out = r3d_anim_transform_subtr(out, rmInfo.distance);
        }

        if (atree->updateCallback) {
            atree->updateCallback(player, boneIdx, &out, atree->updateUserData);
        }

        player->localPose[boneIdx] = r3d_matrix_srt_quat(out.scale, out.rotation, out.translation);
    }

    r3d_anim_matrices_compute(player);
    R3D_UploadAnimationPlayerPose(player);
    return;

failure:
    R3D_TRACELOG(LOG_ERROR, "Animation tree failed");
    memcpy(player->localPose, player->skeleton.localBind, boneCount * sizeof(Matrix));
    memcpy(player->modelPose, player->skeleton.modelBind, boneCount * sizeof(Matrix));
    R3D_UploadAnimationPlayerPose(player);
}

static void atree_travel(r3d_animtree_stm_t* node, R3D_AnimationStmIndex targetIdx)
{
    if (node->activeIdx == targetIdx) return;

    bool found = stm_find_path(node, targetIdx);
    if (!found) {
        r3d_stmstate_t* state = &node->stateList[targetIdx];
        R3D_AnimationTreeNode anode = node->nodeList[targetIdx];
        state->activeIn = NULL;
        node->activeIdx = targetIdx;
        node->path.len = 0;
        anode_reset(anode);
    }
}

// ========================================
// PUBLIC API
// ========================================

R3D_AnimationTree R3D_LoadAnimationTree(R3D_AnimationPlayer player, int maxSize)
{
    return R3D_LoadAnimationTreePro(player, maxSize, -1, NULL, NULL);
}

R3D_AnimationTree R3D_LoadAnimationTreeEx(R3D_AnimationPlayer player, int maxSize, int rootBone)
{
    return R3D_LoadAnimationTreePro(player, maxSize, rootBone, NULL, NULL);
}

R3D_AnimationTree R3D_LoadAnimationTreePro(R3D_AnimationPlayer player, int maxSize, int rootBone,
                                           R3D_AnimationTreeCallback updateCallback,
                                           void* updateUserData)
{
    R3D_AnimationTree tree = {0};
    tree.player = player;
    tree.nodePool = RL_MALLOC(maxSize * sizeof(*tree.nodePool));
    tree.nodePoolMaxSize = maxSize;
    tree.rootBone = rootBone;
    tree.updateCallback = updateCallback;
    tree.updateUserData = updateUserData;
    return tree;
}

void R3D_UnloadAnimationTree(R3D_AnimationTree tree)
{
    int poolSize = tree.nodePoolSize;
    for (int i = 0; i < poolSize; i++) {
        R3D_AnimationTreeNode node = tree.nodePool[i];
        atree_delete(node);
        RL_FREE(node.base);
    }
    RL_FREE(tree.nodePool);
}

void R3D_UpdateAnimationTree(R3D_AnimationTree* tree, float dt)
{
    R3D_UpdateAnimationTreeEx(tree, dt, NULL, NULL);
}

void R3D_UpdateAnimationTreeEx(R3D_AnimationTree* tree, float dt, Transform* rootMotion, Transform* rootDistance)
{
    atree_update(tree, dt, rootMotion, rootDistance);
}

void R3D_AddRootAnimationNode(R3D_AnimationTree* tree, R3D_AnimationTreeNode* node)
{
    tree->rootNode = node;
}

bool R3D_AddAnimationNode(R3D_AnimationTreeNode* parent, R3D_AnimationTreeNode* node, unsigned int inputIndex)
{
    switch(parent->base->type) {
    case R3D_ANIMTREE_ANIM:
    case R3D_ANIMTREE_STM:
    case R3D_ANIMTREE_STM_X:  return false;
    case R3D_ANIMTREE_BLEND2: return atree_blend2_add(parent->bln2, *node, inputIndex);
    case R3D_ANIMTREE_ADD2:   return atree_add2_add(parent->add2, *node, inputIndex);
    case R3D_ANIMTREE_SWITCH: return atree_switch_add(parent->swch, *node, inputIndex);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to add animation node: invalid parent type %d", parent->base->type);
        break;
    }
    return false;
}

R3D_AnimationTreeNode* R3D_CreateAnimationNode(R3D_AnimationTree* tree, R3D_AnimationNodeParams params)
{
    return atree_anim_create(tree, params);
}

R3D_AnimationTreeNode* R3D_CreateBlend2Node(R3D_AnimationTree* tree, R3D_Blend2NodeParams params)
{
    return atree_blend2_create(tree, params);
}

R3D_AnimationTreeNode* R3D_CreateAdd2Node(R3D_AnimationTree* tree, R3D_Add2NodeParams params)
{
    return atree_add2_create(tree, params);
}

R3D_AnimationTreeNode* R3D_CreateSwitchNode(R3D_AnimationTree* tree, unsigned int inputCount, R3D_SwitchNodeParams params)
{
    return atree_switch_create(tree, inputCount, params);
}

R3D_AnimationTreeNode* R3D_CreateStmNode(R3D_AnimationTree* tree, unsigned int statesCount, unsigned int edgesCount)
{
    return atree_stm_create(tree, statesCount, edgesCount, false);
}

R3D_AnimationTreeNode* R3D_CreateStmNodeEx(R3D_AnimationTree* tree, unsigned int statesCount, unsigned int edgesCount, bool enableTravel)
{
    return atree_stm_create(tree, statesCount, edgesCount, enableTravel);
}

R3D_AnimationTreeNode* R3D_CreateStmXNode(R3D_AnimationTree* tree, R3D_AnimationTreeNode* nestedNode)
{
    return atree_stm_x_create(tree, *nestedNode);
}

R3D_AnimationStmIndex R3D_CreateStmNodeState(R3D_AnimationTreeNode* stmNode, R3D_AnimationTreeNode* stateNode, unsigned int outEdgesCount)
{
    return atree_state_create(stmNode->stm, *stateNode, outEdgesCount);
}

R3D_AnimationStmIndex R3D_CreateStmNodeEdge(R3D_AnimationTreeNode* stmNode, R3D_AnimationStmIndex beginStateIndex,
                                            R3D_AnimationStmIndex endStateIndex, R3D_StmEdgeParams params)
{
    return atree_edge_create(stmNode->stm, beginStateIndex, endStateIndex, params);
}

void R3D_SetAnimationNodeParams(R3D_AnimationTreeNode* node, R3D_AnimationNodeParams params)
{
    node->anim->params = params;
}

R3D_AnimationNodeParams R3D_GetAnimationNodeParams(R3D_AnimationTreeNode* node)
{
    return node->anim->params;
}

void R3D_SetBlend2NodeParams(R3D_AnimationTreeNode* node, R3D_Blend2NodeParams params)
{
    node->bln2->params = params;
}

R3D_Blend2NodeParams R3D_GetBlend2NodeParams(R3D_AnimationTreeNode* node)
{
    return node->bln2->params;
}

void R3D_SetAdd2NodeParams(R3D_AnimationTreeNode* node, R3D_Add2NodeParams params)
{
    node->add2->params = params;
}

R3D_Add2NodeParams R3D_GetAdd2NodeParams(R3D_AnimationTreeNode* node)
{
    return node->add2->params;
}

void R3D_SetSwitchNodeParams(R3D_AnimationTreeNode* node, R3D_SwitchNodeParams params)
{
    node->swch->params = params;
}

R3D_SwitchNodeParams R3D_GetSwitchNodeParams(R3D_AnimationTreeNode* node)
{
    return node->swch->params;
}

void R3D_SetStmNodeEdgeParams(R3D_AnimationTreeNode* node, R3D_AnimationStmIndex edgeIndex, R3D_StmEdgeParams params)
{
    node->stm->edgeList[edgeIndex].params = params;
}

R3D_StmEdgeParams R3D_GetStmNodeEdgeParams(R3D_AnimationTreeNode* node, R3D_AnimationStmIndex edgeIndex)
{
    return node->stm->edgeList[edgeIndex].params;
}

R3D_AnimationStmIndex R3D_GetStmStateActiveIndex(R3D_AnimationTreeNode* node)
{
    return node->stm->activeIdx;
}

void R3D_TravelToStmState(R3D_AnimationTreeNode* node, R3D_AnimationStmIndex targetStateIndex)
{
    atree_travel(node->stm, targetStateIndex);
}

R3D_BoneMask R3D_ComputeBoneMask(const R3D_Skeleton* skeleton, const char* boneNames[], unsigned int boneNameCount)
{
    const R3D_BoneInfo* bones = skeleton->bones;
    const int bCount = skeleton->boneCount;
    R3D_BoneMask bMask = {.boneCount = bCount};

    if (bCount > sizeof(bMask.mask) * 8) {
        R3D_TRACELOG(LOG_WARNING, "Failed to compute bone mask: max bone count exceeded (%d)", sizeof(bMask.mask) * 8);
        return (R3D_BoneMask){0};
    }

    int maskBits = sizeof(bMask.mask[0]) * 8;
    for (int i = 0; i < boneNameCount; i++) {
        const char* name = boneNames[i];
        bool found = false;
        for (int bIdx = 0; bIdx < bCount; bIdx++)
            if (!strncmp(name, bones[bIdx].name, sizeof(bones[bIdx].name))) {
                int part = bIdx / maskBits;
                int bit  = bIdx % maskBits;
                bMask.mask[part] |= 1 << bit;
                found = true;
                break;
            }
        if (!found) {
            R3D_TRACELOG(LOG_WARNING, "Failed to compute bone mask: bone \"%s\" not found", name);
            return (R3D_BoneMask){0};
        }
    }

    return bMask;
}
