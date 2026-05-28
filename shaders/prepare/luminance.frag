#version 330 core

uniform sampler2D uSourceTex;

out float FragColor;

void main()
{
    const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);
    const float EPSILON = 1e-4;

    ivec2 srcSize = textureSize(uSourceTex, 0);
    ivec2 base = 2 * ivec2(gl_FragCoord.xy);

    ivec2 p0 = min(base + ivec2(0, 0), srcSize - 1);
    ivec2 p1 = min(base + ivec2(1, 0), srcSize - 1);
    ivec2 p2 = min(base + ivec2(0, 1), srcSize - 1);
    ivec2 p3 = min(base + ivec2(1, 1), srcSize - 1);

    vec3 c0 = texelFetch(uSourceTex, p0, 0).rgb;
    vec3 c1 = texelFetch(uSourceTex, p1, 0).rgb;
    vec3 c2 = texelFetch(uSourceTex, p2, 0).rgb;
    vec3 c3 = texelFetch(uSourceTex, p3, 0).rgb;

    float l0 = max(dot(c0, LUMA), EPSILON);
    float l1 = max(dot(c1, LUMA), EPSILON);
    float l2 = max(dot(c2, LUMA), EPSILON);
    float l3 = max(dot(c3, LUMA), EPSILON);

    FragColor = 0.25 * (log(l0) + log(l1) + log(l2) + log(l3));
}
