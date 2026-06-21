/* view.glsl -- View data structures and uniform block.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#define V_PROJ_PERSP 0
#define V_PROJ_ORTHO 1

struct View {
    vec3 position;
    mat4 view;
    mat4 invView;
    mat4 proj;
    mat4 invProj;
    mat4 viewProj;
    int projMode;
    float aspect;
    float near;
    float far;
};

layout(std140) uniform ViewBlock {
    View uView;
};
