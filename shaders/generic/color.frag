/* color.frag -- Generic fragment shader used only to draw a color (e.g. background)
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

uniform vec4 uColor;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = uColor;
}
