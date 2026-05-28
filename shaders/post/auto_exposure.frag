#version 330 core

uniform sampler2D uSceneTex;
uniform sampler2D uExposureTex;

out vec4 FragColor;

void main()
{
    vec4 color = texelFetch(uSceneTex, ivec2(gl_FragCoord.xy), 0);
    float exposure = texelFetch(uExposureTex, ivec2(0), 0).r;
    FragColor = vec4(color.rgb * exposure, color.a);
}
