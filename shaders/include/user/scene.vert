/* scene.vs -- Contains everything for custom user scene vertex shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
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

vec3 INSTANCE_POSITION;
vec4 INSTANCE_ROTATION;
vec3 INSTANCE_SCALE;
vec4 INSTANCE_COLOR;

#define vertex()

void SceneVertex()
{
    INSTANCE_POSITION = iPosition;
    INSTANCE_ROTATION = iRotation;
    INSTANCE_SCALE = iScale;
    INSTANCE_COLOR = iColor;

    POSITION = aPosition;
    TEXCOORD = uTexCoordOffset + aTexCoord * uTexCoordScale;
    EMISSION = uEmissionColor * uEmissionEnergy;
    COLOR = aColor * uAlbedoColor;
    TANGENT = aTangent;
    NORMAL = aNormal;

    vertex();
}
