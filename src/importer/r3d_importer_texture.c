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

#ifdef _WIN32
#   define NOGDI
#   define NOUSER
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef near
#   undef far
#endif

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
#include "../common/r3d_hash.h"

// ========================================
// CONSTANTS
// ========================================

#define MAX_PATH_LENGTH 256

// ========================================
// INTERNAL ALIASES
// ========================================

typedef uint64_t texture_key_t;

// ========================================
// INTERNAL STRUCTS
// ========================================

struct r3d_importer_texture_cache {
    Texture2D* textures; // [materialCount][R3D_MAP_COUNT]
    int materialCount;
};

typedef struct {
    char paths[3][MAX_PATH_LENGTH];
    enum aiTextureMapMode wrap[2];
    bool hasRoughMetalORM;
    bool isShininessORM;
    bool isORM;
} texture_job_t;

typedef struct {
    Image image;
    Texture2D texture;
    TextureWrap wrapMode;
    r3d_importer_texture_map_t map;
    bool ownsImageData;
} texture_slot_t;

typedef struct {
    texture_key_t key;
    int slotIndex;
    UT_hash_handle hh;
} texture_entry_t;

typedef struct {
    const r3d_importer_t* importer;
    texture_job_t* jobs;
    texture_slot_t* slots;
    int slotCount;
    atomic_int nextJob;
    atomic_int* readySlots;
    atomic_int writePos;
    atomic_int readPos;
} loader_context_t;

// ========================================
// MEMORY ARENA ALLOCATOR
// ========================================

typedef struct {
    void* base;
    size_t used;
    size_t capacity;
} memory_arena_t;

static inline void* arena_alloc(memory_arena_t* arena, size_t size, size_t align)
{
    size_t padding = (align - (arena->used % align)) % align;
    size_t required = arena->used + padding + size;
    if (required > arena->capacity) return NULL;

    void* ptr = (char*)arena->base + arena->used + padding;
    arena->used = required;
    return ptr;
}

#define ARENA_ALLOC(arena, type, count) \
    arena_alloc(arena, sizeof(type) * (count), _Alignof(type))

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
    case aiTextureMapMode_Wrap: return TEXTURE_WRAP_REPEAT;
    case aiTextureMapMode_Mirror: return TEXTURE_WRAP_MIRROR_REPEAT;
    case aiTextureMapMode_Clamp:
    case aiTextureMapMode_Decal:
    default: return TEXTURE_WRAP_CLAMP;
    }
}

static texture_key_t make_key_texture_job(const texture_job_t* job)
{
    uint64_t hash = R3D_HASH_FNV_OFFSET_BASIS_64;

    for (int i = 0; i < ARRAY_SIZE(job->paths); i++) {
        if (job->paths[i][0] != '\0') {
            hash ^= r3d_hash_fnv1a_64_str(job->paths[i]);
            hash *= R3D_HASH_FNV_PRIME_64;
            hash ^= (uint64_t)i;
        }
        else {
            hash ^= (0xB16B00B500000000ULL | i); // :3
            hash *= R3D_HASH_FNV_PRIME_64;
        }
    }

    hash ^= ((uint64_t)job->wrap[0] << 32) | job->wrap[1];
    hash *= R3D_HASH_FNV_PRIME_64;

    return hash;
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

static bool texture_job_extract_albedo(texture_job_t* job, const struct aiMaterial* material)
{
    if (texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_BASE_COLOR, 0)) {
        return true;
    }
    return texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_DIFFUSE, 0);
}

static bool texture_job_extract_emission(texture_job_t* job, const struct aiMaterial* material)
{
    return texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_EMISSIVE, 0);
}

static bool texture_job_extract_orm(texture_job_t* job, const struct aiMaterial* material)
{
    job->isORM = true;

    // Try to extract occlusion
    bool hasOcclusion = texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_AMBIENT_OCCLUSION, 0);
    if (!hasOcclusion) {
        hasOcclusion = texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_LIGHTMAP, 0);
    }

    // Try to extract PBR Metallic Roughness (glTF)
    // If we have it, we can finish the extraction sooner
    bool hasRoughness = texture_job_extract_data(job->paths[1], job->wrap, material, AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLICROUGHNESS_TEXTURE);
    if (hasRoughness) {
        job->hasRoughMetalORM = true;
        return true;
    }

    // Try to extract roughness or shininess
    hasRoughness = texture_job_extract_data(job->paths[1], job->wrap, material, aiTextureType_DIFFUSE_ROUGHNESS, 0);
    if (!hasRoughness) {
        hasRoughness = texture_job_extract_data(job->paths[1], job->wrap, material, aiTextureType_SHININESS, 0);
        if (hasRoughness) job->isShininessORM = true;
    }

    // Try to extract metalness
    bool hasMetalness = texture_job_extract_data(job->paths[2], job->wrap, material, aiTextureType_METALNESS, 0);

    // Success if we have at least one texture
    return (hasOcclusion || hasRoughness || hasMetalness);
}

static bool texture_job_extract_normal(texture_job_t* job, const struct aiMaterial* material)
{
    return texture_job_extract_data(job->paths[0], job->wrap, material, aiTextureType_NORMALS, 0);
}

static bool texture_job_init(texture_job_t* job, const struct aiMaterial* material, r3d_importer_texture_map_t mapIdx)
{
    switch (mapIdx) {
    case R3D_MAP_ALBEDO:   return texture_job_extract_albedo(job, material);
    case R3D_MAP_EMISSION: return texture_job_extract_emission(job, material);
    case R3D_MAP_ORM:      return texture_job_extract_orm(job, material);
    case R3D_MAP_NORMAL:   return texture_job_extract_normal(job, material);
    default:               assert(false); break;
    }
    return false;
}

// ========================================
// IMAGE LOADING
// ========================================

static bool load_image_base(Image* outImage, bool* outOwned, const r3d_importer_t* importer, const char* path)
{
    if (path[0] == '*') {
        int textureIndex = atoi(&path[1]);
        const struct aiTexture* aiTex = r3d_importer_get_texture(importer, textureIndex);

        if (aiTex->mHeight == 0) {
            *outImage = LoadImageFromMemory(
                TextFormat(".%s", aiTex->achFormatHint),
                (const unsigned char*)aiTex->pcData,
                aiTex->mWidth
            );
            *outOwned = (outImage->data != NULL);
        }
        else {
            outImage->width = aiTex->mWidth;
            outImage->height = aiTex->mHeight;
            outImage->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            outImage->mipmaps = 1;
            outImage->data = aiTex->pcData;
            *outOwned = false;
        }
    }
    else {
        *outImage = LoadImage(path);
        *outOwned = (outImage->data != NULL);
    }

    return outImage->data != NULL;
}

static bool load_image_simple(texture_slot_t* slot, const r3d_importer_t* importer, const texture_job_t* job)
{
    slot->wrapMode = get_wrap_mode(job->wrap[0]);
    bool success = load_image_base(&slot->image, &slot->ownsImageData, importer, job->paths[0]);
    if (!success) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load texture: %s", job->paths[0]);
    }
    return success;
}

static bool load_image_orm(texture_slot_t* slot, const r3d_importer_t* importer, const texture_job_t* job)
{
#   define ROUGHNESS_IDX 1

    Image sources[3] = {0};
    bool owned[3] = {0};

    // Load individual components
    for (int i = 0; i < 3; i++) {
        if (job->paths[i][0] != '\0') {
            if (!load_image_base(&sources[i], &owned[i], importer, job->paths[i])) {
                R3D_TRACELOG(LOG_WARNING, "Failed to load ORM component %d: %s", i, job->paths[i]);
            }
            if (i == ROUGHNESS_IDX && job->isShininessORM && sources[i].data) {
                ImageColorInvert(&sources[i]);
            }
        }
    }

    // Compose ORM
    const Image* srcPtrs[3] = {
        sources[0].data ? &sources[0] : NULL,
        sources[1].data ? &sources[1] : NULL,
        job->hasRoughMetalORM ?
            (sources[1].data ? &sources[1] : NULL) :
            (sources[2].data ? &sources[2] : NULL)
    };

    slot->image = r3d_image_compose_rgb(srcPtrs, WHITE);
    slot->wrapMode = get_wrap_mode(job->wrap[0]);
    slot->ownsImageData = true;

    // Free sources
    for (int i = 0; i < 3; i++) {
        if (owned[i] && sources[i].data) {
            UnloadImage(sources[i]);
        }
    }

    bool success = (slot->image.data != NULL);
    if (!success) {
        R3D_TRACELOG(LOG_WARNING, "Failed to compose ORM texture");
    }

    return success;
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
        if (jobIdx >= ctx->slotCount) break;

        texture_slot_t* slot = &ctx->slots[jobIdx];
        const texture_job_t* job = &ctx->jobs[jobIdx];

        // Load image
        if (job->isORM) load_image_orm(slot, ctx->importer, job);
        else load_image_simple(slot, ctx->importer, job);

        // Add to upload queue (atomic reserve + store)
        int pos = atomic_fetch_add_explicit(&ctx->writePos, 1, memory_order_relaxed);
        atomic_store_explicit(&ctx->readySlots[pos], jobIdx, memory_order_release);
    }

    return 0;
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
    int maxSlots = materialCount * R3D_MAP_COUNT;

    /* --- Phase 0: Allocate a large block of memory for work --- */

    size_t arenaSize = 
        sizeof(texture_job_t) * maxSlots +      // Jobs
        sizeof(texture_slot_t) * maxSlots +     // Slots
        sizeof(texture_entry_t) * maxSlots +    // Entries
        sizeof(int) * maxSlots +                // Material mapping
        sizeof(thrd_t) * r3d_get_cpu_count() +  // Thread handles
        sizeof(atomic_int) * maxSlots +         // Ready slots queue
        1024;                                   // Padding for alignment

    void* arenaMemory = RL_CALLOC(arenaSize, 1);
    memory_arena_t arena = {.base = arenaMemory, .used = 0, .capacity = arenaSize};

    /* --- Phase 1: Collect unique textures --- */

    texture_job_t* jobs = ARENA_ALLOC(&arena, texture_job_t, maxSlots);
    texture_slot_t* slots = ARENA_ALLOC(&arena, texture_slot_t, maxSlots);
    texture_entry_t* entries = ARENA_ALLOC(&arena, texture_entry_t, maxSlots);

    int* materialToSlot = ARENA_ALLOC(&arena, int, maxSlots);
    for (int i = 0; i < maxSlots; i++) materialToSlot[i] = -1;

    texture_entry_t* hashTable = NULL;
    int entryPoolUsed = 0;
    int uniqueCount = 0;

    for (int matIdx = 0; matIdx < materialCount; matIdx++)
    {
        const struct aiMaterial* material = r3d_importer_get_material(importer, matIdx);

        for (int mapIdx = 0; mapIdx < R3D_MAP_COUNT; mapIdx++)
        {
            texture_job_t job = {0};
            if (!texture_job_init(&job, material, mapIdx)) continue;

            texture_entry_t* entry = NULL;
            texture_key_t key = make_key_texture_job(&job);
            HASH_FIND(hh, hashTable, &key, sizeof(key), entry);

            if (entry) {
                // Reuse existing slot
                materialToSlot[matIdx * R3D_MAP_COUNT + mapIdx] = entry->slotIndex;
                continue;
            }

            // New unique texture
            entry = &entries[entryPoolUsed++];
            entry->key = key;
            entry->slotIndex = uniqueCount;
            HASH_ADD(hh, hashTable, key, sizeof(key), entry);

            jobs[uniqueCount] = job;
            slots[uniqueCount].map = mapIdx;

            materialToSlot[matIdx * R3D_MAP_COUNT + mapIdx] = uniqueCount;
            uniqueCount++;
        }
    }

    /* --- Phase 2: Parallel loading to RAM --- */

    loader_context_t ctx = {
        .importer = importer,
        .jobs = jobs,
        .slots = slots,
        .slotCount = uniqueCount,
        .readySlots = ARENA_ALLOC(&arena, atomic_int, uniqueCount)
    };

    atomic_init(&ctx.nextJob, 0);
    atomic_init(&ctx.writePos, 0);
    atomic_init(&ctx.readPos, 0);

    int numThreads = r3d_get_cpu_count();
    if (numThreads > uniqueCount) numThreads = uniqueCount;

    thrd_t* threads = ARENA_ALLOC(&arena, thrd_t, numThreads);
    for (int i = 0; i < numThreads; i++) {
        thrd_create(&threads[i], worker_thread, &ctx);
    }

    /* --- Phase 3: Progressive upload to VRAM --- */

    int processedCount = 0;
    int uploadedCount = 0;

    while (processedCount < uniqueCount)
    {
        int readPos = atomic_load_explicit(&ctx.readPos, memory_order_acquire);
        int writePos = atomic_load_explicit(&ctx.writePos, memory_order_acquire);

        if (writePos == readPos) {
            struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000}; // 1ms
            thrd_sleep(&ts, NULL);
            continue;
        }

        atomic_thread_fence(memory_order_acquire);

        for (int i = readPos; i < writePos; i++) {
            int slotIdx = ctx.readySlots[i];
            texture_slot_t* slot = &slots[slotIdx];
            if (slot->image.data) {
                slot->texture = r3d_image_upload(&slot->image, slot->wrapMode, filter, is_srgb(slot->map, colorSpace));
                if (slot->ownsImageData) {
                    UnloadImage(slot->image);
                    slot->image.data = NULL;
                }
                uploadedCount++;
            }
            processedCount++;
        }

        atomic_store_explicit(&ctx.readPos, writePos, memory_order_release);
    }

    for (int i = 0; i < numThreads; i++) {
        thrd_join(threads[i], NULL);
    }

    /* --- Phase 4: Build final cache --- */

    Texture2D* finalTextures = RL_CALLOC(materialCount * R3D_MAP_COUNT, sizeof(Texture2D));
    for (int i = 0; i < maxSlots; i++) {
        int slotIdx = materialToSlot[i];
        if (slotIdx >= 0) finalTextures[i] = slots[slotIdx].texture;
    }

    r3d_importer_texture_cache_t* cache = RL_MALLOC(sizeof(*cache));
    cache->materialCount = materialCount;
    cache->textures = finalTextures;

    /* --- Cleanup --- */

    texture_entry_t* entry, *tmp;
    HASH_ITER(hh, hashTable, entry, tmp) {
        HASH_DEL(hashTable, entry);
    }
    RL_FREE(arenaMemory);

    R3D_TRACELOG(LOG_INFO, "Model textures loaded: %d / %d", uploadedCount, processedCount);

    return cache;
}

void r3d_importer_unload_texture_cache(r3d_importer_texture_cache_t* cache, bool unloadTextures)
{
    if (!cache) return;

    if (unloadTextures) {
        int textureCount = cache->materialCount * R3D_MAP_COUNT;
        for (int i = 0; i < textureCount; i++) {
            if (cache->textures[i].id != 0) {
                UnloadTexture(cache->textures[i]);
            }
        }
    }

    RL_FREE(cache->textures);
    RL_FREE(cache);
}

Texture2D* r3d_importer_get_loaded_texture(r3d_importer_texture_cache_t* cache, int materialIndex, r3d_importer_texture_map_t map)
{
    if (!cache || materialIndex < 0 || materialIndex >= cache->materialCount) return NULL;
    Texture2D* tex = &cache->textures[materialIndex * R3D_MAP_COUNT + map];
    return (tex->id != 0) ? tex : NULL;
}
