/* decal.vert -- Vertex shader used for rendering decals into G-buffers
 *
 * Copyright (c) 2025 Michael Blaine
 * This file is derived from the work of Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Attributes === */

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aColor;
layout(location = 4) in vec4 aTangent;

layout(location = 10) in mat4 iMatModel;
layout(location = 14) in vec4 iColor;

/* === Uniforms === */

uniform mat4 uMatInvView;
uniform mat4 uMatNormal;
uniform mat4 uMatModel;
uniform mat4 uMatVP;

uniform vec4 uAlbedoColor;
uniform float uEmissionEnergy;
uniform vec3 uEmissionColor;
uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;

uniform bool uInstancing;

uniform vec2 viewportSize;

/* === Varyings === */

flat out vec3 vEmission;
out vec2 vTexCoord;
out vec4 vColor;
out mat3 vTBN;

out vec3 vNormal;

/* === Main program === */

void main()
{
    mat4 matModel = uMatModel;
    mat3 matNormal = mat3(uMatNormal);

    if (uInstancing) {
        matModel = transpose(iMatModel) * matModel;
        matNormal = mat3(transpose(inverse(iMatModel))) * matNormal;
    }

    vec3 T = normalize(matNormal * aTangent.xyz);
    vec3 N = normalize(matNormal * aNormal);
    vec3 B = normalize(cross(N, T) * aTangent.w);

    vec3 position = vec3(matModel * vec4(aPosition, 1.0));
    vTexCoord = uTexCoordOffset + aTexCoord * uTexCoordScale;
    vEmission = uEmissionColor * uEmissionEnergy;
    vColor = aColor * iColor * uAlbedoColor;
    vTBN = mat3(T, B, N);

    vNormal = N;

    gl_Position = uMatVP * vec4(position, 1.0);
}
