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

/* === Uniforms === */

uniform mat4 uMatNormal; // Unused - placeholder for future implementation
uniform mat4 uMatModel;
uniform mat4 uMatVP;

uniform vec4 uAlbedoColor;
uniform float uEmissionEnergy;
uniform vec3 uEmissionColor;

uniform bool uInstancing;

/* === Varyings === */

out mat4 vFinalMatModel;
flat out vec3 vEmission;
out vec4 vColor;
out vec4 vClipPos;

/* === Main program === */

void main()
{
    mat4 matModel = uMatModel;
    mat3 matNormal = mat3(uMatNormal);

    if (uInstancing) {
        matModel = transpose(iMatModel) * matModel;
        matNormal = mat3(transpose(inverse(iMatModel))) * matNormal;
    }

    vFinalMatModel = matModel;

    vColor = aColor * uAlbedoColor;
    vEmission = uEmissionColor * uEmissionEnergy;

    vec3 position = vec3(matModel * vec4(aPosition, 1.0));
    gl_Position = uMatVP * vec4(position, 1.0);
    vClipPos  = gl_Position;
}
