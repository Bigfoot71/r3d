/* r3d_importer_texture_cache.c -- Module to load textures from assimp materials.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"
#include "../r3d_image.h"
#include "raylib.h"

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/texture.h>

#include <stdatomic.h>
#include <threads.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#   include <sysinfoapi.h>
#elif defined(__linux__) || defined(__APPLE__)
#   include <unistd.h>
#else
#   error "Oops, platform not supported by R3D"
#endif

// ========================================
// INTERNAL STRUCTURES
// ========================================

typedef struct {
    enum aiTextureMapMode wrap[2];
    Image image;
    bool owned;
} loaded_image_t;

typedef struct ready_job {
    int materialIndex;
    int mapIndex;
    struct ready_job* next;
} ready_job_t;

typedef struct {
    ready_job_t* head;
    ready_job_t* tail;
} job_queue_t;

typedef struct {
    Texture2D textures[R3D_MAP_COUNT];
    bool used;                          // Marked as true if it was obtained via 'r3d_importer_get_texture', if 'false' the textures will be unloaded when the 'r3d_importer_texture_cache_t' is destroyed
} loaded_material_t;

struct r3d_importer_texture_cache {
    loaded_material_t* materials;       // textures[materialCount]
    int materialCount;
};

typedef struct {
    const r3d_importer_t* importer;
    loaded_image_t* images;             // images[materialCount][R3D_MAP_COUNT]
    int materialCount;
    atomic_int nextJob;
    int totalJobs;

    mtx_t queueMutex;
    cnd_t queueCond;
    job_queue_t readyQueue;
} loader_context_t;

// ========================================
// QUEUE HELPERS
// ========================================

static void queue_init(job_queue_t* queue)
{
    queue->head = NULL;
    queue->tail = NULL;
}

static void queue_push(job_queue_t* queue, int materialIndex, int mapIndex)
{
    ready_job_t* job = RL_MALLOC(sizeof(ready_job_t));
    job->materialIndex = materialIndex;
    job->mapIndex = mapIndex;
    job->next = NULL;

    if (queue->tail) {
        queue->tail->next = job;
    } else {
        queue->head = job;
    }
    queue->tail = job;
}

static bool queue_pop(job_queue_t* queue, int* materialIndex, int* mapIndex)
{
    if (!queue->head) {
        return false;
    }

    ready_job_t* job = queue->head;
    *materialIndex = job->materialIndex;
    *mapIndex = job->mapIndex;

    queue->head = job->next;
    if (!queue->head) {
        queue->tail = NULL;
    }

    RL_FREE(job);
    return true;
}

static bool queue_empty(const job_queue_t* queue)
{
    return queue->head == NULL;
}

static void queue_destroy(job_queue_t* queue)
{
    while (queue->head) {
        ready_job_t* job = queue->head;
        queue->head = job->next;
        RL_FREE(job);
    }
    queue->tail = NULL;
}

// ========================================
// TEXTURE WRAP CONVERSION
// ========================================

static int get_wrap_mode(enum aiTextureMapMode wrap)
{
    switch (wrap) {
    case aiTextureMapMode_Wrap:
        return TEXTURE_WRAP_REPEAT;
    case aiTextureMapMode_Mirror:
        return TEXTURE_WRAP_MIRROR_REPEAT;
    case aiTextureMapMode_Clamp:
    case aiTextureMapMode_Decal:
    default:
        return TEXTURE_WRAP_CLAMP;
    }
}

// ========================================
// IMAGE LOADING HELPERS
// ========================================

static bool load_image_base(
    loaded_image_t* image,
    const r3d_importer_t* importer,
    const struct aiMaterial* material,
    enum aiTextureType type,
    unsigned int index)
{
    struct aiString path = {0};
    if (aiGetMaterialTexture(material, type, index, &path, NULL, NULL, NULL, NULL, image->wrap, NULL) != AI_SUCCESS) {
        return false;
    }

    // Embedded texture (starts with '*')
    if (path.data[0] == '*')
    {
        int textureIndex = atoi(&path.data[1]);
        const struct aiTexture* aiTex = r3d_importer_get_texture(importer, textureIndex);

        if (aiTex->mHeight == 0) {
            image->image = LoadImageFromMemory(
                TextFormat(".%s", aiTex->achFormatHint),
                (const unsigned char*)aiTex->pcData,
                aiTex->mWidth
            );
            image->owned = true;
        }
        else {
            // Uncompressed RGBA texture
            image->image.width = aiTex->mWidth;
            image->image.height = aiTex->mHeight;
            image->image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            image->image.mipmaps = 1;
            // No copy needed - will be uploaded immediately
            image->image.data = aiTex->pcData;
            image->owned = false;
        }
    }
    else {
        // External texture file
        image->image = LoadImage(path.data);
        if (image->image.data != NULL) {
            image->owned = true;
        }
    }

    return image->image.data != NULL;
}

static bool load_image_albedo(
    loaded_image_t* image,
    const r3d_importer_t* importer,
    const struct aiMaterial* material)
{
    bool valid = load_image_base(image, importer, material, aiTextureType_BASE_COLOR, 0);
    if (!valid) {
        valid = load_image_base(image, importer, material, aiTextureType_DIFFUSE, 0);
    }
    return valid;
}

static bool load_image_emission(
    loaded_image_t* image,
    const r3d_importer_t* importer,
    const struct aiMaterial* material)
{
    return load_image_base(image, importer, material, aiTextureType_EMISSIVE, 0);
}

static bool load_image_orm(
    loaded_image_t* image,
    const r3d_importer_t* importer,
    const struct aiMaterial* material)
{
    loaded_image_t imOcclusion = {0};
    loaded_image_t imRoughness = {0};
    loaded_image_t imMetalness = {0};

    // Load occlusion map
    bool retOcclusion = load_image_base(&imOcclusion, importer, material, aiTextureType_AMBIENT_OCCLUSION, 0);
    if (!retOcclusion) {
        retOcclusion = load_image_base(&imOcclusion, importer, material, aiTextureType_LIGHTMAP, 0);
    }

    // Load roughness map
    bool retRoughness = load_image_base(&imRoughness, importer, material, aiTextureType_DIFFUSE_ROUGHNESS, 0);
    if (!retRoughness) {
        retRoughness = load_image_base(&imRoughness, importer, material, aiTextureType_SHININESS, 0);
        if (retRoughness) {
            ImageColorInvert(&imRoughness.image);
        }
    }

    // Load metalness map
    bool retMetalness = load_image_base(&imMetalness, importer, material, aiTextureType_METALNESS, 0);
    if (!retMetalness && !retRoughness) {
        // Try GLTF metallic-roughness texture
        retRoughness = load_image_base(&imRoughness, importer, material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE);
        if (retRoughness) {
            retMetalness = retRoughness;
            imMetalness = imRoughness;
        }
    }

    // If no image could be loaded, return
    if (!retOcclusion && !retRoughness && !retMetalness) {
        return false;
    }

    // Compose ORM map
    const Image* sources[3] = {
        retOcclusion ? &imOcclusion.image : NULL,
        retRoughness ? &imRoughness.image : NULL,
        retMetalness ? &imMetalness.image : NULL
    };

    image->image = r3d_compose_images_rgb(sources, WHITE);
    image->owned = true;

    // Set wrap mode from first available texture
    if (retMetalness) {
        image->wrap[0] = imMetalness.wrap[0];
        image->wrap[1] = imMetalness.wrap[1];
    } else if (retRoughness) {
        image->wrap[0] = imRoughness.wrap[0];
        image->wrap[1] = imRoughness.wrap[1];
    } else if (retOcclusion) {
        image->wrap[0] = imOcclusion.wrap[0];
        image->wrap[1] = imOcclusion.wrap[1];
    }

    // Free allocated data
    if (imOcclusion.owned) {
        UnloadImage(imOcclusion.image);
    }
    if (imRoughness.owned) {
        UnloadImage(imRoughness.image);
    }
    if (imMetalness.owned && imMetalness.image.data != imRoughness.image.data) {
        UnloadImage(imMetalness.image);
    }

    return true;
}

static bool load_image_normal(
    loaded_image_t* image,
    const r3d_importer_t* importer,
    const struct aiMaterial* material)
{
    return load_image_base(image, importer, material, aiTextureType_NORMALS, 0);
}

static bool load_image_for_map(
    loaded_image_t* image,
    const r3d_importer_t* importer,
    const struct aiMaterial* material,
    r3d_importer_texture_map_t map)
{
    switch (map) {
    case R3D_MAP_ALBEDO:
        return load_image_albedo(image, importer, material);
    case R3D_MAP_EMISSION:
        return load_image_emission(image, importer, material);
    case R3D_MAP_ORM:
        return load_image_orm(image, importer, material);
    case R3D_MAP_NORMAL:
        return load_image_normal(image, importer, material);
    default:
        return false;
    }
}

// ========================================
// WORKER THREAD
// ========================================

static int worker_thread(void* arg)
{
    loader_context_t* ctx = (loader_context_t*)arg;

    while (true) {
        // Get next job
        int jobIndex = atomic_fetch_add(&ctx->nextJob, 1);
        if (jobIndex >= ctx->totalJobs) {
            break;
        }

        int materialIdx = jobIndex / R3D_MAP_COUNT;
        int mapIdx = jobIndex % R3D_MAP_COUNT;

        // Load the image
        const struct aiMaterial* material = r3d_importer_get_material(ctx->importer, materialIdx);
        loaded_image_t* image = &ctx->images[materialIdx * R3D_MAP_COUNT + mapIdx];

        load_image_for_map(image, ctx->importer, material, (r3d_importer_texture_map_t)mapIdx);

        // Notify that image is ready
        mtx_lock(&ctx->queueMutex);
        queue_push(&ctx->readyQueue, materialIdx, mapIdx);
        cnd_signal(&ctx->queueCond);
        mtx_unlock(&ctx->queueMutex);
    }

    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

r3d_importer_texture_cache_t* r3d_importer_load_texture_cache(const r3d_importer_t* importer, TextureFilter filter)
{
    if (!importer || !r3d_importer_is_valid(importer)) {
        TraceLog(LOG_ERROR, "R3D: Invalid importer for texture loading");
        return NULL;
    }

    // Allocate cache
    r3d_importer_texture_cache_t* cache = RL_MALLOC(sizeof(r3d_importer_texture_cache_t));
    cache->materialCount = r3d_importer_get_material_count(importer);

    // Allocate materials array
    cache->materials = RL_CALLOC(cache->materialCount, sizeof(loaded_material_t));

    // Setup loading context
    loader_context_t ctx = {0};
    ctx.importer = importer;
    ctx.materialCount = cache->materialCount;
    ctx.totalJobs = cache->materialCount * R3D_MAP_COUNT;
    atomic_init(&ctx.nextJob, 0);

    // Allocate image storage
    ctx.images = RL_MALLOC(ctx.totalJobs * sizeof(loaded_image_t));
    memset(ctx.images, 0, ctx.totalJobs * sizeof(loaded_image_t));

    // Initialize synchronization
    mtx_init(&ctx.queueMutex, mtx_plain);
    cnd_init(&ctx.queueCond);
    queue_init(&ctx.readyQueue);

    // Determine thread count
    int numThreads = 4; // Default fallback
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        numThreads = sysinfo.dwNumberOfProcessors;
    #elif defined(__linux__) || defined(__APPLE__)
        numThreads = sysconf(_SC_NPROCESSORS_ONLN);
    #endif

    if (numThreads > ctx.totalJobs) {
        numThreads = ctx.totalJobs;
    }
    if (numThreads < 1) {
        numThreads = 1;
    }

    TraceLog(LOG_INFO, "R3D: Loading textures with %d worker threads", numThreads);

    // Launch worker threads
    thrd_t* threads = RL_MALLOC(numThreads * sizeof(thrd_t));
    for (int i = 0; i < numThreads; i++) {
        thrd_create(&threads[i], worker_thread, &ctx);
    }

    // Progressive upload loop
    int uploadedCount = 0;
    while (uploadedCount < ctx.totalJobs)
    {
        int materialIdx, mapIdx;

        // Wait for ready job
        mtx_lock(&ctx.queueMutex);
        while (queue_empty(&ctx.readyQueue)) {
            cnd_wait(&ctx.queueCond, &ctx.queueMutex);
        }
        queue_pop(&ctx.readyQueue, &materialIdx, &mapIdx);
        mtx_unlock(&ctx.queueMutex);

        // Upload texture
        loaded_image_t* img = &ctx.images[materialIdx * R3D_MAP_COUNT + mapIdx];
        if (img->image.data) {
            Texture2D* texture = &cache->materials[materialIdx].textures[mapIdx];
            *texture = LoadTextureFromImage(img->image);
            if (filter >= TEXTURE_FILTER_TRILINEAR) {
                GenTextureMipmaps(texture);
            }

            SetTextureWrap(*texture, get_wrap_mode(img->wrap[0]));
            SetTextureFilter(*texture, filter);

            if (img->owned) {
                UnloadImage(img->image);
                img->image.data = NULL;
            }
        }

        uploadedCount++;
    }

    // Join threads
    for (int i = 0; i < numThreads; i++) {
        thrd_join(threads[i], NULL);
    }

    // Cleanup
    RL_FREE(threads);
    queue_destroy(&ctx.readyQueue);
    cnd_destroy(&ctx.queueCond);
    mtx_destroy(&ctx.queueMutex);
    RL_FREE(ctx.images);

    TraceLog(LOG_INFO, "R3D: Loaded %d textures successfully", uploadedCount);

    return cache;
}

void r3d_importer_unload_texture_cache(r3d_importer_texture_cache_t* cache)
{
    if (!cache) return;

    for (int i = 0; i < cache->materialCount; i++) {
        if (!cache->materials[i].used) {
            for (int j = 0; j < R3D_MAP_COUNT; j++) {
                UnloadTexture(cache->materials[i].textures[j]);
            }
        }
    }

    RL_FREE(cache->materials);
    RL_FREE(cache);
}

Texture2D* r3d_importer_get_loaded_texture(r3d_importer_texture_cache_t* cache, int materialIndex, r3d_importer_texture_map_t map)
{
    if (!cache || materialIndex < 0 || materialIndex >= cache->materialCount) return NULL;
    if (cache->materials[materialIndex].textures[map].id == 0) return NULL;

    cache->materials[materialIndex].used = true;

    return &cache->materials[materialIndex].textures[map];
}
