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

/* === Defines === */

#define NUM_LIGHTS 8

/* === Attributes === */

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;

/* === Instance attributes === */

layout(location = 10) in mat4 iMatModel;
layout(location = 14) in vec4 iColor;

/* === Uniforms === */

uniform mat4 uMatModel;
uniform mat4 uMatMVP;

uniform mat4 uMatLightMVP[NUM_LIGHTS];

uniform vec4 uColAlbedo;

/* === Varyings === */

out vec3 vPosition;
out vec2 vTexCoord;
out vec4 vColor;
out mat3 vTBN;

out vec4 vPosLightSpace[NUM_LIGHTS];

/* === Main program === */

void main()
{
    vTexCoord = aTexCoord;
    vColor = aColor * iColor * uColAlbedo;

    mat4 matInst = transpose(iMatModel);
    mat4 matModel = uMatModel * matInst;
    mat3 matNormal = transpose(inverse(mat3(matModel)));

    vPosition = vec3(matModel * vec4(aPosition, 1.0));

    // The TBN matrix is used to transform vectors from tangent space to world space
    // It is currently used to transform normals from a normal map to world space normals
    vec3 T = normalize(vec3(matModel * vec4(aTangent.xyz, 0.0)));
    vec3 N = normalize(matNormal * aNormal);
    vec3 B = normalize(cross(N, T)) * aTangent.w;
    vTBN = mat3(T, B, N);

    for (int i = 0; i < NUM_LIGHTS; i++)
    {
        vPosLightSpace[i] = uMatLightMVP[i] * vec4(vPosition, 1.0);
    }

    gl_Position = uMatMVP * (matInst * vec4(aPosition, 1.0));
}
