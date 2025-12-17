/* geometry.vert -- Vertex shader used for rendering in G-buffers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/billboard.glsl"

/* === Attributes === */

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;
layout(location = 5) in ivec4 aBoneIDs;
layout(location = 6) in vec4 aWeights;

layout(location = 10) in mat4 iMatModel;
layout(location = 14) in vec4 iColor;

/* === Uniforms === */

uniform sampler1D uTexBoneMatrices;

uniform mat4 uMatInvView;       ///< Only for billboard modes
uniform mat4 uMatNormal;
uniform mat4 uMatModel;
uniform mat4 uMatVP;

uniform vec4 uAlbedoColor;
uniform float uEmissionEnergy;
uniform vec3 uEmissionColor;
uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;

uniform bool uInstancing;
uniform bool uSkinning;
uniform int uBillboard;

/* === Varyings === */

flat out vec3 vEmission;
out vec2 vTexCoord;
out vec4 vColor;
out mat3 vTBN;

/* === Helper Functions === */

mat4 BoneMatrix(int boneID)
{
    int baseIndex = 4 * boneID;

    vec4 row0 = texelFetch(uTexBoneMatrices, baseIndex + 0, 0);
    vec4 row1 = texelFetch(uTexBoneMatrices, baseIndex + 1, 0);
    vec4 row2 = texelFetch(uTexBoneMatrices, baseIndex + 2, 0);
    vec4 row3 = texelFetch(uTexBoneMatrices, baseIndex + 3, 0);

    return transpose(mat4(row0, row1, row2, row3));
}

mat4 SkinMatrix(ivec4 boneIDs, vec4 weights)
{
    return weights.x * BoneMatrix(boneIDs.x) +
           weights.y * BoneMatrix(boneIDs.y) +
           weights.z * BoneMatrix(boneIDs.z) +
           weights.w * BoneMatrix(boneIDs.w);
}

/* === Main program === */

void main()
{
    mat4 matModel = uMatModel;
    mat3 matNormal = mat3(uMatNormal);

    if (uSkinning) {
        mat4 sMatModel = SkinMatrix(aBoneIDs, aWeights);
        matModel = matModel * sMatModel;
        matNormal = matNormal * mat3(transpose(inverse(sMatModel)));
    }

    if (uInstancing) {
        matModel = transpose(iMatModel) * matModel;
        matNormal = mat3(transpose(inverse(iMatModel))) * matNormal;
    }

    switch(uBillboard) {
    case BILLBOARD_NONE:
        break;
    case BILLBOARD_FRONT:
        BillboardFront(matModel, matNormal, uMatInvView);
        break;
    case BILLBOARD_Y_AXIS:
        BillboardYAxis(matModel, matNormal, uMatInvView);
        break;
    }

    vec3 T = normalize(matNormal * aTangent.xyz);
    vec3 N = normalize(matNormal * aNormal);
    vec3 B = normalize(cross(N, T) * aTangent.w);

    vec3 position = vec3(matModel * vec4(aPosition, 1.0));
    vTexCoord = uTexCoordOffset + aTexCoord * uTexCoordScale;
    vEmission = uEmissionColor * uEmissionEnergy;
    vColor = aColor * iColor * uAlbedoColor;
    vTBN = mat3(T, B, N);

    gl_Position = uMatVP * vec4(position, 1.0);
}
