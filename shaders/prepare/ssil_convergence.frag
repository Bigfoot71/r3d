/* ssil_convergence.frag -- SSIL history blending fragment shader
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Varyings == */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexCurrent;
uniform sampler2D uTexHistory;
uniform float uConvergence;

/* === Fragments === */

layout(location = 0) out vec4 FragColor;

/* === Program === */

void main()
{
    vec4 current = texture(uTexCurrent, vTexCoord);
    vec4 history = texture(uTexHistory, vTexCoord);
    FragColor = mix(current, history, uConvergence);
}
