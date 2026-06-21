/* dof_coc.frag -- Circle of Confusion calculation shader
 *
 * Copyright (c) 2025-2026 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

#include <ubo/fx.glsl>

noperspective in vec2 vTexCoord;
uniform sampler2D uDepthTex;
out float FragCoC;

void main()
{
    float depth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    float coc = (1.0 / uDof.focusPoint - 1.0 / depth) * uDof.focusScale;
    coc *= mix(uDof.nearScale, 1.0, step(0.0, coc));
    FragCoC = clamp(coc, -1.0, 1.0);
}
