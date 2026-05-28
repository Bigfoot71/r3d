#version 330 core

uniform sampler2D uSourceTex;

out vec4 FragColor;

void main()
{
    ivec2 pixCoord = 2 * ivec2(gl_FragCoord.xy);

    vec3 c0 = texelFetch(uSourceTex, pixCoord + ivec2(0, 0), 0).rgb;
    vec3 c1 = texelFetch(uSourceTex, pixCoord + ivec2(1, 0), 0).rgb;
    vec3 c2 = texelFetch(uSourceTex, pixCoord + ivec2(0, 1), 0).rgb;
    vec3 c3 = texelFetch(uSourceTex, pixCoord + ivec2(1, 1), 0).rgb;

    float l0 = dot(c0, vec3(0.2127, 0.7152, 0.0722));
    float l1 = dot(c1, vec3(0.2127, 0.7152, 0.0722));
    float l2 = dot(c2, vec3(0.2127, 0.7152, 0.0722));
    float l3 = dot(c3, vec3(0.2127, 0.7152, 0.0722));

    float avgLog = (log(l0 + 0.0001) + log(l1 + 0.0001)
                  + log(l2 + 0.0001) + log(l3 + 0.0001)) * 0.25;

    FragColor = vec4(avgLog, 0.0, 0.0, 1.0);
}
