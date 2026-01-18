/* r3d_importer_texture_cache.c -- Module to load textures from assimp materials.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"
#include <r3d_config.h>

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/texture.h>

#include <tinycthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <uthash.h>
#include <assert.h>
#include <stdio.h>
#include <rlgl.h>
#include <glad.h>

#include "../common/r3d_helper.h"
#include "../common/r3d_image.h"

// ========================================
// CONSTANTS
// ========================================

#define MAX_PATH_LENGTH 256
#define MAX_KEY_LENGTH 768

// ========================================
// INTERNAL ENUMS
// ========================================

typedef enum {
    MAP_ORM_OCCLUSION,
    MAP_ORM_ROUGHNESS,
    MAP_ORM_METALNESS,
    MAP_ORM_COUNT
} map_orm_enum_t;

typedef enum {
    TEXTURE_JOB_SIMPLE,
    TEXTURE_JOB_ORM
} texture_job_enum_t;

// ========================================
// INTERNAL STRUCTS
// ========================================

typedef struct {
    enum aiTextureMapMode wrap[2];
    Image image;
    bool owned;
} loaded_image_t;

typedef struct {
    Texture2D textures[R3D_MAP_COUNT];
    bool used;
} loaded_material_t;

typedef struct {
    char path[MAX_PATH_LENGTH];
    enum aiTextureMapMode wrap[2];
} texture_job_data_simple_t;

typedef struct {
    char paths[MAP_ORM_COUNT][MAX_PATH_LENGTH];
    enum aiTextureMapMode wrap[2];
    bool invertRoughness;
} texture_job_data_orm_t;

typedef struct {
    r3d_importer_texture_map_t map;
    texture_job_enum_t type;
    union {
        texture_job_data_simple_t simple;
        texture_job_data_orm_t orm;
    } data;
} texture_job_t;

typedef struct {
    char key[MAX_KEY_LENGTH];
    int uniqueIndex;
    UT_hash_handle hh;
} texture_hash_entry_t;

struct r3d_importer_texture_cache {
    loaded_material_t* materials;
    int materialCount;
};

typedef struct {

    const r3d_importer_t* importer;

    // Unique textures to load
    texture_job_t* jobs;
    int jobCount;

    // Loaded images (one per unique texture)
    loaded_image_t* images;

    // Ring buffer for ready jobs
    int* readyJobs;
    int readyCapacity;
    atomic_int readyHead;
    atomic_int readyTail;
    atomic_int completedJobs;
    atomic_int nextJob;

} loader_context_t;

// ========================================
// KEY GENERATION FOR HASHING
// ========================================

static inline void make_key_texture_job_simple(char* key, const texture_job_data_simple_t* jobData)
{
    snprintf(key, MAX_KEY_LENGTH, "S|%s|%d|%d", jobData->path, jobData->wrap[0], jobData->wrap[1]);
}

static inline void make_key_texture_job_orm(char* key, const texture_job_data_orm_t* jobData)
{
    // Create a composite key from all 3 paths + wrap modes
    char pathPart[MAX_PATH_LENGTH] = {0};

    for (int i = 0; i < 3; i++) {
        if (jobData->paths[i][0] != '\0') {
            strncat(pathPart, jobData->paths[i], sizeof(pathPart) - strlen(pathPart) - 1);
            strncat(pathPart, "|", sizeof(pathPart) - strlen(pathPart) - 1);
        }
        else {
            strncat(pathPart, "NULL|", sizeof(pathPart) - strlen(pathPart) - 1);
        }
    }

    snprintf(
        key, MAX_KEY_LENGTH, "ORM|%s%d|%d|%d", pathPart, 
        (int)jobData->wrap[0], (int)jobData->wrap[1], (int)jobData->invertRoughness
    );
}

static void make_key_texture_job(char* key, const texture_job_t* job)
{
    switch (job->type) {
    case TEXTURE_JOB_SIMPLE:
        make_key_texture_job_simple(key, &job->data.simple);
        break;
    case TEXTURE_JOB_ORM:
        make_key_texture_job_orm(key, &job->data.orm);
        break;
    }
}

// ========================================
// RING BUFFER HELPERS
// ========================================

static inline void ring_push(loader_context_t* ctx, int jobIndex)
{
    int head = atomic_load_explicit(&ctx->readyHead, memory_order_relaxed);
    ctx->readyJobs[head % ctx->readyCapacity] = jobIndex;

    atomic_store_explicit(&ctx->readyHead, head + 1, memory_order_release);
}

static inline bool ring_pop(loader_context_t* ctx, int* jobIndex)
{
    int tail = atomic_load_explicit(&ctx->readyTail, memory_order_relaxed);
    int head = atomic_load_explicit(&ctx->readyHead, memory_order_acquire);

    if (tail >= head) return false;

    *jobIndex = ctx->readyJobs[tail % ctx->readyCapacity];
    atomic_store_explicit(&ctx->readyTail, tail + 1, memory_order_relaxed);

    return true;
}

// ========================================
// IMAGE LOADING
// ========================================

static bool load_image_base(
    loaded_image_t* image, const r3d_importer_t* importer, const char* path,
    enum aiTextureMapMode wrapU, enum aiTextureMapMode wrapV)
{
    image->wrap[0] = wrapU;
    image->wrap[1] = wrapV;

    if (path[0] == '*') {
        int textureIndex = atoi(&path[1]);
        const struct aiTexture* aiTex = r3d_importer_get_texture(importer, textureIndex);

        if (aiTex->mHeight == 0) {
            // Compressed texture - must decode
            image->image = LoadImageFromMemory(
                TextFormat(".%s", aiTex->achFormatHint),
                (const unsigned char*)aiTex->pcData,
                aiTex->mWidth
            );
            if (image->image.data != NULL) {
                image->owned = true;
            }
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
        image->image = LoadImage(path);
        if (image->image.data != NULL) {
            image->owned = true;
        }
    }

    return image->image.data != NULL;
}

static bool load_image_simple(loaded_image_t* image, const r3d_importer_t* importer, const texture_job_data_simple_t* job)
{
    return load_image_base(image, importer, job->path, job->wrap[0], job->wrap[1]);
}

static bool load_image_orm(loaded_image_t* image, const r3d_importer_t* importer, const texture_job_data_orm_t* job)
{
    loaded_image_t sources[MAP_ORM_COUNT] = {0};

    // Load individual components
    for (int i = 0; i < MAP_ORM_COUNT; i++) {
        if (job->paths[i][0] != '\0') {
            load_image_base(&sources[i], importer, job->paths[i], job->wrap[0], job->wrap[1]);
            if (i == MAP_ORM_ROUGHNESS && job->invertRoughness && sources[i].image.data) {
                ImageColorInvert(&sources[i].image);
            }
        }
    }

    // Compose ORM
    const Image* srcPtrs[3] = {
        sources[0].image.data ? &sources[0].image : NULL,
        sources[1].image.data ? &sources[1].image : NULL,
        sources[2].image.data ? &sources[2].image : NULL
    };

    image->image = r3d_image_compose_rgb(srcPtrs, WHITE);
    image->owned = true;
    image->wrap[0] = job->wrap[0];
    image->wrap[1] = job->wrap[1];

    // Free sources
    for (int i = 0; i < 3; i++) {
        if (sources[i].owned && sources[i].image.data) {
            UnloadImage(sources[i].image);
        }
    }

    return image->image.data != NULL;
}

// ========================================
// HELPER FUNCTIONS
// ========================================

static inline bool is_srgb(r3d_importer_texture_map_t map, R3D_ColorSpace space)
{
    return (space == R3D_COLORSPACE_SRGB && (map == R3D_MAP_ALBEDO || map == R3D_MAP_EMISSION));
}

static inline TextureWrap get_wrap_mode(enum aiTextureMapMode wrap)
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
// WORKER THREAD
// ========================================

static int worker_thread(void* arg)
{
    loader_context_t* ctx = (loader_context_t*)arg;

    while (true)
    {
        int jobIdx = atomic_fetch_add_explicit(&ctx->nextJob, 1, memory_order_relaxed);
        if (jobIdx >= ctx->jobCount) break;

        texture_job_t* job = &ctx->jobs[jobIdx];
        loaded_image_t* img = &ctx->images[jobIdx];

        if (job->type == TEXTURE_JOB_SIMPLE) {
            load_image_simple(img, ctx->importer, &job->data.simple);
        }
        else {
            load_image_orm(img, ctx->importer, &job->data.orm);
        }

        ring_push(ctx, jobIdx);
        atomic_fetch_add_explicit(&ctx->completedJobs, 1, memory_order_release);
    }

    return 0;
}

// ========================================
// DESCRIPTOR EXTRACTION
// ========================================

static bool texture_job_extract_data(
    char* outPath, enum aiTextureMapMode* outWrap,
    const struct aiMaterial* material,
    enum aiTextureType type,
    unsigned int index)
{
    struct aiString path = {0};
    if (aiGetMaterialTexture(material, type, index, &path, NULL, NULL, NULL, NULL, outWrap, NULL) != AI_SUCCESS) {
        outPath[0] = '\0';
        return false;
    }

    strncpy(outPath, path.data, MAX_PATH_LENGTH - 1);
    outPath[MAX_PATH_LENGTH - 1] = '\0';

    return true;
}

static bool texture_job_extract_albedo(texture_job_data_simple_t* job, const struct aiMaterial* material)
{
    if (texture_job_extract_data(job->path, job->wrap, material, aiTextureType_BASE_COLOR, 0)) {
        return true;
    }
    return texture_job_extract_data(job->path, job->wrap, material, aiTextureType_DIFFUSE, 0);
}

static bool texture_job_extract_emission(texture_job_data_simple_t* job, const struct aiMaterial* material)
{
    return texture_job_extract_data(job->path, job->wrap, material, aiTextureType_EMISSIVE, 0);
}

static bool texture_job_extract_orm(texture_job_data_orm_t* job, const struct aiMaterial* material)
{
    // Occlusion
    bool hasOcclusion = texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_AMBIENT_OCCLUSION, 0);
    if (!hasOcclusion) {
        hasOcclusion = texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_LIGHTMAP, 0);
    }

    // Roughness
    bool hasRoughness = texture_job_extract_data(job->paths[1], job->wrap, material, aiTextureType_DIFFUSE_ROUGHNESS, 0);
    if (!hasRoughness) {
        enum aiTextureMapMode tmpWrap[2];
        hasRoughness = texture_job_extract_data(job->paths[1], tmpWrap, material, aiTextureType_SHININESS, 0);
        if (hasRoughness) {
            job->invertRoughness = true;
            if (!hasOcclusion) {
                job->wrap[0] = tmpWrap[0];
                job->wrap[1] = tmpWrap[1];
            }
        }
    }

    // Metalness
    bool hasMetalness = texture_job_extract_data(job->paths[2], job->wrap, material, aiTextureType_METALNESS, 0);
    if (!hasMetalness && !hasRoughness) {
        // GLTF metallic-roughness combined texture
        enum aiTextureMapMode tmpWrap[2];
        hasRoughness = texture_job_extract_data(job->paths[1], tmpWrap, material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE);
        if (hasRoughness) {
            hasMetalness = true;
            strncpy(job->paths[2], job->paths[1], MAX_PATH_LENGTH);
            if (!hasOcclusion) {
                job->wrap[0] = tmpWrap[0];
                job->wrap[1] = tmpWrap[1];
            }
        }
    }

    return (hasOcclusion || hasRoughness || hasMetalness);
}

static bool texture_job_extract_normal(texture_job_data_simple_t* job, const struct aiMaterial* material)
{
    return texture_job_extract_data(job->path, job->wrap, material, aiTextureType_NORMALS, 0);
}

static bool texture_job_init(texture_job_t* job, const struct aiMaterial* material, r3d_importer_texture_map_t mapIdx)
{
    job->map = mapIdx;

    switch (mapIdx) {
    case R3D_MAP_ALBEDO:
        job->type = TEXTURE_JOB_SIMPLE;
        return texture_job_extract_albedo(&job->data.simple, material);
    case R3D_MAP_EMISSION:
        job->type = TEXTURE_JOB_SIMPLE;
        return texture_job_extract_emission(&job->data.simple, material);
    case R3D_MAP_ORM:
        job->type = TEXTURE_JOB_ORM;
        return texture_job_extract_orm(&job->data.orm, material);
    case R3D_MAP_NORMAL:
        job->type = TEXTURE_JOB_SIMPLE;
        return texture_job_extract_normal(&job->data.simple, material);
    default:
        assert(false);
        break;
    }

    return false;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

r3d_importer_texture_cache_t* r3d_importer_load_texture_cache(
    const r3d_importer_t* importer, 
    R3D_ColorSpace colorSpace, 
    TextureFilter filter)
{
    if (!importer || !r3d_importer_is_valid(importer)) {
        R3D_TRACELOG(LOG_ERROR, "Invalid importer for texture loading");
        return NULL;
    }

    int materialCount = r3d_importer_get_material_count(importer);
    int mapCount = materialCount * R3D_MAP_COUNT;

    /* --- Phase 1: Collect unique textures from all materials --- */

    texture_hash_entry_t* hashTable = NULL;
    texture_job_t* jobs = RL_MALLOC(mapCount * sizeof(texture_job_t));
    int* materialToTexture = RL_MALLOC(mapCount * sizeof(int));

    for (int i = 0; i < mapCount; i++) {
        materialToTexture[i] = -1;
    }

    int totalDescriptors = 0, uniqueCount = 0;
    for (int matIdx = 0; matIdx < materialCount; matIdx++)
    {
        const struct aiMaterial* material = r3d_importer_get_material(importer, matIdx);

        for (int mapIdx = 0; mapIdx < R3D_MAP_COUNT; mapIdx++)
        {
            texture_job_t job = {0};
            if (!texture_job_init(&job, material, mapIdx)) continue;
            totalDescriptors++;

            char key[MAX_KEY_LENGTH] = {0};
            make_key_texture_job(key, &job);

            texture_hash_entry_t* entry = NULL;
            HASH_FIND_STR(hashTable, key, entry);
            if (entry) {
                materialToTexture[matIdx * R3D_MAP_COUNT + mapIdx] = entry->uniqueIndex;
                continue;
            }

            entry = RL_MALLOC(sizeof(texture_hash_entry_t));
            strncpy(entry->key, key, sizeof(entry->key) - 1);
            entry->uniqueIndex = uniqueCount;
            HASH_ADD_STR(hashTable, key, entry);
            jobs[uniqueCount] = job;

            materialToTexture[matIdx * R3D_MAP_COUNT + mapIdx] = uniqueCount;
            uniqueCount++;
        }
    }

    /* --- Phase 2: Setup loading context --- */

    loader_context_t ctx = {0};
    ctx.importer = importer;
    ctx.jobs = jobs;
    ctx.jobCount = uniqueCount;
    ctx.images = RL_CALLOC(uniqueCount, sizeof(loaded_image_t));
    atomic_init(&ctx.nextJob, 0);
    atomic_init(&ctx.completedJobs, 0);

    ctx.readyCapacity = uniqueCount;
    ctx.readyJobs = RL_MALLOC(ctx.readyCapacity * sizeof(int));
    atomic_init(&ctx.readyHead, 0);
    atomic_init(&ctx.readyTail, 0);

    /* --- Phase 3: Load all images to RAM in parallel --- */

    int numThreads = r3d_get_cpu_count();
    if (numThreads > uniqueCount) numThreads = uniqueCount;

    R3D_TRACELOG(LOG_INFO, "Loading %d unique textures with %d threads", uniqueCount, numThreads);

    thrd_t* threads = RL_MALLOC(numThreads * sizeof(thrd_t));
    for (int i = 0; i < numThreads; i++) {
        thrd_create(&threads[i], worker_thread, &ctx);
    }

    /* --- Phase 4: Upload progressively --- */

    Texture2D* uploadedTextures = RL_CALLOC(uniqueCount, sizeof(Texture2D));
    int processedCount = 0;
    int uploadedCount = 0;

    while (processedCount < uniqueCount)
    {
        int jobIdx;

        if (ring_pop(&ctx, &jobIdx)) {
            loaded_image_t* img = &ctx.images[jobIdx];

            if (img->image.data != NULL) {
                uploadedTextures[jobIdx] = r3d_image_upload(
                    &img->image, get_wrap_mode(img->wrap[0]), filter, 
                    is_srgb(ctx.jobs[jobIdx].map, colorSpace)
                );

                if (img->owned) {
                    UnloadImage(img->image);
                    img->image.data = NULL;
                }

                uploadedCount++;
            }

            processedCount++;
        }
        else {
            int completed = atomic_load_explicit(&ctx.completedJobs, memory_order_acquire);
            if (completed >= uniqueCount) continue;
            thrd_yield();
        }
    }

    for (int i = 0; i < numThreads; i++) {
        thrd_join(threads[i], NULL);
    }

    /* --- Phase 5: Build cache --- */

    r3d_importer_texture_cache_t* cache = RL_MALLOC(sizeof(r3d_importer_texture_cache_t));
    cache->materialCount = materialCount;
    cache->materials = RL_CALLOC(materialCount, sizeof(loaded_material_t));

    for (int matIdx = 0; matIdx < materialCount; matIdx++) {
        for (int mapIdx = 0; mapIdx < R3D_MAP_COUNT; mapIdx++) {
            int uniqueIdx = materialToTexture[matIdx * R3D_MAP_COUNT + mapIdx];
            if (uniqueIdx >= 0) {
                cache->materials[matIdx].textures[mapIdx] = uploadedTextures[uniqueIdx];
            }
        }
    }

    /* --- Cleanup --- */

    RL_FREE(materialToTexture);
    RL_FREE(uploadedTextures);
    RL_FREE(ctx.readyJobs);
    RL_FREE(ctx.images);
    RL_FREE(threads);
    RL_FREE(jobs);

    texture_hash_entry_t* entry, *tmp;
    HASH_ITER(hh, hashTable, entry, tmp) {
        HASH_DEL(hashTable, entry);
        RL_FREE(entry);
    }

    R3D_TRACELOG(LOG_INFO, "Texture cache created: %d unique textures uploaded for %d materials", uploadedCount, materialCount);

    return cache;
}

void r3d_importer_unload_texture_cache(r3d_importer_texture_cache_t* cache)
{
    if (!cache) return;

    for (int i = 0; i < cache->materialCount; i++) {
        if (!cache->materials[i].used) {
            for (int j = 0; j < R3D_MAP_COUNT; j++) {
                if (cache->materials[i].textures[j].id != 0) {
                    UnloadTexture(cache->materials[i].textures[j]);
                }
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
