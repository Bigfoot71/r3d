/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#version 330 core

/* === Includes === */

#include "../include/billboard.glsl"
#include "../include/light.glsl"

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

uniform mat4 uMatLightVP[LIGHT_FORWARD_COUNT];
uniform mat4 uMatInvView;       ///< Only for billboard modes
uniform mat4 uMatNormal;
uniform mat4 uMatModel;
uniform mat4 uMatVP;

uniform vec4 uAlbedoColor;
uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;

uniform bool uInstancing;
uniform bool uSkinning;
uniform int uBillboard;

/* === Varyings === */

out vec3 vPosition;
out vec2 vTexCoord;
out vec4 vColor;
out mat3 vTBN;

out vec4 vPosLightSpace[LIGHT_FORWARD_COUNT];

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
        matModel = sMatModel * matModel;
        matNormal = mat3(transpose(inverse(sMatModel))) * matNormal;
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

    vPosition = vec3(matModel * vec4(aPosition, 1.0));
    vTexCoord = uTexCoordOffset + aTexCoord * uTexCoordScale;
    vColor = aColor * iColor * uAlbedoColor;
    vTBN = mat3(T, B, N);

    for (int i = 0; i < LIGHT_FORWARD_COUNT; i++) {
        vPosLightSpace[i] = uMatLightVP[i] * vec4(vPosition, 1.0);
    }

    gl_Position = uMatVP * vec4(vPosition, 1.0);
}
