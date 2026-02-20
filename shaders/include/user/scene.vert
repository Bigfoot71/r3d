/* scene.vs -- Contains everything for custom user scene vertex shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Built-In Constants === */

#define MATRIX_MODEL    uMatModel
#define MATRIX_NORMAL   mat3(uMatNormal)

#if defined(UNLIT) || defined(DEPTH) || defined(DEPTH_CUBE) || defined(PROBE)
#   define MATRIX_INV_VIEW          uMatInvView
#   define MATRIX_VIEW_PROJECTION   uMatViewProj
#else
#   define MATRIX_INV_VIEW          uView.invView
#   define MATRIX_VIEW_PROJECTION   uView.viewProj
#endif

/* === Built-In Output Variables === */

vec3 POSITION = vec3(0.0);
vec2 TEXCOORD = vec2(0.0);
vec3 EMISSION = vec3(0.0);
vec4 COLOR    = vec4(0.0);
vec4 TANGENT  = vec4(0.0);
vec3 NORMAL   = vec3(0.0);

vec3 INSTANCE_POSITION = vec3(0.0);
vec4 INSTANCE_ROTATION = vec4(0.0);
vec3 INSTANCE_SCALE    = vec3(0.0);
vec4 INSTANCE_COLOR    = vec4(0.0);
vec4 INSTANCE_CUSTOM   = vec4(0.0);

/* === Built-In Globals === */

int FRAME_INDEX = 0;
float TIME = 0.0;

/* === Internal Vertex Stage === */

#define vertex()

void SceneVertex()
{
    INSTANCE_POSITION = iPosition;
    INSTANCE_ROTATION = iRotation;
    INSTANCE_SCALE = iScale;
    INSTANCE_COLOR = iColor;
    INSTANCE_CUSTOM = iCustom;

    POSITION = aPosition;
    TEXCOORD = uTexCoordOffset + aTexCoord * uTexCoordScale;
    EMISSION = uEmissionColor * uEmissionEnergy;
    COLOR = aColor * uAlbedoColor;
    TANGENT = aTangent;
    NORMAL = aNormal;

    FRAME_INDEX = uFrame.index;
    TIME = uFrame.time;

    vertex();
}
