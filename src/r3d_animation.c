/* r3d_animation.h -- R3D Animation Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation.h>
#include <raymath.h>
#include <string.h>
#include <glad.h>

#include "./importer/r3d_importer.h"

// ========================================
// PUBLIC API
// ========================================

R3D_AnimationLib R3D_LoadAnimationLib(const char* filePath)
{
    R3D_AnimationLib animLib = {0};

    r3d_importer_t importer = {0};
    if (!r3d_importer_create_from_file(&importer, filePath)) {
        return animLib;
    }

    r3d_importer_load_animations(&importer, &animLib);
    r3d_importer_destroy(&importer);

    return animLib;
}

R3D_AnimationLib R3D_LoadAnimationLibFromMemory(const void* data, unsigned int size, const char* hint)
{
    R3D_AnimationLib animLib = {0};

    r3d_importer_t importer = {0};
    if (!r3d_importer_create_from_memory(&importer, data, size, hint)) {
        return animLib;
    }

    r3d_importer_load_animations(&importer, &animLib);
    r3d_importer_destroy(&importer);

    return animLib;
}

void R3D_UnloadAnimationLib(R3D_AnimationLib animLib)
{
    if (!animLib.animations) return;

    for (int i = 0; i < animLib.count; ++i) {
        R3D_Animation* anim = &animLib.animations[i];

        for (int j = 0; j < anim->channelCount; ++j) {
            R3D_AnimationChannel* channel = &anim->channels[j];

            RL_FREE((void*)channel->translation.times);
            RL_FREE((void*)channel->translation.values);

            RL_FREE((void*)channel->rotation.times);
            RL_FREE((void*)channel->rotation.values);

            RL_FREE((void*)channel->scale.times);
            RL_FREE((void*)channel->scale.values);
        }

        RL_FREE(anim->channels);
    }

    RL_FREE(animLib.animations);
}

int R3D_GetAnimationIndex(R3D_AnimationLib animLib, const char* name)
{
    for (int i = 0; i < animLib.count; i++) {
        if (strcmp(animLib.animations[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

R3D_Animation* R3D_GetAnimation(R3D_AnimationLib animLib, const char* name)
{
    int index = R3D_GetAnimationIndex(animLib, name);
    if (index < 0) return NULL;

    return &animLib.animations[index];
}
