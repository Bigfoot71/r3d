/* blur_up.frag - Upsampling part of ARM dual filtering
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Adapted from the ARM dual filtering method.
// See: https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_notes.pdf

#version 330 core

noperspective in vec2 vTexCoord;
uniform sampler2D uTexSource;
uniform int uMipSource;
out vec4 FragColor;

void main()
{
    vec2 halfPixel = 0.5 / vec2(textureSize(uTexSource, uMipSource));
    float lod = float(uMipSource);

    vec4 sum = textureLod(uTexSource, vTexCoord + vec2(-halfPixel.x * 2.0, 0.0), lod);
    sum += textureLod(uTexSource, vTexCoord + vec2(-halfPixel.x, halfPixel.y), lod) * 2.0;
    sum += textureLod(uTexSource, vTexCoord + vec2(0.0, halfPixel.y * 2.0), lod);
    sum += textureLod(uTexSource, vTexCoord + vec2(halfPixel.x, halfPixel.y), lod) * 2.0;
    sum += textureLod(uTexSource, vTexCoord + vec2(halfPixel.x * 2.0, 0.0), lod);
    sum += textureLod(uTexSource, vTexCoord + vec2(halfPixel.x, -halfPixel.y), lod) * 2.0;
    sum += textureLod(uTexSource, vTexCoord + vec2(0.0, -halfPixel.y * 2.0), lod);
    sum += textureLod(uTexSource, vTexCoord + vec2(-halfPixel.x, -halfPixel.y), lod) * 2.0;

    FragColor = sum / 12.0;
}
