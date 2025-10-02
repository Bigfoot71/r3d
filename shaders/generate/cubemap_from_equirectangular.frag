/* cubemap_from_equirectangular.frag -- Fragment shader for converting equirectangular panorama to cubemap
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

in vec3 vPosition;

uniform sampler2D uTexEquirectangular;

out vec4 FragColor;

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, -0.3183); // negative Y, to flip axis
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(vPosition));
    vec3 color = texture(uTexEquirectangular, uv).rgb;
    FragColor = vec4(color, 1.0);
}
