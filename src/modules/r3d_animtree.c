/* r3d_animtree.c -- Internal R3D animation tree module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <stdlib.h>

#include "../common/r3d_math.h"
#include "./r3d_animtree.h"

// ========================================
// TREE UPDATE/EVAL SUPPORT INFO STRUCTURES
// ========================================

typedef struct {
    bool  anode_done;
    float xfade;
    float consumed_t;
} upinfo_t;

typedef struct {
    Transform motion;
    Transform distance;
} rminfo_t;

// ========================================
// TREE NODE COMPUTE FUNCTION PROTOTYPES
// ========================================

static bool anode_update(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                         float elapsed_time, upinfo_t* info);

static bool anode_eval(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                       int bone_idx, Transform* out, rminfo_t* info);

static void anode_reset(R3D_AnimationTreeNode anode);

// ========================================
// BONE FUNCTIONS
// ========================================

static bool is_root_bone(const R3D_AnimationTree* atree, int bone_idx) {
    return atree->rootBone == bone_idx;
}

static bool valid_root_bone(int bone_idx) {
    return bone_idx >= 0;
}

static bool masked_bone(const R3D_BoneMask* bmask, int bone_idx) {
    int maskb = (sizeof(bmask->mask[0]) * 8);
    int part  = bone_idx / maskb;
    int bit   = bone_idx % maskb;
    return (bmask->mask[part] & (1 << bit)) != 0;
}

// ========================================
// TRANSFORM/MATRIX FUNCTIONS
// ========================================

static Transform transform_lerp(Transform tf_a, Transform tf_b, float value) {
    Transform result;
    result.translation = Vector3Lerp(tf_a.translation, tf_b.translation, value);
    result.rotation    = QuaternionSlerp(tf_a.rotation, tf_b.rotation, value);
    result.scale       = Vector3Lerp(tf_a.scale, tf_b.scale, value);
    return result;
}

static Transform transform_add(Transform tf_a, Transform tf_b) {
    Transform result;
    result.translation = Vector3Add(tf_a.translation, tf_b.translation);
    result.rotation    = QuaternionAdd(tf_a.rotation, tf_b.rotation);
    result.scale       = Vector3Add(tf_a.scale, tf_b.scale);
    return result;
}

static Transform transform_add_v(Transform tf_a, Transform tf_b, float value) {
    Transform result;
    result.translation = Vector3Add(tf_a.translation,
                                    Vector3Scale(tf_b.translation, value));
    result.rotation    = QuaternionAdd(tf_a.rotation,
                                       QuaternionScale(tf_b.rotation, value));
    result.scale       = Vector3Add(tf_a.scale,
                                    Vector3Scale(tf_b.scale, value));
    return result;
}

static Transform transform_addx_v(Transform tf_a, Transform tf_b, float value) {
    Transform result;
    result.translation = Vector3Add(tf_a.translation,
                                    Vector3Scale(tf_b.translation, value));
    result.rotation    = QuaternionSlerp(tf_a.rotation, tf_b.rotation, value);
    result.scale       = Vector3Add(tf_a.scale,
                                    Vector3Scale(tf_b.scale, value));
    return result;
}

static Transform transform_subtr(Transform tf_a, Transform tf_b) {
    Transform result;
    result.translation = Vector3Subtract(tf_a.translation, tf_b.translation);
    result.rotation    = QuaternionSubtract(tf_a.rotation, tf_b.rotation);
    result.scale       = Vector3Subtract(tf_a.scale, tf_b.scale);
    return result;
}

static Transform transform_scale(Transform tf, float val) {
    Transform result;
    result.translation = Vector3Scale(tf.translation, val);
    result.rotation    = QuaternionScale(tf.rotation, val);
    result.scale       = Vector3Scale(tf.scale, val);
    return result;
}

static void compute_model_matrices(R3D_AnimationPlayer* player) {
    const R3D_BoneInfo* bones      = player->skeleton.bones;
    const Matrix        root_bind  = player->skeleton.rootBind;
    const Matrix*       local_pose = player->localPose;
    const int           bone_cnt   = player->skeleton.boneCount;

    Matrix* pose = player->modelPose;
    for(int bone_idx = 0; bone_idx < bone_cnt; bone_idx++) {
        int    parent_idx  = bones[bone_idx].parent;
        Matrix parent_pose = parent_idx >= 0 ? pose[parent_idx] : root_bind;

        pose[bone_idx] = MatrixMultiply(local_pose[bone_idx], parent_pose);
    }
}

// ========================================
// ANIMATION CHANNEL FUNCTIONS
// ========================================

static const R3D_AnimationChannel* find_bone_channel(const R3D_Animation* anim,
                                                     int bone_idx) {
    for(int i = 0; i < anim->channelCount; i++)
        if(anim->channels[i].boneIndex == bone_idx)
            return &anim->channels[i];
    return NULL;
}

static void find_key_frames(const float* times, uint32_t count, float time,
                            uint32_t* out_idx_0, uint32_t* out_idx_1,
                            float* out_t) {
    // No keys
    if(count == 0) {
        *out_idx_0 = *out_idx_1 = 0;
        *out_t     = 0.0f;
        return;
    }
    // Single key or before first
    if(count == 1 || time <= times[0]) {
        *out_idx_0 = *out_idx_1 = 0;
        *out_t     = 0.0f;
        return;
    }
    // After last
    if(time >= times[count - 1]) {
        *out_idx_0 = *out_idx_1 = count - 1;
        *out_t     = 0.0f;
        return;
    }
    // Binary search
    uint32_t left = 0;
    uint32_t right = count - 1;
    while(right - left > 1) {
        uint32_t mid = (left + right) >> 1;
        if(times[mid] <= time) left  = mid;
        else                   right = mid;
    }
    *out_idx_0 = left;
    *out_idx_1 = right;

    const float t0 = times[left];
    const float t1 = times[right];
    const float dt = t1 - t0;
    *out_t = (dt > 0.0f) ? (time - t0) / dt : 0.0f;
}

static Transform channel_lerp(const R3D_AnimationChannel* channel, float time,
                              Transform* rest_0, Transform* rest_n) {
    Transform result = {.translation = {0.0f, 0.0f, 0.0f},
                        .rotation    = {0.0f, 0.0f, 0.0f, 1.0f},
                        .scale       = {1.0f, 1.0f, 1.0f}};
    if(channel->translation.count > 0) {
        const Vector3* values = (const Vector3*)channel->translation.values;
        uint32_t       i0, i1;
        float          t;
        find_key_frames(channel->translation.times,
                        channel->translation.count,
                        time, &i0, &i1, &t);
        result.translation = Vector3Lerp(values[i0], values[i1], t);
        if(rest_0) rest_0->translation = values[0];
        if(rest_n) rest_n->translation = values[channel->translation.count-1];
    }
    if(channel->rotation.count > 0) {
        const Quaternion* values = (const Quaternion*)channel->rotation.values;
        uint32_t          i0, i1;
        float             t;
        find_key_frames(channel->rotation.times,
                        channel->rotation.count,
                        time, &i0, &i1, &t);
        result.rotation = QuaternionSlerp(values[i0], values[i1], t);
        if(rest_0) rest_0->rotation = values[0];
        if(rest_n) rest_n->rotation = values[channel->rotation.count-1];
    }
    if(channel->scale.count > 0) {
        const Vector3* values = (const Vector3*)channel->scale.values;
        uint32_t       i0, i1;
        float          t;
        find_key_frames(channel->scale.times,
                        channel->scale.count,
                        time, &i0, &i1, &t);
        result.scale = Vector3Lerp(values[i0], values[i1], t);
        if(rest_0) rest_0->scale = values[0];
        if(rest_n) rest_n->scale = values[channel->scale.count-1];
    }
    return result;
}

// ========================================
// ANIMATION STATE MACHINE
// ========================================

static r3d_stmedge_t* stm_find_edge(const r3d_animtree_stm_t* node,
                                    const r3d_stmstate_t* state) {
    unsigned int path_len = node->path.len;
    unsigned int path_idx = node->path.idx;
    if(path_idx < path_len) return node->path.edges[path_idx];
    else {
        unsigned int edges_cnt = state->out_cnt;
        for(int e_idx = 0; e_idx < edges_cnt; e_idx++) {
            r3d_stmedge_t*    edge = state->out_list[e_idx];
            R3D_StmEdgeStatus stat = edge->params.currentStatus;
            bool              open = (stat == R3D_STM_EDGE_AUTO ||
                                      stat == R3D_STM_EDGE_ONCE);
            if(open) return edge;
        }
    }
    return NULL;
}

static bool stm_update_edge(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                            r3d_stmedge_t* edge, float elapsed_time, float* consumed_time,
                            bool* done)
{
    float xfade    = edge->params.xFadeTime;
    bool  do_xfade = (xfade > elapsed_time);
    if(do_xfade) {
        float w_incr = Remap(elapsed_time, 0.0f, xfade, 0.0f, 1.0f);
        edge->end_weight += w_incr;

        float w_clamp = CLAMP(edge->end_weight, 0.0f, 1.0f);
        float w_delta = edge->end_weight - w_clamp;
        edge->end_weight = w_clamp;
        *consumed_time   = (w_incr > 0.0f ?
                            elapsed_time * (1.0f - w_delta/w_incr) :
                            elapsed_time);
        *done            = FloatEquals(edge->end_weight, 1.0f);
    } else {
        edge->end_weight = 1.0f;
        *consumed_time   = 0.0f;
        *done            = true;
    }

    if(*done) return true;
    else      return anode_update(atree, node->node_list[edge->beg_idx],
                                  elapsed_time, NULL);
}

static bool stm_next_state(r3d_animtree_stm_t* node, r3d_stmedge_t* edge,
                           bool edge_done, bool node_done, R3D_AnimationStmIndex* next_idx) {
    R3D_StmEdgeMode mode = edge->params.mode;

    bool ready = ((mode == R3D_STM_EDGE_INSTANT && edge_done) ||
                  (mode == R3D_STM_EDGE_ONDONE  && node_done));
    if(!ready) return false;

    R3D_AnimationStmIndex end_idx = edge->end_idx;
    r3d_stmstate_t*       state   = &node->state_list[end_idx];
    R3D_AnimationTreeNode anode   = node->node_list[end_idx];

    state->active_in = edge;
    anode_reset(anode);

    edge->end_weight = 0.0f;
    if(edge->params.currentStatus == R3D_STM_EDGE_ONCE)
        edge->params.currentStatus = edge->params.nextStatus;

    *next_idx = end_idx;
    return true;
}

static bool stm_update_state(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                             float elapsed_time, float* consumed_time,
                             R3D_AnimationStmIndex* act_idx, bool* do_next, bool* stm_done) {
    R3D_AnimationTreeNode* node_list = node->node_list;
    r3d_stmstate_t*        state     = &node->state_list[*act_idx];

    bool           edge_done = true;
    float          edge_time = 0.0f;
    r3d_stmedge_t* act_edge  = state->active_in;
    if(act_edge)
        if(!stm_update_edge(atree, node, act_edge, elapsed_time,
                            &edge_time, &edge_done))
            return false;
    if(edge_done) state->active_in = NULL;

    r3d_stmedge_t* next_edge = stm_find_edge(node, state);
    upinfo_t       node_info = {.xfade = (next_edge ?
                                          next_edge->params.xFadeTime :
                                          0.0f)};
    if(!anode_update(atree, node_list[*act_idx], elapsed_time, &node_info))
        return false;

    bool node_done = edge_done && node_info.anode_done;
    bool is_next   = (next_edge ?
                      stm_next_state(node, next_edge, edge_done, node_done,
                                     act_idx) :
                      false);
    *stm_done      = (edge_done &&
                      (node_list[*act_idx].base->type == R3D_ANIMTREE_STM_X));
    *do_next       = node_done && is_next && !*stm_done;
    *consumed_time = MAX(edge_time, node_info.consumed_t);
    return true;
}

static int expand_path(r3d_animtree_stm_t* node, r3d_stmedge_t** edges, r3d_stmedge_t** next,
                       int next_cnt, unsigned int path_len, const r3d_stmstate_t* state) {
    unsigned int max_open_paths = node->max_edges;
    unsigned int max_path_len   = node->max_states;
    unsigned int out_cnt        = state->out_cnt;

    int added = 0;
    for(int e_idx = 0; e_idx < out_cnt; e_idx++) {
        r3d_stmedge_t* edge = state->out_list[e_idx];

        bool closed = edge->params.currentStatus == R3D_STM_EDGE_OFF;
        if(closed) continue;

        if(next_cnt < max_open_paths) {
            memcpy(&next[next_cnt*max_path_len], edges, path_len * sizeof(r3d_stmedge_t*));
            next[next_cnt*max_path_len + path_len] = edge;

            next_cnt += 1;
            added    += 1;
        } else {
            R3D_TRACELOG(LOG_WARNING, "Failed to find path: max open paths count exceeded (%d)",
                         max_open_paths);
            break;
        }
    }
    return added;
}

static bool stm_find_path(r3d_animtree_stm_t* node, R3D_AnimationStmIndex target_idx) {
    unsigned int    max_path_len = node->max_states;
    r3d_stmedge_t** open_paths   = node->path.open;
    r3d_stmedge_t** next_paths   = node->path.next;
    bool*           marked       = node->path.mark;
    memset(marked, false, node->states_cnt * sizeof(*node->path.mark));
    marked[node->active_idx] = true;

    int paths_cnt = expand_path(node, open_paths, open_paths, 0, 0,
                                &node->state_list[node->active_idx]);

    unsigned int path_len = 1;
    while(paths_cnt > 0) {
        int next_cnt = 0;
        for(int p_idx = 0; p_idx < paths_cnt; p_idx++) {
            R3D_AnimationStmIndex state_idx = open_paths[p_idx*max_path_len + path_len-1]->end_idx;
            if(state_idx == target_idx) {
                memcpy(node->path.edges, &open_paths[p_idx*max_path_len],
                       path_len * sizeof(r3d_stmedge_t*));
                node->path.idx = 0;
                node->path.len = path_len;
                return true;
            }
            if(marked[state_idx]) continue;
            else marked[state_idx] = true;

            next_cnt += expand_path(node, &open_paths[p_idx*max_path_len], next_paths,
                                    next_cnt, path_len, &node->state_list[state_idx]);
        }

        if(path_len < max_path_len) path_len += 1;
        else {
            R3D_TRACELOG(LOG_WARNING, "Failed to find path: max path lenght exceeded (%d)",
                         max_path_len);
            break;
        }

        memcpy(open_paths, next_paths, max_path_len * next_cnt * sizeof(r3d_stmedge_t*));
        paths_cnt = next_cnt;
    }
    return false;
}

// ========================================
// TREE NODE CREATE FUNCTION
// ========================================

static R3D_AnimationTreeNode* anode_create(R3D_AnimationTree* atree, r3d_animtree_type_t type,
                                           size_t node_size) {
    R3D_AnimationTreeNode* node;

    unsigned int pool_size = atree->nodePoolSize;
    if(pool_size < atree->nodePoolMaxSize) {
        node             = &atree->nodePool[pool_size];
        node->base       = RL_CALLOC(1, node_size);
        node->base->type = type;
    } else return NULL;

    atree->nodePoolSize += 1;
    return node;
}

// ========================================
// TREE NODE RESET FUNCTIONS
// ========================================

static void anode_reset_anim(r3d_animtree_anim_t* node) {
    const R3D_Animation* a = node->animation;
    R3D_AnimationState*  s = &node->params.state;

    float duration = a->duration / a->ticksPerSecond;
    s->currentTime = (s->speed >= 0.0f) ? 0.0f : duration;
}

static void anode_reset_blend2(r3d_animtree_blend2_t* node) {
    anode_reset(node->in_main);
    anode_reset(node->in_blend);
}

static void anode_reset_add2(r3d_animtree_add2_t* node) {
    anode_reset(node->in_main);
    anode_reset(node->in_add);
}

static void anode_reset_switch(r3d_animtree_switch_t* node) {
    unsigned char in_count  = node->in_cnt;
    unsigned char active_in = node->params.activeInput;

    for(int i = 0; i < in_count; i++)
        node->in_weights[i] = 0.0f;
    node->in_weights[active_in] = 1.0f;

    for(int i = 0; i < in_count; i++)
        anode_reset(node->in_list[active_in]);
}

static void anode_reset_stm(r3d_animtree_stm_t* node) {
    node->active_idx = 0;

    r3d_stmedge_t* edge = node->state_list[0].active_in;
    if(edge) edge->end_weight = 0.0f;

    anode_reset(node->node_list[0]);
}

// other stm reset version: reset only active node+state
/*
static void anode_reset_stm(r3d_animtree_stm_t* node) {
    unsigned int   act_idx = node->active_idx;
    r3d_stmedge_t* edge    = node->state_list[act_idx].active_in;
    if(edge) edge->end_weight = 0.0f;

    anode_reset(node->node_list[act_idx]);
}
*/

static void anode_reset_stm_x(r3d_animtree_stm_x_t* node) {
    anode_reset(node->nested);
}

static void anode_reset(R3D_AnimationTreeNode anode) {
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:   return anode_reset_anim(anode.anim);
    case R3D_ANIMTREE_BLEND2: return anode_reset_blend2(anode.bln2);
    case R3D_ANIMTREE_ADD2:   return anode_reset_add2(anode.add2);
    case R3D_ANIMTREE_SWITCH: return anode_reset_switch(anode.swch);
    case R3D_ANIMTREE_STM:    return anode_reset_stm(anode.stm);
    case R3D_ANIMTREE_STM_X:  return anode_reset_stm_x(anode.stmx);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to reset node: invalid type %d",
                     anode.base->type);
    }
}

// ========================================
// TREE NODE UPDATE FUNCTIONS
// ========================================

static bool anode_update_anim(const R3D_AnimationTree* atree, r3d_animtree_anim_t* node,
                              float elapsed_time, upinfo_t* info) {
    const R3D_Animation* a = node->animation;
    R3D_AnimationState*  s = &node->params.state;
    if(!s->play) {
        if(info) *info = (upinfo_t){.anode_done = true,
                                    .xfade      = info->xfade,
                                    .consumed_t = 0.0f};
        return true;
    }

    float speed    = s->speed;
    float duration = a->duration / a->ticksPerSecond;
    float t_incr   = speed * elapsed_time;
    float t_curr   = s->currentTime + t_incr;
    s->currentTime = t_curr;

    bool cross = ((speed < 0.0f && t_curr <= 0.0f) ||
                  (speed > 0.0f && t_curr >= duration));
    if(cross) {
        if((s->play = s->loop))
            s->currentTime -= copysignf(duration, speed);
        else {
            float t_clamp = CLAMP(s->currentTime, 0.0f, duration);
            float t_delta = t_clamp - s->currentTime;
            elapsed_time  *= 1.0f - t_delta/t_incr;
            s->currentTime = t_clamp;
        }
        node->root.loops = (int)(elapsed_time / duration);
    } else
        node->root.loops = -1;

    if(info) {
        float xf       = info->xfade;
        float dur_xf   = CLAMP(duration - xf, 0.0f, duration);
        bool  cross_xf = ((speed < 0.0f && t_curr <= xf) ||
                          (speed > 0.0f && t_curr >= dur_xf));
        *info = (upinfo_t){.anode_done = cross_xf ? node->params.looper : false,
                           .xfade      = xf,
                           .consumed_t = elapsed_time};
    }
    return true;
}

static bool anode_update_blend2(const R3D_AnimationTree* atree, r3d_animtree_blend2_t* node,
                                float elapsed_time, upinfo_t* info) {
    return (anode_update(atree, node->in_main, elapsed_time, info) &&
            anode_update(atree, node->in_blend, elapsed_time, NULL));
}

static bool anode_update_add2(const R3D_AnimationTree* atree, r3d_animtree_add2_t* node,
                              float elapsed_time, upinfo_t* info) {
    return (anode_update(atree, node->in_main, elapsed_time, info) &&
            anode_update(atree, node->in_add, elapsed_time, NULL));
}

static bool anode_update_switch(const R3D_AnimationTree* atree, r3d_animtree_switch_t* node,
                                float elapsed_time, upinfo_t* info) {
    unsigned int in_count  = node->in_cnt;
    unsigned int active_in = node->params.activeInput;
    if(active_in >= in_count) {
        R3D_TRACELOG(LOG_WARNING, "Failed to update switch: active input %d out of range (%d)",
                     active_in, in_count);
        return false;
    }

    bool reset = (active_in == node->prev_in) ? false : !node->params.synced;
    if(reset) anode_reset(node->in_list[active_in]);

    for(int i = 0; i < in_count; i++)
        if(!anode_update(atree, node->in_list[i], elapsed_time, NULL))
            return false;
    node->prev_in = active_in;

    float xfade    = node->params.xFadeTime;
    bool  no_xfade = (xfade <= elapsed_time);
    if(no_xfade) {
        for(int i = 0; i < in_count; i++)
            node->in_weights[i] = 0.0f;
        node->in_weights[active_in] = 1.0f;
    } else {
        float w_fade = Remap(elapsed_time, 0.0f, xfade, 0.0f, 1.0f);
        for(int i = 0; i < in_count; i++) {
            float w_sign = (i == active_in) ? 1.0f : -1.0f;
            node->in_weights[i] = CLAMP(node->in_weights[i] + w_sign*w_fade,
                                        0.0f, 1.0f);
        }
    }

    float w_sum = 0.0f;
    for(int i = 0; i < in_count; i++)
        w_sum += node->in_weights[i];
    node->weights_isum = 1.0f / w_sum;
    return true;
}

static bool anode_update_stm(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                             float elapsed_time, upinfo_t* info) {
    R3D_AnimationStmIndex act_idx = node->active_idx;

    r3d_stmvisit_t* visited = node->visit_list;
    memset(visited, 0, node->states_cnt * sizeof(*node->visit_list));

    float start_time = elapsed_time;
    bool  do_next    = true;
    bool  stm_done;
    while(do_next) {
        if(visited[act_idx].yes &&
           FloatEquals(visited[act_idx].when, elapsed_time)) {
            R3D_TRACELOG(LOG_WARNING, "Failed to update stm: cycle detected, aborted");
            return false;
        }
        visited[act_idx] = (r3d_stmvisit_t){.yes  = true,
                                            .when = elapsed_time};

        float consumed_time;
        if(!stm_update_state(atree, node, elapsed_time,
                             &consumed_time, &act_idx, &do_next, &stm_done))
            return false;
        elapsed_time -= consumed_time;

        if(do_next) {
            unsigned int path_len = node->path.len;
            unsigned int path_idx = node->path.idx;
            if(path_idx < path_len) node->path.idx += 1;
        }
        if(FloatEquals(elapsed_time, 0.0f))
            break;
        if(elapsed_time < 0.0f) {
            R3D_TRACELOG(LOG_WARNING, "Failed to update stm: incorrect time calculation (%f)",
                         elapsed_time);
            return false;
        }
    }
    node->active_idx = act_idx;

    if(info) *info = (upinfo_t){.anode_done = stm_done,
                                .consumed_t = start_time - elapsed_time};
    return true;
}

static bool anode_update_stm_x(const R3D_AnimationTree* atree, r3d_animtree_stm_x_t* node,
                               float elapsed_time, upinfo_t* info) {
    return anode_update(atree, node->nested, elapsed_time, info);
}

static bool anode_update(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                         float elapsed_time, upinfo_t* info) {
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:   return anode_update_anim(atree, anode.anim,
                                                       elapsed_time, info);
    case R3D_ANIMTREE_BLEND2: return anode_update_blend2(atree, anode.bln2,
                                                         elapsed_time, info);
    case R3D_ANIMTREE_ADD2:   return anode_update_add2(atree, anode.add2,
                                                       elapsed_time, info);
    case R3D_ANIMTREE_SWITCH: return anode_update_switch(atree, anode.swch,
                                                         elapsed_time, info);
    case R3D_ANIMTREE_STM:    return anode_update_stm(atree, anode.stm,
                                                      elapsed_time, info);
    case R3D_ANIMTREE_STM_X:  return anode_update_stm_x(atree, anode.stmx,
                                                        elapsed_time, info);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to update animation tree: invalid node type %d",
                     anode.base->type);
    }
    return false;
}

// ========================================
// TREE NODE EVAL FUNCTIONS
// ========================================

static bool anode_eval_anim(const R3D_AnimationTree* atree, r3d_animtree_anim_t* node,
                            int bone_idx, Transform* out, rminfo_t* info) {
    const R3D_Animation*        a = node->animation;
    const R3D_AnimationChannel* c = find_bone_channel(a, bone_idx);
    R3D_AnimationState          s = node->params.state;

    float time = s.currentTime;
    float tps  = a->ticksPerSecond;
    *out = !c ? (Transform){0} : channel_lerp(c, time * tps, NULL, NULL);
    if(node->params.evalCallback)
        node->params.evalCallback(a, s, bone_idx, out, node->params.evalUserData);

    if(is_root_bone(atree, bone_idx)) {
        if(info) {
            Transform motion = {0};
            float     speed  = s.speed;
            int       loops  = node->root.loops;
            if(loops > 0)
                motion = transform_scale(transform_subtr(node->root.rest_n,
                                                         node->root.rest_0),
                                         (float)loops);
            if(loops >= 0) {
                Transform last   = node->root.last;
                Transform rest_0 = (speed > 0.0f ?
                                    node->root.rest_0 : node->root.rest_n);
                Transform rest_n = (speed > 0.0f ?
                                    node->root.rest_n : node->root.rest_0);
                Transform split  = transform_add(transform_subtr(rest_n, last),
                                                 transform_subtr(*out, rest_0));
                motion          = transform_add(motion, split);
                motion.rotation = QuaternionNormalize(motion.rotation);
                info->motion    = motion;
            } else
                info->motion = transform_subtr(*out, node->root.last);
            info->distance = transform_subtr(*out, node->root.rest_0);
        }
        node->root.last = *out;
    }
    return true;
}

static bool anode_eval_blend2(const R3D_AnimationTree* atree, r3d_animtree_blend2_t* node,
                              int bone_idx, Transform* out, rminfo_t* info) {
    const R3D_BoneMask* bmask = node->params.boneMask;

    bool do_blend = !bmask || masked_bone(bmask, bone_idx);
    bool is_rm    = info && is_root_bone(atree, bone_idx);

    rminfo_t  rm[2];
    Transform in[2]  = {0};
    bool      succ_0 = anode_eval(atree, node->in_main, bone_idx, &in[0],
                                  is_rm ? &rm[0] : NULL);
    bool      succ_1 = (do_blend ?
                        anode_eval(atree, node->in_blend, bone_idx, &in[1],
                                   is_rm ? &rm[1] : NULL) :
                        true);
    if(!succ_0 || !succ_1) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval blend2 node");
        return false;
    }
    float w = CLAMP(node->params.blend, 0.0f, 1.0f);
    *out = (do_blend ?
            transform_lerp(in[0], in[1], w) :
            in[0]);

    if(is_rm)
        *info = (do_blend ?
                 (rminfo_t){.motion   = transform_lerp(rm[0].motion,
                                                       rm[1].motion, w),
                            .distance = transform_lerp(rm[0].distance,
                                                       rm[1].distance, w)} :
                 rm[0]);
    return true;
}

static bool anode_eval_add2(const R3D_AnimationTree* atree, r3d_animtree_add2_t* node,
                            int bone_idx, Transform* out, rminfo_t* info) {
    const R3D_BoneMask* bmask = node->params.boneMask;

    bool do_add = !bmask || masked_bone(bmask, bone_idx);
    bool is_rm  = info && is_root_bone(atree, bone_idx);

    rminfo_t  rm[2];
    Transform in[2]  = {0};
    bool      succ_0 = anode_eval(atree, node->in_main, bone_idx, &in[0],
                                  is_rm ? &rm[0] : NULL);
    bool      succ_1 = (do_add ?
                        anode_eval(atree, node->in_add, bone_idx, &in[1],
                                   is_rm ? &rm[1] : NULL) :
                        true);
    if(!succ_0 || !succ_1) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval add2 node");
        return false;
    }
    float w = CLAMP(node->params.weight, 0.0f, 1.0f);
    *out = (do_add ?
            transform_add_v(in[0], in[1], w) :
            in[0]);

    if(is_rm)
        *info = (do_add ?
                 (rminfo_t){.motion   = transform_lerp(rm[0].motion,
                                                       rm[1].motion, w),
                            .distance = transform_lerp(rm[0].distance,
                                                       rm[1].distance, w)} :
                 rm[0]);
    return true;
}

static bool anode_eval_switch(const R3D_AnimationTree* atree, r3d_animtree_switch_t* node,
                              int bone_idx, Transform* out, rminfo_t* info) {
    unsigned char in_count = node->in_cnt;
    float         w_isum   = node->weights_isum;
    bool          is_rm    = info && is_root_bone(atree, bone_idx);

    rminfo_t  rm    = {0};
    Transform in_tr = {0};
    for(int i = 0; i < in_count; i++) {
        rminfo_t  rm_i;
        Transform in_i = {0};
        bool      succ = anode_eval(atree, node->in_list[i], bone_idx, &in_i,
                                    is_rm ? &rm_i : NULL);
        if(!succ) {
            R3D_TRACELOG(LOG_WARNING, "Failed to eval switch node: input %d failed", i);
            return false;
        }

        float w = node->in_weights[i] * w_isum;
        in_tr = transform_addx_v(in_tr, in_i, w);

        if(is_rm)
            rm = (rminfo_t){.motion   = transform_addx_v(rm.motion,
                                                         rm_i.motion, w),
                            .distance = transform_addx_v(rm.distance,
                                                         rm_i.distance, w)};
    }
    *out = in_tr;

    if(is_rm) *info = rm;
    return true;
}


static bool anode_eval_stm(const R3D_AnimationTree* atree, r3d_animtree_stm_t* node,
                           int bone_idx, Transform* out, rminfo_t* info) {

    R3D_AnimationStmIndex act_idx = node->active_idx;
    bool                  is_rm   = info && is_root_bone(atree, bone_idx);

    rminfo_t  s_rm;
    Transform s_tr = {0};
    bool      succ = anode_eval(atree, node->node_list[act_idx], bone_idx,
                                &s_tr, is_rm ? &s_rm : NULL);
    if(!succ) {
        R3D_TRACELOG(LOG_WARNING, "Failed to eval stm state %d", act_idx);
        return false;
    }

    const r3d_stmedge_t* edge = node->state_list[act_idx].active_in;
    if(edge) {
        rminfo_t  e_rm;
        Transform e_tr = {0};
        succ = anode_eval(atree, node->node_list[edge->beg_idx], bone_idx,
                          &e_tr, is_rm ? &e_rm : NULL);
        if(!succ) {
            R3D_TRACELOG(LOG_WARNING, "Failed to eval stm state %d", edge->beg_idx);
            return false;
        }

        float e_endw = CLAMP(edge->end_weight, 0.0f, 1.0f);
        *out = transform_lerp(e_tr, s_tr, e_endw);

        if(is_rm)
            *info = (rminfo_t){.motion   = transform_lerp(e_rm.motion,
                                                          s_rm.motion, e_endw),
                               .distance = transform_lerp(e_rm.distance,
                                                          s_rm.distance, e_endw)};
    } else {
        *out = s_tr;

        if(is_rm) *info = s_rm;
    }
    return true;
}

static bool anode_eval_stm_x(const R3D_AnimationTree* atree, r3d_animtree_stm_x_t* node,
                             int bone_idx, Transform* out, rminfo_t* info) {
    return anode_eval(atree, node->nested, bone_idx, out, info);
}

static bool anode_eval(const R3D_AnimationTree* atree, R3D_AnimationTreeNode anode,
                       int bone_idx, Transform* out, rminfo_t* info) {
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:   return anode_eval_anim(atree, anode.anim,
                                                     bone_idx, out, info);
    case R3D_ANIMTREE_BLEND2: return anode_eval_blend2(atree, anode.bln2,
                                                       bone_idx, out, info);
    case R3D_ANIMTREE_ADD2:   return anode_eval_add2(atree, anode.add2,
                                                     bone_idx, out, info);
    case R3D_ANIMTREE_SWITCH: return anode_eval_switch(atree, anode.swch,
                                                       bone_idx, out, info);
    case R3D_ANIMTREE_STM:    return anode_eval_stm(atree, anode.stm,
                                                    bone_idx, out, info);
    case R3D_ANIMTREE_STM_X:  return anode_eval_stm_x(atree, anode.stmx,
                                                      bone_idx, out, info);
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to eval animation tree: invalid node type %d",
                     anode.base->type);
    }
    return false;
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_animtree_blend2_add(r3d_animtree_blend2_t* parent, R3D_AnimationTreeNode anode,
                             unsigned int in_idx) {
    switch(in_idx) {
    case 0: parent->in_main  = anode; return true;
    case 1: parent->in_blend = anode; return true;
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to add node into blend2: invalid input index %d",
                     in_idx);
    };
    return false;
}

bool r3d_animtree_add2_add(r3d_animtree_add2_t* parent, R3D_AnimationTreeNode anode,
                           unsigned int in_idx) {
    switch(in_idx) {
    case 0: parent->in_main = anode; return true;
    case 1: parent->in_add  = anode; return true;
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to add node into add2: invalid input index %d",
                     in_idx);
    };
    return false;
}

bool r3d_animtree_switch_add(r3d_animtree_switch_t* parent, R3D_AnimationTreeNode anode,
                             unsigned int in_idx) {
    if(in_idx < parent->in_cnt) {
        parent->in_list[in_idx] = anode;
        return true;
    }
    R3D_TRACELOG(LOG_WARNING, "Failed to add node into switch: invalid input index %d",
                 in_idx);
    return false;
}

R3D_AnimationTreeNode* r3d_animtree_anim_create(R3D_AnimationTree* atree,
                                                R3D_AnimationNodeParams params) {
    const R3D_Animation* a = R3D_GetAnimation(atree->player.animLib, params.name);
    if(!a) {
        R3D_TRACELOG(LOG_WARNING, "Failed to create animation node: animation \"%s\" not found",
                     params.name);
        return NULL;
    }
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_ANIM,
                                                sizeof(r3d_animtree_anim_t));
    if(!anode) return NULL;

    r3d_animtree_anim_t* anim = anode->anim;
    anim->animation = a;
    anim->params    = params;

    int bone_idx = atree->rootBone;
    if(valid_root_bone(bone_idx)) {
        const R3D_AnimationState*   s = &anim->params.state;
        const R3D_AnimationChannel* c = find_bone_channel(a, bone_idx);
        if(c) anim->root.last = channel_lerp(c, s->currentTime * a->ticksPerSecond,
                                             &anim->root.rest_0,
                                             &anim->root.rest_n);
        anim->root.loops = -1;
    }
    return anode;
}

R3D_AnimationTreeNode* r3d_animtree_blend2_create(R3D_AnimationTree* atree,
                                                  R3D_Blend2NodeParams params) {
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_BLEND2,
                                                sizeof(r3d_animtree_blend2_t));
    if(!anode) return NULL;

    anode->bln2->params = params;
    return anode;
}

R3D_AnimationTreeNode* r3d_animtree_add2_create(R3D_AnimationTree* atree,
                                                R3D_Add2NodeParams params) {
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_ADD2,
                                                sizeof(r3d_animtree_add2_t));
    if(!anode) return NULL;

    anode->add2->params = params;
    return anode;
}

R3D_AnimationTreeNode* r3d_animtree_switch_create(R3D_AnimationTree* atree,
                                                  unsigned int in_cnt,
                                                  R3D_SwitchNodeParams params) {
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_SWITCH,
                                                sizeof(r3d_animtree_switch_t));
    if(!anode) return NULL;

    r3d_animtree_switch_t* swch = anode->swch;
    swch->in_list    = RL_MALLOC(in_cnt * sizeof(*swch->in_list));
    swch->in_weights = RL_MALLOC(in_cnt * sizeof(*swch->in_weights));
    swch->in_cnt     = in_cnt;
    swch->params     = params;
    return anode;
}

R3D_AnimationTreeNode* r3d_animtree_stm_create(R3D_AnimationTree* atree,
                                               unsigned int states_cnt, unsigned int edges_cnt,
                                               bool travel) {
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_STM,
                                                sizeof(r3d_animtree_stm_t));
    if(!anode) return NULL;

    r3d_animtree_stm_t* stm = anode->stm;
    stm->node_list  = RL_MALLOC(states_cnt * sizeof(*stm->node_list));
    stm->edge_list  = RL_MALLOC(edges_cnt * sizeof(*stm->edge_list));
    stm->state_list = RL_MALLOC(states_cnt * sizeof(*stm->state_list));
    stm->visit_list = RL_MALLOC(states_cnt * sizeof(*stm->visit_list));
    stm->max_states = states_cnt;
    stm->max_edges  = edges_cnt;
    if(travel) {
        stm->path.edges = RL_MALLOC(states_cnt * sizeof(*stm->path.edges));
        stm->path.open  = RL_MALLOC(edges_cnt * states_cnt * sizeof(*stm->path.open));
        stm->path.next  = RL_MALLOC(edges_cnt * states_cnt * sizeof(*stm->path.next));
        stm->path.mark  = RL_MALLOC(states_cnt * sizeof(*stm->path.mark));
    }
    return anode;
}

R3D_AnimationTreeNode* r3d_animtree_stm_x_create(R3D_AnimationTree* atree,
                                                 R3D_AnimationTreeNode nested) {
    R3D_AnimationTreeNode* anode = anode_create(atree, R3D_ANIMTREE_STM_X,
                                                sizeof(r3d_animtree_stm_x_t));
    if(!anode) return NULL;

    anode->stmx->nested = nested;
    return anode;
}

R3D_AnimationStmIndex r3d_amintree_state_create(r3d_animtree_stm_t* node,
                                                R3D_AnimationTreeNode anode,
                                                unsigned int edges_cnt) {
    R3D_AnimationStmIndex next_idx = node->states_cnt;
    if(next_idx >= node->max_states) return -1;

    r3d_stmstate_t* state = &node->state_list[next_idx];
    *state = (r3d_stmstate_t){.out_list  = (edges_cnt > 0 ?
                                            RL_MALLOC(edges_cnt * sizeof(*state->out_list)) :
                                            NULL),
                              .out_cnt   = 0,
                              .max_out   = edges_cnt,
                              .active_in = NULL};
    node->node_list[next_idx] = anode;

    node->states_cnt += 1;
    return next_idx;
}

R3D_AnimationStmIndex r3d_amintree_edge_create(r3d_animtree_stm_t* node,
                                               R3D_AnimationStmIndex beg_idx,
                                               R3D_AnimationStmIndex end_idx,
                                               R3D_StmEdgeParams params) {
    R3D_AnimationStmIndex next_idx = node->edges_cnt;
    if(next_idx >= node->max_edges) return -1;

    r3d_stmedge_t* edge = &node->edge_list[next_idx];
    *edge = (r3d_stmedge_t){.beg_idx    = beg_idx,
                            .end_idx    = end_idx,
                            .end_weight = 0.0f,
                            .params     = params};

    r3d_stmstate_t* beg_state = &node->state_list[beg_idx];
    unsigned int    out_cnt   = beg_state->out_cnt;
    if(out_cnt >= beg_state->max_out) return -1;
    beg_state->out_list[out_cnt] = edge;
    beg_state->out_cnt          += 1;

    node->edges_cnt += 1;
    return next_idx;
}

void r3d_animtree_delete(R3D_AnimationTreeNode anode) {
    switch(anode.base->type) {
    case R3D_ANIMTREE_ANIM:
    case R3D_ANIMTREE_BLEND2:
    case R3D_ANIMTREE_ADD2:
    case R3D_ANIMTREE_STM_X:
        return;
    case R3D_ANIMTREE_SWITCH:
        RL_FREE(anode.swch->in_list);
        RL_FREE(anode.swch->in_weights);
        return;
    case R3D_ANIMTREE_STM:
        for(int i = 0; i < anode.stm->states_cnt; i++)
            if(anode.stm->state_list[i].out_list)
                RL_FREE(anode.stm->state_list[i].out_list);
        RL_FREE(anode.stm->node_list);
        RL_FREE(anode.stm->edge_list);
        RL_FREE(anode.stm->state_list);
        RL_FREE(anode.stm->visit_list);
        if(anode.stm->path.edges) RL_FREE(anode.stm->path.edges);
        if(anode.stm->path.open)  RL_FREE(anode.stm->path.open);
        if(anode.stm->path.next)  RL_FREE(anode.stm->path.next);
        if(anode.stm->path.mark)  RL_FREE(anode.stm->path.mark);
        return;
    default:
        R3D_TRACELOG(LOG_WARNING, "Failed to delete node: invalid type %d",
                     anode.base->type);
    }
}

void r3d_animtree_update(R3D_AnimationTree* atree, float elapsed_time,
                         Transform* root_motion, Transform* root_distance) {
    R3D_AnimationPlayer* p        = &atree->player;
    Matrix*              pose     = p->localPose;
    int                  bone_cnt = p->skeleton.boneCount;
    if(elapsed_time < 0.0f) return;

    bool succ;
    succ = anode_update(atree, *atree->rootNode, elapsed_time, NULL);
    if(succ)
        for(int bone_idx = 0; bone_idx < bone_cnt; bone_idx++) {
            bool      is_rm  = is_root_bone(atree, bone_idx);
            rminfo_t  info;

            Transform out = {0};
            succ = anode_eval(atree, *atree->rootNode, bone_idx, &out,
                              is_rm ? &info : NULL);
            if(succ) {
                if(is_rm) {
                    if(root_motion)   *root_motion   = info.motion;
                    if(root_distance) *root_distance = info.distance;
                    out = transform_subtr(out, info.distance);
                }
                if(atree->updateCallback)
                    atree->updateCallback(p, bone_idx, &out, atree->updateUserData);
                pose[bone_idx] = r3d_matrix_srt_quat(out.scale,
                                                     out.rotation,
                                                     out.translation);
            } else break;
        }

    if(succ)
        compute_model_matrices(p);
    else {
        R3D_TRACELOG(LOG_ERROR, "Animation tree failed");
        memcpy(p->localPose, p->skeleton.localBind, bone_cnt * sizeof(Matrix));
        memcpy(p->modelPose, p->skeleton.modelBind, bone_cnt * sizeof(Matrix));
    }
    R3D_UploadAnimationPlayerPose(p);
}

void r3d_animtree_travel(r3d_animtree_stm_t* node, R3D_AnimationStmIndex target_idx) {
    if(node->active_idx == target_idx) return;

    bool found = stm_find_path(node, target_idx);
    if(!found) {
        r3d_stmstate_t*       state = &node->state_list[target_idx];
        R3D_AnimationTreeNode anode = node->node_list[target_idx];
        state->active_in = NULL;
        node->active_idx = target_idx;
        node->path.len   = 0;
        anode_reset(anode);
    }
}

// EOF
