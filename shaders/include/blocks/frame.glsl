/* frame.glsl -- Contains all the information about the current frame.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

struct Frame {
    vec2 screenSize;
    vec2 texelSize;
    float time;
    int index;
};

layout(std140) uniform FrameBlock {
    Frame uFrame;
};
