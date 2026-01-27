/* scene.fs -- Contains everything for custom user scene fragment shader
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

vec3 ALBEDO;
float ALPHA;
vec3 EMISSION;
vec3 TANGENT;
vec3 BITANGENT;
vec3 NORMAL;
vec3 NORMAL_MAP;
float OCCLUSION;
float ROUGHNESS;
float METALNESS;

#define fragment()

void SceneFragment(vec2 texCoord, float alphaCutoff)
{
    vec4 color = vColor * texture(uAlbedoMap, texCoord);
    if (color.a < alphaCutoff) discard;

    ALBEDO = color.rgb;
    ALPHA = color.a;

#if !defined(DEPTH) && !defined(DEPTH_CUBE)

    EMISSION   = vEmission * texture(uEmissionMap, texCoord).rgb;
    NORMAL_MAP = texture(uNormalMap, texCoord).rgb;
    vec3 ORM   = texture(uOrmMap, texCoord).rgb;
    OCCLUSION  = uOcclusion * ORM.x;
    ROUGHNESS  = uRoughness * ORM.y;
    METALNESS  = uMetalness * ORM.z;

#if !defined(DECAL)
    TANGENT    = vTBN[0];
    BITANGENT  = vTBN[1];
    NORMAL     = vTBN[2];
#endif // !DECAL

#endif // !DEPTH && !DEPTH_CUBE

    fragment();

    // Alpha cutoff again after user code
    if (ALPHA < alphaCutoff) discard;
}
