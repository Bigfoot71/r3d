/* r3d_animation.c -- R3D Animation Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation.h>
#include <raymath.h>
#include <string.h>
#include <glad.h>

#include "./importer/r3d_importer_internal.h"

// ========================================
// PUBLIC API
// ========================================

R3D_AnimationLib R3D_LoadAnimationLib(const char* filePath)
{
    R3D_Importer* importer = R3D_LoadImporter(filePath, 0);
    if (importer == NULL) return (R3D_AnimationLib) {0};

    R3D_AnimationLib animLib = R3D_LoadAnimationLibFromImporter(importer);
    R3D_UnloadImporter(importer);

    return animLib;
}

R3D_AnimationLib R3D_LoadAnimationLibFromMemory(const void* data, unsigned int size, const char* hint)
{
    R3D_Importer* importer = R3D_LoadImporterFromMemory(data, size, hint, 0);
    if (importer == NULL) return (R3D_AnimationLib) {0};

    R3D_AnimationLib animLib = R3D_LoadAnimationLibFromImporter(importer);
    R3D_UnloadImporter(importer);

    return animLib;
}

R3D_AnimationLib R3D_LoadAnimationLibFromImporter(const R3D_Importer* importer)
{
    R3D_AnimationLib animLib = {0};
    r3d_importer_load_animations(importer, &animLib);
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
