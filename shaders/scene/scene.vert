/* scene.vert -- Common vertex shader for all scene render paths.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Constants === */

#define BILLBOARD_NONE   0
#define BILLBOARD_FRONT  1
#define BILLBOARD_Y_AXIS 2

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Attributes === */

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;
layout(location = 5) in ivec4 aBoneIDs;
layout(location = 6) in vec4 aWeights;

layout(location = 10) in vec3 iPosition;
layout(location = 11) in vec4 iRotation;
layout(location = 12) in vec3 iScale;
layout(location = 13) in vec4 iColor;

/* === Uniforms === */

uniform sampler1D uTexBoneMatrices;

uniform mat4 uMatModel;
uniform mat4 uMatNormal;

uniform vec4 uAlbedoColor;
uniform vec3 uEmissionColor;
uniform float uEmissionEnergy;

uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;
uniform bool uInstancing;

uniform bool uSkinning;
uniform int uBillboard;

#if defined(FORWARD)
uniform mat4 uMatLightVP[LIGHT_FORWARD_COUNT];
#endif // FORWARD

#if defined(DEPTH) || defined(DEPTH_CUBE)
uniform mat4 uMatInvView;   // inv view only for billboard modes
uniform mat4 uMatVP;
#endif // DEPTH || DEPTH_CUBE

/* === Varyings === */

smooth out vec3 vPosition;
smooth out vec2 vTexCoord;
flat   out vec3 vEmission;
smooth out vec4 vColor;
smooth out mat3 vTBN;

#if defined(FORWARD)
smooth out vec4 vPosLightSpace[LIGHT_FORWARD_COUNT];
#endif // FORWARD

#if defined(DECAL)
smooth out mat4 vMatDecal;
smooth out vec4 vClipPos;
#endif // DECAL

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

#if defined(DECAL)
mat4 MatrixTransform(vec3 translation, vec4 quat, vec3 scale)
{
    float xx = quat.x * quat.x;
    float yy = quat.y * quat.y;
    float zz = quat.z * quat.z;
    float xy = quat.x * quat.y;
    float xz = quat.x * quat.z;
    float yz = quat.y * quat.z;
    float wx = quat.w * quat.x;
    float wy = quat.w * quat.y;
    float wz = quat.w * quat.z;

    return mat4(
        scale.x * (1.0 - 2.0 * (yy + zz)), scale.x * 2.0 * (xy + wz),         scale.x * 2.0 * (xz - wy),         0.0,
        scale.y * 2.0 * (xy - wz),         scale.y * (1.0 - 2.0 * (xx + zz)), scale.y * 2.0 * (yz + wx),         0.0,
        scale.z * 2.0 * (xz + wy),         scale.z * 2.0 * (yz - wx),         scale.z * (1.0 - 2.0 * (xx + yy)), 0.0,
        translation.x,                     translation.y,                     translation.z,                     1.0
    );
}
#endif

void BillboardFront(inout vec3 position, inout vec3 normal, inout vec3 tangent, vec3 center, mat4 invView)
{
    vec3 right = invView[0].xyz;
    vec3 up = invView[1].xyz;
    vec3 forward = invView[2].xyz;

    vec3 localPos = position - center;
    vec3 localNormal = normal;
    vec3 localTangent = tangent;
    
    position = center + localPos.x*right + localPos.y*up + localPos.z*forward;
    normal = localNormal.x*right + localNormal.y*up + localNormal.z*forward;
    tangent = localTangent.x*right + localTangent.y*up + localTangent.z*forward;
}

void BillboardYAxis(inout vec3 position, inout vec3 normal, inout vec3 tangent, vec3 center, mat4 invView)
{
    vec3 cameraPos = vec3(invView[3]);
    vec3 upVector = vec3(0, 1, 0);

    vec3 look = normalize(cameraPos - center);
    vec3 right = normalize(cross(upVector, look));
    vec3 front = normalize(cross(right, upVector));

    vec3 localPos = position - center;
    vec3 localNormal = normal;
    vec3 localTangent = tangent;

    position = center + localPos.x*right + localPos.y*upVector + localPos.z*front;
    normal = localNormal.x*right + localNormal.y*upVector + localNormal.z*front;
    tangent = localTangent.x*right + localTangent.y*upVector + localTangent.z*front;
}

/* === Main program === */

void main()
{
#if defined(DECAL)
    mat4 finalMatModel = uMatModel;
#endif // DECAL

    vec3 billboardCenter = vec3(uMatModel[3]);
    vec3 finalPosition = vec3(uMatModel * vec4(aPosition, 1.0));
    vec3 finalNormal = mat3(uMatNormal) * aNormal;
    vec3 finalTangent = mat3(uMatNormal) * aTangent.xyz;

    if (uSkinning)
    {
        mat4 sMatModel = SkinMatrix(aBoneIDs, aWeights);
        mat3 sMatNormal = mat3(transpose(inverse(sMatModel)));
        finalPosition = vec3(sMatModel * vec4(finalPosition, 1.0));
        finalNormal = sMatNormal * finalNormal;
        finalTangent = sMatNormal * finalTangent;
    }

    if (uInstancing)
    {
        billboardCenter += iPosition;
        finalPosition = finalPosition * iScale;
        finalPosition = M_Rotate3D(finalPosition, iRotation);
        finalPosition = finalPosition + iPosition;
        finalNormal = M_Rotate3D(finalNormal, iRotation);
        finalTangent = M_Rotate3D(finalTangent, iRotation);

    #if defined(DECAL)
        mat4 iMatModel = MatrixTransform(iPosition, iRotation, iScale);
        finalMatModel = iMatModel * finalMatModel;
    #endif // DECAL
    }

#if defined(DEPTH) || defined(DEPTH_CUBE)
    if (uBillboard == BILLBOARD_FRONT) {
        BillboardFront(finalPosition, finalNormal, finalTangent, billboardCenter, uMatInvView);
    }
    else if (uBillboard == BILLBOARD_Y_AXIS) {
        BillboardYAxis(finalPosition, finalNormal, finalTangent, billboardCenter, uMatInvView);
    }
#else
    if (uBillboard == BILLBOARD_FRONT) {
        BillboardFront(finalPosition, finalNormal, finalTangent, billboardCenter, uView.invView);
    }
    else if (uBillboard == BILLBOARD_Y_AXIS) {
        BillboardYAxis(finalPosition, finalNormal, finalTangent, billboardCenter, uView.invView);
    }
#endif

    vec3 T = normalize(finalTangent);
    vec3 N = normalize(finalNormal);
    vec3 B = normalize(cross(N, T) * aTangent.w);

    vPosition = finalPosition;
    vTexCoord = uTexCoordOffset + aTexCoord * uTexCoordScale;
    vEmission = uEmissionColor * uEmissionEnergy;
    vColor = aColor * iColor * uAlbedoColor;
    vTBN = mat3(T, B, N);

#if defined(FORWARD)
    for (int i = 0; i < LIGHT_FORWARD_COUNT; i++) {
        vPosLightSpace[i] = uMatLightVP[i] * vec4(vPosition, 1.0);
    }
#endif // FORWARD

#if defined(DEPTH) || defined(DEPTH_CUBE)
    gl_Position = uMatVP * vec4(vPosition, 1.0);
#else
    gl_Position = uView.viewProj * vec4(vPosition, 1.0);
#endif

#if defined(DECAL)
    vMatDecal = inverse(finalMatModel) * uView.invView;
    vClipPos = gl_Position;
#endif // DECAL
}
