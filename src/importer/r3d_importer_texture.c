/* r3d_importer_texture_cache.c -- Module to load textures from assimp materials.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"

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
#elif defined(__linux__) || defined(__APPLE__)
#   include <unistd.h>
#else
#   error "Oops, platform not supported by R3D"
#endif

#include <tinycthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <rlgl.h>
#include <glad.h>

#include "../details/r3d_image.h"

// ========================================
// INTERNAL STRUCTURES
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

struct r3d_importer_texture_cache {
    loaded_material_t* materials;
    int materialCount;
};

typedef struct {
    const r3d_importer_t* importer;
    loaded_image_t* images;             // Flat array: images[materialCount * R3D_MAP_COUNT]
    int materialCount;
    atomic_int nextJob;
    int totalJobs;

    // Ring buffer for ready jobs
    int* readyJobs;                     // Array of job indices
    int readyCapacity;
    atomic_int readyHead;               // Write position
    atomic_int readyTail;               // Read position
    atomic_bool allJobsSubmitted;
} loader_context_t;

// ========================================
// GLOBAL CACHE
// ========================================

static int g_numCPUs = 0;

static int get_cpu_count(void)
{
    if (g_numCPUs > 0) return g_numCPUs;

    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        g_numCPUs = sysinfo.dwNumberOfProcessors;
    #elif defined(__linux__) || defined(__APPLE__)
        g_numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    #endif

    if (g_numCPUs < 1) {
        TraceLog(LOG_WARNING, "R3D: Failed to detect CPU count, defaulting to 1 thread");
        g_numCPUs = 1;
    }

    return g_numCPUs;
}

// ========================================
// RING BUFFER HELPERS
// ========================================

static void ring_push(loader_context_t* ctx, int jobIndex)
{
    int head = atomic_load(&ctx->readyHead);
    ctx->readyJobs[head % ctx->readyCapacity] = jobIndex;
    atomic_store(&ctx->readyHead, head + 1);
}

static bool ring_pop(loader_context_t* ctx, int* jobIndex)
{
    int tail = atomic_load(&ctx->readyTail);
    int head = atomic_load(&ctx->readyHead);
    
    if (tail >= head) {
        return false;
    }
    
    *jobIndex = ctx->readyJobs[tail % ctx->readyCapacity];
    atomic_store(&ctx->readyTail, tail + 1);
    return true;
}

// ========================================
// TEXTURE LOADING FUNCTIONS
// ========================================

static bool has_gl_ext(const char* name)
{
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    for (GLint i = 0; i < n; i++) {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (strcmp(ext, name) == 0) return true;
    }

    return false;
}

static bool has_gl_anisotropy(float* max)
{
    static bool hasAniso = false;
    static float maxAniso = 1.0f;

    if (!hasAniso) {
        hasAniso = has_gl_ext("GL_EXT_texture_filter_anisotropic");
        if (hasAniso) {
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fminf(8.0f, maxAniso));
        }
    }

    if (max) *max = maxAniso;

    return hasAniso;
}

void get_gl_texture_format(int format, bool srgb, GLenum* glInternalFormat, GLenum* glFormat, GLenum* glType)
{
    // TODO: Add checks for support of compressed formats (consider WebGL 2 for later)

    *glInternalFormat = 0;
    *glFormat = 0;
    *glType = 0;

    switch (format) {
    case RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: *glInternalFormat = GL_R8; *glFormat = GL_RED; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: *glInternalFormat = GL_RG8; *glFormat = GL_RG; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R5G6B5: *glInternalFormat = GL_RGB565; *glFormat = GL_RGB; *glType = GL_UNSIGNED_SHORT_5_6_5; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8: *glInternalFormat = GL_RGB8; *glFormat = GL_RGB; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R5G5B5A1: *glInternalFormat = GL_RGB5_A1; *glFormat = GL_RGBA; *glType = GL_UNSIGNED_SHORT_5_5_5_1; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R4G4B4A4: *glInternalFormat = GL_RGBA4; *glFormat = GL_RGBA; *glType = GL_UNSIGNED_SHORT_4_4_4_4; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: *glInternalFormat = GL_RGBA8; *glFormat = GL_RGBA; *glType = GL_UNSIGNED_BYTE; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R32: *glInternalFormat = GL_R32F; *glFormat = GL_RED; *glType = GL_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32: *glInternalFormat = GL_RGB32F; *glFormat = GL_RGB; *glType = GL_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: *glInternalFormat = GL_RGBA32F; *glFormat = GL_RGBA; *glType = GL_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R16: *glInternalFormat = GL_R16F; *glFormat = GL_RED; *glType = GL_HALF_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16: *glInternalFormat = GL_RGB16F; *glFormat = GL_RGB; *glType = GL_HALF_FLOAT; break;
    case RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16: *glInternalFormat = GL_RGBA16F; *glFormat = GL_RGBA; *glType = GL_HALF_FLOAT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT1_RGB: *glInternalFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT1_RGBA: *glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT3_RGBA: *glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; break;
    case RL_PIXELFORMAT_COMPRESSED_DXT5_RGBA: *glInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; break;
    default: TRACELOG(RL_LOG_WARNING, "R3D: Current format not supported (%i)", format); break;
    }

    if (srgb) {
        switch (*glInternalFormat) {
        case GL_RGBA8: *glInternalFormat = GL_SRGB8_ALPHA8; break;
        case GL_RGB8: *glInternalFormat = GL_SRGB8; break;
        }
    }
}

static void upload_mipmap(const uint8_t *data, int width, int height, int level, 
                          int format, bool srgb)
{
    GLenum glInternalFormat, glFormat, glType;
    get_gl_texture_format(format, srgb, &glInternalFormat, &glFormat, &glType);
    
    if (glInternalFormat == 0) return;
    
    if (format < RL_PIXELFORMAT_COMPRESSED_DXT1_RGB) {
        glTexImage2D(GL_TEXTURE_2D, level, glInternalFormat, width, height, 
                     0, glFormat, glType, data);
    } else {
        int size = GetPixelDataSize(width, height, format);
        glCompressedTexImage2D(GL_TEXTURE_2D, level, glInternalFormat, 
                              width, height, 0, size, data);
    }
}

static void set_texture_swizzle(int format)
{
    static const GLint grayscale[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
    static const GLint gray_alpha[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
    
    const GLint *mask = NULL;
    if (format == RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) mask = grayscale;
    else if (format == RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) mask = gray_alpha;
    
    if (mask) {
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, mask);
    }
}

static void set_texture_wrap(TextureWrap wrap)
{
    static const GLenum wrap_modes[] = {
        [TEXTURE_WRAP_REPEAT] = GL_REPEAT,
        [TEXTURE_WRAP_CLAMP] = GL_CLAMP_TO_EDGE,
        [TEXTURE_WRAP_MIRROR_REPEAT] = GL_MIRRORED_REPEAT,
        [TEXTURE_WRAP_MIRROR_CLAMP] = GL_MIRROR_CLAMP_TO_EDGE
    };
    
    if (wrap < sizeof(wrap_modes) / sizeof(wrap_modes[0]) && wrap_modes[wrap]) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_modes[wrap]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_modes[wrap]);
    }
}

static void set_texture_filter(TextureFilter filter)
{
    typedef struct {
        GLenum mag, min;
        float aniso;
    } FilterMode;
    
    static const FilterMode modes[] = {
        [TEXTURE_FILTER_POINT] = {GL_NEAREST, GL_NEAREST, 0},
        [TEXTURE_FILTER_BILINEAR] = {GL_LINEAR, GL_LINEAR, 0},
        [TEXTURE_FILTER_TRILINEAR] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 0},
        [TEXTURE_FILTER_ANISOTROPIC_4X] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 4.0f},
        [TEXTURE_FILTER_ANISOTROPIC_8X] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 8.0f},
        [TEXTURE_FILTER_ANISOTROPIC_16X] = {GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, 16.0f}
    };
    
    if (filter < sizeof(modes) / sizeof(modes[0]) && modes[filter].mag) {
        const FilterMode *m = &modes[filter];
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m->mag);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m->min);
        if (m->aniso > 0) {
            float maxAniso = 1.0f;
            if (has_gl_anisotropy(&maxAniso)) {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fminf(m->aniso, maxAniso));
            }
        }
    }
}

static Texture2D load_texture(const Image* image, TextureWrap wrap, 
                               TextureFilter filter, bool srgb)
{
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const uint8_t *dataPtr = image->data;
    int mipW = image->width, mipH = image->height;

    for (int i = 0; i < image->mipmaps; i++) {
        upload_mipmap(dataPtr, mipW, mipH, i, image->format, srgb);
        if (i == 0) set_texture_swizzle(image->format);

        int mipSize = GetPixelDataSize(mipW, mipH, image->format);
        if (dataPtr) dataPtr += mipSize;

        mipW = (mipW > 1) ? mipW / 2 : 1;
        mipH = (mipH > 1) ? mipH / 2 : 1;
    }

    if (image->mipmaps == 1 && filter >= TEXTURE_FILTER_TRILINEAR) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    set_texture_wrap(wrap);
    set_texture_filter(filter);
    glBindTexture(GL_TEXTURE_2D, 0);

    return (Texture2D) {
        .id = id,
        .width = image->width,
        .height = image->height,
        .mipmaps = image->mipmaps,
        .format = image->format
    };
}

// ========================================
// IMAGE LOADING FUNCTIONS
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
            // Compressed texture - must decode
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
// HELPER FUNCTIONS
// ========================================

static bool is_srgb(r3d_importer_texture_map_t map, R3D_ColorSpace space)
{
    return (space == R3D_COLORSPACE_SRGB && (map == R3D_MAP_ALBEDO || map == R3D_MAP_EMISSION));
}

static TextureWrap get_wrap_mode(enum aiTextureMapMode wrap)
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
        loaded_image_t* image = &ctx->images[jobIndex];

        load_image_for_map(image, ctx->importer, material, (r3d_importer_texture_map_t)mapIdx);

        // Push to ready queue
        ring_push(ctx, jobIndex);
    }
    
    return 0;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

r3d_importer_texture_cache_t* r3d_importer_load_texture_cache(const r3d_importer_t* importer, R3D_ColorSpace colorSpace, TextureFilter filter)
{
    if (!importer || !r3d_importer_is_valid(importer)) {
        TraceLog(LOG_ERROR, "R3D: Invalid importer for texture loading");
        return NULL;
    }

    // Allocate cache
    r3d_importer_texture_cache_t* cache = RL_MALLOC(sizeof(r3d_importer_texture_cache_t));
    cache->materialCount = r3d_importer_get_material_count(importer);
    cache->materials = RL_CALLOC(cache->materialCount, sizeof(loaded_material_t));

    // Setup loading context
    loader_context_t ctx = {0};
    ctx.importer = importer;
    ctx.materialCount = cache->materialCount;
    ctx.totalJobs = cache->materialCount * R3D_MAP_COUNT;
    atomic_init(&ctx.nextJob, 0);
    atomic_init(&ctx.allJobsSubmitted, false);

    // Allocate image storage (single flat array)
    ctx.images = RL_CALLOC(ctx.totalJobs, sizeof(loaded_image_t));

    // Allocate ring buffer for ready jobs
    ctx.readyCapacity = ctx.totalJobs;
    ctx.readyJobs = RL_MALLOC(ctx.readyCapacity * sizeof(int));
    atomic_init(&ctx.readyHead, 0);
    atomic_init(&ctx.readyTail, 0);

    // Determine thread count
    int numThreads = get_cpu_count();
    if (numThreads > ctx.totalJobs) {
        numThreads = ctx.totalJobs;
    }

    TraceLog(LOG_INFO, "R3D: Loading textures with %d worker threads", numThreads);

    // Launch worker threads
    thrd_t* threads = RL_MALLOC(numThreads * sizeof(thrd_t));
    for (int i = 0; i < numThreads; i++) {
        thrd_create(&threads[i], worker_thread, &ctx);
    }

    // Progressive upload loop on main thread
    int uploadedCount = 0;
    while (uploadedCount < ctx.totalJobs) {
        int jobIndex;

        // Try to get a ready job from ring buffer
        if (ring_pop(&ctx, &jobIndex)) {
            int materialIdx = jobIndex / R3D_MAP_COUNT;
            int mapIdx = jobIndex % R3D_MAP_COUNT;

            loaded_image_t* img = &ctx.images[jobIndex];

            // Upload texture to GPU
            if (img->image.data) {
                Texture2D* texture = &cache->materials[materialIdx].textures[mapIdx];
                *texture = load_texture(
                    &img->image, get_wrap_mode(img->wrap[0]),
                    filter, is_srgb(mapIdx, colorSpace)
                );

                // Free image data
                if (img->owned) {
                    UnloadImage(img->image);
                    img->image.data = NULL;
                }
            }

            uploadedCount++;
        }
        else {
            // No job ready yet, yield CPU
            thrd_yield();
        }
    }

    // Join threads
    for (int i = 0; i < numThreads; i++) {
        thrd_join(threads[i], NULL);
    }

    // Cleanup
    RL_FREE(threads);
    RL_FREE(ctx.readyJobs);
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
