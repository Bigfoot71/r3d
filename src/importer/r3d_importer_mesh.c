/* r3d_importer_mesh.c -- Module to import meshes from assimp scene.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer_internal.h"

#include <r3d/r3d_mesh_data.h>
#include <r3d/r3d_mesh.h>
#include <r3d_config.h>
#include <raylib.h>

#include <assimp/mesh.h>
#include <float.h>

#include "../common/r3d_math.h"

// ========================================
// CONSTANTS
// ========================================

#define MAX_BONE_WEIGHTS 4
#define MIN_BONE_WEIGHT_THRESHOLD 1e-3f

// ========================================
// VERTEX PROCESSING (INTERNAL)
// ========================================

static inline void process_vertex_position(Vector3* position, const struct aiVector3D* aiPos, const Matrix* transform, bool hasBones, BoundingBox* aabb)
{
    Vector3 lPosition = r3d_importer_cast(*aiPos);
    Vector3 gPosition = r3d_vector3_transform(lPosition, transform);

    *position = hasBones ? lPosition : gPosition;
    aabb->min = Vector3Min(aabb->min, gPosition);
    aabb->max = Vector3Max(aabb->max, gPosition);
}

static inline void process_vertex_texcoord(Vector2* texcoord, const struct aiMesh* aiMesh, int index)
{
    if (aiMesh->mTextureCoords[0] && aiMesh->mNumUVComponents[0] >= 2) {
        *texcoord = r3d_importer_cast_to_vector2(aiMesh->mTextureCoords[0][index]);
    }
    // NOTE: Vertices are zero-initialized
    //else {
    //    *texcoord = (Vector2) {0.0f, 0.0f};
    //}
}

static inline void process_vertex_normal(Vector3* normal, const struct aiMesh* aiMesh, int index, const Matrix* normalMatrix, bool hasBones)
{
    if (aiMesh->mNormals) {
        *normal = r3d_importer_cast(aiMesh->mNormals[index]);
        if (!hasBones) {
            *normal = r3d_vector3_transform_linear(*normal, normalMatrix);
        }
    }
    else {
        *normal = (Vector3) {0.0f, 0.0f, 1.0f};
    }
}

static inline void process_vertex_tangent(R3D_Vertex* vertex, const struct aiMesh* aiMesh, int index, const Matrix* normalMatrix, bool hasBones)
{
    if (aiMesh->mNormals && aiMesh->mTangents && aiMesh->mBitangents) {
        Vector3 normal = vertex->normal;
        Vector3 tangent = r3d_importer_cast(aiMesh->mTangents[index]);
        Vector3 bitangent = r3d_importer_cast(aiMesh->mBitangents[index]);

        if (!hasBones) {
            tangent = r3d_vector3_transform_linear(tangent, normalMatrix);
            bitangent = r3d_vector3_transform_linear(bitangent, normalMatrix);
        }

        Vector3 reconstructedBitangent = Vector3CrossProduct(normal, tangent);
        float handedness = Vector3DotProduct(reconstructedBitangent, bitangent);
        vertex->tangent = (Vector4) {tangent.x, tangent.y, tangent.z, copysignf(1.0f, handedness)};
    }
    else {
        vertex->tangent = (Vector4) {1.0f, 0.0f, 0.0f, 1.0f};
    }
}

static inline void process_vertex_color(Color* color, const struct aiMesh* aiMesh, int index)
{
    if (aiMesh->mColors[0]) {
        *color = r3d_importer_cast(aiMesh->mColors[0][index]);
    }
    else {
        *color = WHITE;
    }
}

// ========================================
// FACE/INDEX PROCESSING (INTERNAL)
// ========================================

static void process_indices(const struct aiMesh* aiMesh, R3D_MeshData* data)
{
    uint32_t* indexPtr = data->indices;
    for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
        const struct aiFace* face = &aiMesh->mFaces[i];
        *indexPtr++ = face->mIndices[0];
        *indexPtr++ = face->mIndices[1];
        *indexPtr++ = face->mIndices[2];
    }
}

// ========================================
// BONE PROCESSING (INTERNAL)
// ========================================

static inline bool assign_bone_weight(R3D_Vertex* vertex, uint32_t boneIndex, float weightValue)
{
    int emptySlot = -1;
    int minWeightSlot = 0;
    float minWeight = vertex->weights[0];

    // Pass to find both empty slot and minimum weight
    for (int slot = 0; slot < MAX_BONE_WEIGHTS; slot++) {
        float w = vertex->weights[slot];
        if (w == 0.0f && emptySlot == -1) {
            emptySlot = slot;
        }
        if (w < minWeight) {
            minWeight = w;
            minWeightSlot = slot;
        }
    }

    // Use empty slot if available
    if (emptySlot != -1) {
        vertex->weights[emptySlot] = weightValue;
        vertex->boneIds[emptySlot] = boneIndex;
        return true;
    }

    // All slots occupied - replace if new weight is larger
    if (weightValue > minWeight) {
        vertex->weights[minWeightSlot] = weightValue;
        vertex->boneIds[minWeightSlot] = boneIndex;
        return true;
    }

    return false;
}

static void normalize_bone_weights(R3D_Vertex* vertex)
{
    float w0 = vertex->weights[0];
    float w1 = vertex->weights[1];
    float w2 = vertex->weights[2];
    float w3 = vertex->weights[3];

    float totalWeight = w0 + w1 + w2 + w3;

    if (totalWeight > 0.0f) {
        float invTotal = 1.0f / totalWeight;
        vertex->weights[0] = w0 * invTotal;
        vertex->weights[1] = w1 * invTotal;
        vertex->weights[2] = w2 * invTotal;
        vertex->weights[3] = w3 * invTotal;
    }
    else {
        vertex->weights[0] = 1.0f;
    }
}

static bool process_bones(const struct aiMesh* aiMesh, R3D_MeshData* data, int vertexCount)
{
    if (aiMesh->mNumBones == 0) {
        // No bones - initialize default weights
        for (int i = 0; i < vertexCount; i++) {
            data->vertices[i].weights[0] = 1.0f;
        }
        return true;
    }

    // Process each bone
    for (unsigned int boneIndex = 0; boneIndex < aiMesh->mNumBones; boneIndex++)
    {
        const struct aiBone* bone = aiMesh->mBones[boneIndex];

        // Process all vertex weights for this bone
        for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++)
        {
            const struct aiVertexWeight* weight = &bone->mWeights[weightIndex];
            uint32_t vertexId = weight->mVertexId;
            float weightValue = weight->mWeight;

            // Validate vertex ID
            if (vertexId >= (uint32_t)vertexCount) {
                R3D_TRACELOG(LOG_ERROR, "Invalid vertex ID %u in bone weights (max: %d)", vertexId, vertexCount);
                continue;
            }

            // Skip negligible weights
            if (weightValue < MIN_BONE_WEIGHT_THRESHOLD) {
                continue;
            }

            assign_bone_weight(&data->vertices[vertexId], boneIndex, weightValue);
        }
    }

    // Normalize all vertex weights
    for (int i = 0; i < vertexCount; i++) {
        normalize_bone_weights(&data->vertices[i]);
    }

    return true;
}

// ========================================
// MESH LOADING (INTERNAL)
// ========================================

static R3D_PrimitiveType get_primitive_type(unsigned int aiPrimitiveTypes)
{
    // A single mesh may theoretically contain multiple primitive types,
    // but we use `aiProcess_SortByPType` during import, which resolves this issue,
    // so we can assume there is only one primitive type per mesh.

    if (BIT_TEST(aiPrimitiveTypes, aiPrimitiveType_POINT)) {
        return R3D_PRIMITIVE_POINTS;
    }

    if (BIT_TEST(aiPrimitiveTypes, aiPrimitiveType_LINE)) {
        return R3D_PRIMITIVE_LINES;
    }

    if (BIT_TEST(aiPrimitiveTypes, aiPrimitiveType_TRIANGLE)) {
        return R3D_PRIMITIVE_TRIANGLES;
    }

    // NOTE: This should never happen if the mesh has been triangulated.
    //if (BIT_TEST(aiPrimitiveTypes, aiPrimitiveType_POLYGON)) {
    //    return 0;
    //}

    if (BIT_TEST(aiPrimitiveTypes, aiPrimitiveType_NGONEncodingFlag)) {
        R3D_TRACELOG(LOG_WARNING, "NGON primitive encoding not supported");
        return R3D_PRIMITIVE_TRIANGLE_FAN;
    }

    return R3D_PRIMITIVE_TRIANGLES;
}

static bool load_mesh_internal(
    R3D_Mesh* outMesh,
    R3D_MeshData* outMeshData,
    const struct aiMesh* aiMesh,
    Matrix transform,
    bool hasBones)
{
    // Validate input
    if (!aiMesh) {
        R3D_TRACELOG(LOG_ERROR, "Invalid parameters during assimp mesh processing");
        return false;
    }

    if (aiMesh->mNumVertices == 0 || aiMesh->mNumFaces == 0) {
        R3D_TRACELOG(LOG_ERROR, "Empty mesh detected during assimp mesh processing");
        return false;
    }

    // Allocate mesh data
    int vertexCount = aiMesh->mNumVertices;
    int indexCount = 3 * aiMesh->mNumFaces;

    R3D_MeshData data = R3D_CreateMeshData(vertexCount, indexCount);
    if (!data.vertices || !data.indices) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load mesh; Unable to allocate mesh data");
        return false;
    }

    // Initialize bounding box
    BoundingBox aabb;
    aabb.min = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};
    aabb.max = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    // Pre-compute normal matrix for non-bone meshes
    Matrix normalMatrix = {0};
    if (!hasBones) {
        normalMatrix = r3d_matrix_normal(&transform);
    }

    // Process all vertex attributes
    for (int i = 0; i < vertexCount; i++) {
        R3D_Vertex* vertex = &data.vertices[i];
        process_vertex_position(&vertex->position, &aiMesh->mVertices[i], &transform, hasBones, &aabb);
        process_vertex_texcoord(&vertex->texcoord, aiMesh, i);
        process_vertex_normal(&vertex->normal, aiMesh, i, &normalMatrix, hasBones);
        process_vertex_tangent(vertex, aiMesh, i, &normalMatrix, hasBones);
        process_vertex_color(&vertex->color, aiMesh, i);
    }

    // Process indices
    process_indices(aiMesh, &data);

    // Process bone data
    if (!process_bones(aiMesh, &data, vertexCount)) {
        R3D_UnloadMeshData(data);
        return false;
    }

    // Upload the mesh
    R3D_PrimitiveType ptype = get_primitive_type(aiMesh->mPrimitiveTypes);
    *outMesh = R3D_LoadMesh(ptype, data, &aabb, R3D_STATIC_MESH);
    if (outMeshData == NULL) R3D_UnloadMeshData(data);
    else *outMeshData = data;

    return true;
}

// ========================================
// RECURSIVE LOADING
// ========================================

static bool load_recursive(const R3D_Importer* importer, R3D_Model* model, const struct aiNode* node, const Matrix* parentTransform)
{
    Matrix localTransform = r3d_importer_cast(node->mTransformation);
    Matrix globalTransform = r3d_matrix_multiply(&localTransform, parentTransform);

    // Process all meshes in this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        uint32_t meshIndex = node->mMeshes[i];
        const struct aiMesh* mesh = r3d_importer_get_mesh(importer, meshIndex);

        if (!load_mesh_internal(&model->meshes[meshIndex], model->meshData ? &model->meshData[meshIndex] : NULL, mesh, globalTransform, mesh->mNumBones > 0)) {
            R3D_TRACELOG(LOG_ERROR, "Unable to load mesh [%u]; The model will be invalid", meshIndex);
            return false;
        }

        model->meshMaterials[meshIndex] = mesh->mMaterialIndex;
    }

    // Process all children recursively
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        if (!load_recursive(importer, model, node->mChildren[i], &globalTransform)) {
            return false;
        }
    }

    return true;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_load_meshes(const R3D_Importer* importer, R3D_Model* model)
{
    if (!model || !importer || !r3d_importer_is_valid(importer)) {
        R3D_TRACELOG(LOG_ERROR, "Invalid parameters for mesh loading");
        return false;
    }

    bool keepMeshData = BIT_TEST(importer->flags, R3D_IMPORT_MESH_DATA);
    const struct aiScene* scene = r3d_importer_get_scene(importer);

    // Allocate space for meshes
    model->meshCount = scene->mNumMeshes;
    model->meshes = RL_CALLOC(model->meshCount, sizeof(*model->meshes));
    model->meshMaterials = RL_CALLOC(model->meshCount, sizeof(*model->meshMaterials));
    if (keepMeshData) model->meshData = RL_CALLOC(model->meshCount, sizeof(*model->meshData));
    if (!model->meshes || !model->meshMaterials || (keepMeshData && !model->meshData)) {
        R3D_TRACELOG(LOG_ERROR, "Unable to allocate memory for meshes");
        goto cleanup_and_fail;
    }

    // Load all meshes recursively
    if (!load_recursive(importer, model, r3d_importer_get_root(importer), &R3D_MATRIX_IDENTITY)) {
        goto cleanup_and_fail;
    }

    // Calculate model bounding box
    model->aabb.min = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};
    model->aabb.max = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (int i = 0; i < model->meshCount; i++) {
        model->aabb.min = Vector3Min(model->aabb.min, model->meshes[i].aabb.min);
        model->aabb.max = Vector3Max(model->aabb.max, model->meshes[i].aabb.max);
    }

    return true;

cleanup_and_fail:
    if (model->meshes) {
        for (int i = 0; i < model->meshCount; i++) {
            R3D_UnloadMesh(model->meshes[i]);
        }
    }

    if (model->meshData) {
        for (int i = 0; i < model->meshCount; i++) {
            R3D_UnloadMeshData(model->meshData[i]);
        }
    }

    RL_FREE(model->meshMaterials);
    RL_FREE(model->meshData);
    RL_FREE(model->meshes);

    model->meshMaterials = NULL;
    model->meshData = NULL;
    model->meshes = NULL;
    model->meshCount = 0;

    return false;
}
