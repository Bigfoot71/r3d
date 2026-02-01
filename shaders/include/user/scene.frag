/* scene.fs -- Contains everything for custom user scene fragment shader
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Built-In Input Variables === */

vec2 TEXCOORD       = vec2(0.0);
vec3 TANGENT        = vec3(0.0);
vec3 BITANGENT      = vec3(0.0);
vec3 NORMAL         = vec3(0.0);

/* === Built-In Output Variables === */

vec3 ALBEDO         = vec3(0.0);
float ALPHA         = 0.0;
vec3 EMISSION       = vec3(0.0);
vec3 NORMAL_MAP     = vec3(0.0);
float OCCLUSION     = 0.0;
float ROUGHNESS     = 0.0;
float METALNESS     = 0.0;

/* === User Callable === */

vec4 SampleAlbedo(vec2 texCoord)
{
    return vColor * texture(uAlbedoMap, texCoord);
}

vec3 SampleEmission(vec2 texCoord)
{
    vec3 emission = vec3(0.0);
#if !defined(UNLIT) && !defined(DEPTH) && !defined(DEPTH_CUBE)
    emission = vEmission * texture(uEmissionMap, texCoord).rgb;
#endif
    return emission;
}

vec3 SampleNormal(vec2 texCoord)
{
    vec3 normal = vec3(0.0);
#if !defined(UNLIT) && !defined(DEPTH) && !defined(DEPTH_CUBE)
    normal = texture(uNormalMap, texCoord).rgb;
#endif
    return normal;
}

vec3 SampleOrm(vec2 texCoord)
{
    vec3 ORM = vec3(0.0);
#if !defined(UNLIT) && !defined(DEPTH) && !defined(DEPTH_CUBE)
    ORM = texture(uOrmMap, texCoord).rgb;
    ORM.x *= uOcclusion;
    ORM.y *= uRoughness;
    ORM.z *= uMetalness;
#endif
    return ORM;
}

void FetchMaterial(vec2 texCoord)
{
    vec4 color = vColor * texture(uAlbedoMap, texCoord);
    ALBEDO = color.rgb;
    ALPHA = color.a;

#if !defined(UNLIT) && !defined(DEPTH) && !defined(DEPTH_CUBE)
    EMISSION   = vEmission * texture(uEmissionMap, texCoord).rgb;
    NORMAL_MAP = texture(uNormalMap, texCoord).rgb;
    vec3 ORM   = texture(uOrmMap, texCoord).rgb;
    OCCLUSION  = uOcclusion * ORM.x;
    ROUGHNESS  = uRoughness * ORM.y;
    METALNESS  = uMetalness * ORM.z;
#endif // !DEPTH && !DEPTH_CUBE
}

/* === Internal Fragment Stage === */

#define fragment()

void SceneFragment(vec2 texCoord, mat3 tbn, float alphaCutoff)
{
    /* --- Fill input variables --- */

    TEXCOORD  = texCoord;
    TANGENT   = tbn[0];
    BITANGENT = tbn[1];
    NORMAL    = tbn[2];

    /* --- Fetch output variables --- */

#if !defined(R3D_NO_AUTO_FETCH)
    vec4 color = vColor * texture(uAlbedoMap, texCoord);
    if (color.a < alphaCutoff) discard;
    ALBEDO = color.rgb;
    ALPHA = color.a;

#if !defined(UNLIT) && !defined(DEPTH) && !defined(DEPTH_CUBE)
    EMISSION   = vEmission * texture(uEmissionMap, texCoord).rgb;
    NORMAL_MAP = texture(uNormalMap, texCoord).rgb;
    vec3 ORM   = texture(uOrmMap, texCoord).rgb;
    OCCLUSION  = uOcclusion * ORM.x;
    ROUGHNESS  = uRoughness * ORM.y;
    METALNESS  = uMetalness * ORM.z;

#endif // !R3D_NO_AUTO_FETCH && !UNLIT && !DEPTH && !DEPTH_CUBE
#endif // !R3D_NO_AUTO_FETCH

    /* --- Execute user code --- */

    fragment();

    // Alpha cutoff again after user code
    if (ALPHA < alphaCutoff) discard;
}
