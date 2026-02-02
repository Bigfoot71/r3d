/* skybox.vert -- Vertex shader used to render skyboxes
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Constants === */

const vec2 positions[3] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

/* === Uniforms === */

uniform mat4 uMatInvProj;
uniform mat4 uMatInvView;

/* === Varyings === */

out vec3 vViewRay;

/* === Program === */

void main()
{
    vec2 pos = positions[gl_VertexID];
    gl_Position = vec4(pos, 1.0, 1.0);

    vec4 unprojected = uMatInvProj * vec4(pos, 1.0, 1.0);
    vViewRay = mat3(uMatInvView) * unprojected.xyz;
}
