/* scene.vs -- Contains everything for custom user scene vertex shader
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

vec3 POSITION;
vec2 TEXCOORD;
vec3 EMISSION;
vec4 COLOR;
vec4 TANGENT;
vec3 NORMAL;

#define vertex()

void SceneVertex()
{
    POSITION = aPosition;
    TEXCOORD = uTexCoordOffset + aTexCoord * uTexCoordScale;
    EMISSION = uEmissionColor * uEmissionEnergy;
    COLOR = aColor * iColor * uAlbedoColor;
    TANGENT = aTangent;
    NORMAL = aNormal;

    vertex();
}
