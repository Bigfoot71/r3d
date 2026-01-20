/* visualizer.frag -- Buffer visualizer fragment shader
 *
 * Copyright (c) 2025 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Constants === */

#define OUTPUT_ALBEDO       1
#define OUTPUT_NORMAL       2
#define OUTPUT_ORM          3
#define OUTPUT_DIFFUSE      4
#define OUTPUT_SPECULAR     5
#define OUTPUT_SSAO         6
#define OUTPUT_SSIL         7
#define OUTPUT_SSR          8
#define OUTPUT_BLOOM        9

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uSourceTex;
uniform int uOutputMode;

/* === Fragments === */

out vec4 FragColor;

/* === Main Program === */

void main()
{
    FragColor = texture(uSourceTex, vTexCoord);

    switch (uOutputMode) {
    case OUTPUT_ALBEDO:
        FragColor = vec4(FragColor.rgb, 1.0);
        break;
    case OUTPUT_NORMAL:
        FragColor = vec4(0.5 * M_DecodeOctahedral(FragColor.rg) + 0.5, 1.0);
        break;
    case OUTPUT_ORM:
        FragColor = vec4(FragColor.rgb, 1.0);
        break;
    case OUTPUT_DIFFUSE:
        FragColor = vec4(FragColor.rgb, 1.0);
        break;
    case OUTPUT_SPECULAR:
        FragColor = vec4(FragColor.rgb, 1.0);
        break;
    case OUTPUT_SSAO:
        FragColor = vec4(FragColor.xxx, 1.0);
        break;
    case OUTPUT_SSIL:
        FragColor = vec4(mix(FragColor.rgb, vec3(FragColor.a), 0.2), 1.0);
        break;
    case OUTPUT_SSR:
        FragColor = vec4(FragColor.rgb * FragColor.a, 1.0);
        break;
    case OUTPUT_BLOOM:
        FragColor = vec4(FragColor.rgb, 1.0);
        break;
    default:
        break;
    }
}
