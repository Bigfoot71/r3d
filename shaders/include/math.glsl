/* math.glsl -- Contains everything you need for maths
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Constants === */

#define M_PI        3.1415926535897931
#define M_TAU       6.2831853071795862
#define M_INV_PI    0.3183098861837907

/* === Functions === */

vec3 M_Rotate3D(vec3 v, vec4 q)
{
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

mat3 M_OrthonormalBasis(vec3 n)
{
    // Previously we used Frisvad's method to generate a stable orthonormal basis
    // SEE: https://backend.orbit.dtu.dk/ws/portalfiles/portal/126824972/onb_frisvad_jgt2012_v2.pdf

    // However, it can cause visible artifacts (eg. bright pixels on the -Z face of irradiance cubemaps)
    // So now we use the revised method by Duff et al., it's more accurate, though slightly slower
    // SEE: https://graphics.pixar.com/library/OrthonormalB/paper.pdf

    float sgn = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (sgn + n.z);
    float b = n.x * n.y * a;

    vec3 t = vec3(1.0 + sgn * n.x * n.x * a, sgn * b, -sgn * n.x);
    vec3 bt = vec3(b, sgn + n.y * n.y * a, -n.y);

    return mat3(t, bt, n);
}

vec2 M_OctahedronWrap(vec2 val)
{
    // Reference(s):
    // - Octahedron normal vector encoding
    //   https://web.archive.org/web/20191027010600/https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/comment-page-1/
    return (1.0 - abs(val.yx)) * mix(vec2(-1.0), vec2(1.0), vec2(greaterThanEqual(val.xy, vec2(0.0))));
}

vec3 M_DecodeOctahedral(vec2 encoded)
{
    encoded = encoded * 2.0 - 1.0;

    vec3 normal;
    normal.z  = 1.0 - abs(encoded.x) - abs(encoded.y);
    normal.xy = normal.z >= 0.0 ? encoded.xy : M_OctahedronWrap(encoded.xy);
    return normalize(normal);
}

vec2 M_EncodeOctahedral(vec3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    normal.xy = normal.z >= 0.0 ? normal.xy : M_OctahedronWrap(normal.xy);
    normal.xy = normal.xy * 0.5 + 0.5;
    return normal.xy;
}

vec3 M_NormalScale(vec3 normal, float scale)
{
    normal.xy *= scale;
    normal.z = sqrt(1.0 - clamp(dot(normal.xy, normal.xy), 0.0, 1.0));
    return normal;
}

float M_HashIGN(vec2 pos)
{
    // http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(pos, magic.xy)));
}
