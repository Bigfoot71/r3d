/* cubeface_down.frag -- Fragment shader for downsampling a cubemap face
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

noperspective in vec2 vTexCoord;

uniform samplerCube uSourceTex;
uniform float uSourceTexel;
uniform float uSourceLod;
uniform int uSourceFace;

out vec4 FragColor;

vec3 GetDirection(vec2 uv, int face)
{
    uv = uv * 2.0 - 1.0;

    switch(face) {
    case 0: return normalize(vec3( 1.0, -uv.y, -uv.x)); // +X
    case 1: return normalize(vec3(-1.0, -uv.y,  uv.x)); // -X
    case 2: return normalize(vec3( uv.x,  1.0,  uv.y)); // +Y
    case 3: return normalize(vec3( uv.x, -1.0, -uv.y)); // -Y
    case 4: return normalize(vec3( uv.x, -uv.y,  1.0)); // +Z
    case 5: return normalize(vec3(-uv.x, -uv.y, -1.0)); // -Z
    }

    return vec3(0.0);
}

void main()
{
    float offset = uSourceTexel * 0.5;

    vec2 offsets[4] = vec2[4](
        vec2(-offset, -offset),
        vec2(+offset, -offset),
        vec2(-offset, +offset),
        vec2(+offset, +offset)
    );

    vec4 result = vec4(0.0);

    result += textureLod(uSourceTex, GetDirection(vTexCoord + offsets[0], uSourceFace), uSourceLod);
    result += textureLod(uSourceTex, GetDirection(vTexCoord + offsets[1], uSourceFace), uSourceLod);
    result += textureLod(uSourceTex, GetDirection(vTexCoord + offsets[2], uSourceFace), uSourceLod);
    result += textureLod(uSourceTex, GetDirection(vTexCoord + offsets[3], uSourceFace), uSourceLod);

    FragColor = result * 0.25;
}
