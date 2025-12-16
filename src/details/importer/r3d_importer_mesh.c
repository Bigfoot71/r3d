/* r3d_importer_mesh.c -- Module to import meshes from assimp scene.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"
#include "../r3d_math.h"

#include <r3d/r3d_mesh_data.h>
#include <r3d/r3d_mesh.h>
#include <raylib.h>

#include <assimp/mesh.h>
#include <string.h>
#include <float.h>

// ========================================
// MESH LOADING (INTERNAL)
// ========================================

static bool load_mesh_internal(R3D_Mesh* outMesh, const struct aiMesh* aiMesh, Matrix transform, bool hasBones)
{
    // Validate input
    if (!aiMesh) {
        TraceLog(LOG_ERROR, "RENDER: Invalid parameters during assimp mesh processing");
        return false;
    }

    if (aiMesh->mNumVertices == 0 || aiMesh->mNumFaces == 0) {
        TraceLog(LOG_ERROR, "RENDER: Empty mesh detected during assimp mesh processing");
        return false;
    }

    // Allocate mesh data
    int vertexCount = aiMesh->mNumVertices;
    int indexCount = 3 * aiMesh->mNumFaces;

    R3D_MeshData data = R3D_CreateMeshData(vertexCount, indexCount);
    if (!data.vertices || !data.indices) {
        TraceLog(LOG_ERROR, "RENDER: Failed to load mesh; Unable to allocate mesh data");
        return false;
    }

    // Initialize bounding box
    BoundingBox aabb;
    aabb.min = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};
    aabb.max = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};

    // Compute normal matrix (for non-bone meshes)
    Matrix normalMatrix = {0};
    if (!hasBones) {
        normalMatrix = r3d_matrix_normal(&transform);
    }

    // Process vertex attributes
    for (int i = 0; i < vertexCount; i++)
    {
        R3D_Vertex* vertex = &data.vertices[i];

        // Position
        Vector3 lPosition = r3d_importer_cast(aiMesh->mVertices[i]);
        Vector3 gPosition = Vector3Transform(lPosition, transform);

        // Meshes with bones keep vertices in local space
        vertex->position = hasBones ? lPosition : gPosition;

        // Always use global position for AABB
        aabb.min = Vector3Min(aabb.min, gPosition);
        aabb.max = Vector3Max(aabb.max, gPosition);

        // Texture coordinates
        if (aiMesh->mTextureCoords[0] && aiMesh->mNumUVComponents[0] >= 2) {
            vertex->texcoord = r3d_importer_cast_to_vector2(aiMesh->mTextureCoords[0][i]);
        } else {
            vertex->texcoord = (Vector2){ 0.0f, 0.0f };
        }

        // Normals
        if (aiMesh->mNormals) {
            vertex->normal = r3d_importer_cast(aiMesh->mNormals[i]);
            if (!hasBones) {
                vertex->normal = Vector3Transform(vertex->normal, normalMatrix);
            }
        } else {
            vertex->normal = (Vector3){ 0.0f, 0.0f, 1.0f };
        }

        // Tangent
        if (aiMesh->mNormals && aiMesh->mTangents && aiMesh->mBitangents) {
            Vector3 normal = vertex->normal;
            Vector3 tangent = r3d_importer_cast(aiMesh->mTangents[i]);
            Vector3 bitangent = r3d_importer_cast(aiMesh->mBitangents[i]);

            if (!hasBones) {
                tangent = Vector3Transform(tangent, normalMatrix);
                bitangent = Vector3Transform(bitangent, normalMatrix);
            }

            Vector3 reconstructedBitangent = Vector3CrossProduct(normal, tangent);
            float handedness = Vector3DotProduct(reconstructedBitangent, bitangent);

            vertex->tangent = (Vector4) {
                tangent.x, tangent.y, tangent.z,
                (handedness < 0.0f) ? -1.0f : 1.0f
            };
        } else {
            vertex->tangent = (Vector4) {1.0f, 0.0f, 0.0f, 1.0f};
        }

        // Vertex color
        if (aiMesh->mColors[0]) {
            vertex->color = r3d_importer_cast(aiMesh->mColors[0][i]);
        } else {
            vertex->color = WHITE;
        }
    }
    
    // Process bone data
    if (aiMesh->mNumBones > 0)
    {
        for (unsigned int boneIndex = 0; boneIndex < aiMesh->mNumBones; boneIndex++)
        {
            const struct aiBone* bone = aiMesh->mBones[boneIndex];
            if (!bone) {
                TraceLog(LOG_WARNING, "RENDER: nullptr bone at index %u", boneIndex);
                continue;
            }

            // Process all vertex weights for this bone
            for (unsigned int weightIndex = 0; weightIndex < bone->mNumWeights; weightIndex++)
            {
                const struct aiVertexWeight* weight = &bone->mWeights[weightIndex];

                uint32_t vertexId = weight->mVertexId;
                float weightValue = weight->mWeight;

                // Validate vertex ID
                if (vertexId >= vertexCount) {
                    TraceLog(LOG_ERROR, "RENDER: Invalid vertex ID %u in bone weights (max: %d)", vertexId, vertexCount);
                    continue;
                }

                // Skip weights that are too small to matter
                if (weightValue < 0.001f) {
                    continue;
                }

                // Find an empty slot in the vertex bone data (max 4 bones per vertex)
                R3D_Vertex* vertex = &data.vertices[vertexId];
                bool slotFound = false;

                for (int slot = 0; slot < 4; slot++) {
                    if (vertex->weights[slot] == 0.0f) {
                        vertex->weights[slot] = weightValue;
                        vertex->boneIds[slot] = (float)boneIndex;
                        slotFound = true;
                        break;
                    }
                }

                // If all 4 slots are occupied, replace the smallest weight
                if (!slotFound) {
                    int minWeightIndex = 0;
                    for (int slot = 1; slot < 4; slot++) {
                        if (vertex->weights[slot] < vertex->weights[minWeightIndex]) {
                            minWeightIndex = slot;
                        }
                    }

                    if (weightValue > vertex->weights[minWeightIndex]) {
                        vertex->weights[minWeightIndex] = weightValue;
                        vertex->boneIds[minWeightIndex] = (float)boneIndex;
                    }
                }
            }
        }

        // Normalize bone weights for each vertex
        for (int i = 0; i < vertexCount; i++) {
            R3D_Vertex* vertex = &data.vertices[i];
            float totalWeight = 0.0f;

            // Calculate total weight
            for (int j = 0; j < 4; j++) {
                totalWeight += vertex->weights[j];
            }

            // Normalize weights if total > 0
            if (totalWeight > 0.0f) {
                for (int j = 0; j < 4; j++) {
                    vertex->weights[j] /= totalWeight;
                }
            } else {
                // If no bone weights, assign to first bone with weight 1.0
                vertex->weights[0] = 1.0f;
                vertex->boneIds[0] = 0.0f;
            }
        }
    }
    else {
        // No bones found for this mesh
        for (int i = 0; i < vertexCount; i++) {
            data.vertices[i].weights[0] = 1.0f;
            data.vertices[i].boneIds[0] = 0.0f;
        }
    }

    // Process indices and validate faces
    size_t indexOffset = 0;
    for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
        const struct aiFace* face = &aiMesh->mFaces[i];

        if (face->mNumIndices != 3) {
            TraceLog(LOG_ERROR, "RENDER: Non-triangular face detected (indices: %u)", face->mNumIndices);
            R3D_UnloadMeshData(&data);
            return false;
        }

        for (unsigned int j = 0; j < 3; j++) {
            if (face->mIndices[j] >= aiMesh->mNumVertices) {
                TraceLog(LOG_ERROR, "RENDER: Invalid vertex index (%u >= %u)", face->mIndices[j], aiMesh->mNumVertices);
                R3D_UnloadMeshData(&data);
                return false;
            }
        }

        data.indices[indexOffset++] = face->mIndices[0];
        data.indices[indexOffset++] = face->mIndices[1];
        data.indices[indexOffset++] = face->mIndices[2];
    }

    // Final validation: index count consistency
    if (indexOffset != indexCount) {
        TraceLog(LOG_ERROR, "RENDER: Inconsistency in the number of indices (%zu != %d)", indexOffset, indexCount);
        R3D_UnloadMeshData(&data);
        return false;
    }

    // Upload the mesh!
    *outMesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, &data, &aabb, R3D_STATIC_MESH);
    R3D_UnloadMeshData(&data);

    return true;
}

// ========================================
// RECURSIVE LOADING
// ========================================

static bool load_recursive(const r3d_importer_t* importer, R3D_Model* model, const struct aiNode* node, Matrix parentTransform)
{
    Matrix localTransform = r3d_importer_cast(node->mTransformation);
    Matrix globalTransform = r3d_matrix_multiply(&localTransform, &parentTransform);

    // Process all meshes in this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        uint32_t meshIndex = node->mMeshes[i];
        const struct aiMesh* mesh = r3d_importer_get_mesh(importer, meshIndex);
        if (!load_mesh_internal(&model->meshes[meshIndex], mesh, globalTransform, mesh->mNumBones > 0)) {
            TraceLog(LOG_ERROR, "RENDER: Unable to load mesh [%u]; The model will be invalid", node->mMeshes[i]);
            return false;
        }
        model->meshMaterials[meshIndex] = mesh->mMaterialIndex;
    }

    // Process all children recursively
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        if (!load_recursive(importer, model, node->mChildren[i], globalTransform)) {
            return false;
        }
    }

    return true;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_load_meshes(const r3d_importer_t* importer, R3D_Model* model)
{
    if (!model || !importer || !r3d_importer_is_valid(importer)) {
        TraceLog(LOG_ERROR, "RENDER: Invalid parameters for mesh loading");
        return false;
    }

    const struct aiScene* scene = r3d_importer_get_scene(importer);

    model->meshCount = scene->mNumMeshes;
    model->meshes = RL_CALLOC(model->meshCount, sizeof(R3D_Mesh));
    model->meshMaterials = RL_CALLOC(model->meshCount, sizeof(int));

    if (!model->meshes || !model->meshMaterials) {
        TraceLog(LOG_ERROR, "RENDER: Unable to allocate memory for meshes");
        if (model->meshMaterials) RL_FREE(model->meshMaterials);
        if (model->meshes) RL_FREE(model->meshes);
        return false;
    }

    // Load all meshes recursively
    if (!load_recursive(importer, model, r3d_importer_get_root(importer), R3D_MATRIX_IDENTITY)) {
        for (int i = 0; i < model->meshCount; i++) {
            R3D_UnloadMesh(&model->meshes[i]);
        }
        RL_FREE(model->meshMaterials);
        RL_FREE(model->meshes);
        return false;
    }

    // Calculate model bounding box
    model->aabb.min = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};
    model->aabb.max = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    for (int i = 0; i < model->meshCount; i++) {
        model->aabb.min = Vector3Min(model->aabb.min, model->meshes[i].aabb.min);
        model->aabb.max = Vector3Max(model->aabb.max, model->meshes[i].aabb.max);
    }

    return true;
}
