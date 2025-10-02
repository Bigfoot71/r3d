/* skybox.frag -- Fragment shader used to render skyboxes
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

in vec3 vPosition;

uniform samplerCube uCubeSky;
uniform float uSkyIntensity;

layout(location = 0) out vec3 FragColor;

void main()
{
    FragColor = texture(uCubeSky, vPosition).rgb * uSkyIntensity;
}
