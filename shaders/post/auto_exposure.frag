#version 330 core

uniform sampler2D uSceneTex;
uniform float uExposure;

out vec4 FragColor;

void main()
{
    vec4 color = texelFetch(uSceneTex, ivec2(gl_FragCoord.xy), 0);
    FragColor = vec4(color.rgb * uExposure, 1.0);
}
