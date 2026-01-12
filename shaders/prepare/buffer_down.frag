#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uOrmTex;
uniform sampler2D uDepthTex;
uniform sampler2D uDiffuseTex;

/* === Fragments === */

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragNormal;
layout(location = 2) out vec4 FragOrm;
layout(location = 3) out vec4 FragDepth;
layout(location = 4) out vec4 FragDiffuse;

/* === Main Program === */

void main()
{
    ivec2 pixCoord = 2 * ivec2(gl_FragCoord.xy);

    ivec2 p0 = pixCoord + ivec2(0, 0);
    ivec2 p1 = pixCoord + ivec2(1, 0);
    ivec2 p2 = pixCoord + ivec2(0, 1);
    ivec2 p3 = pixCoord + ivec2(1, 1);

    float d0 = texelFetch(uDepthTex, p0, 0).r;
    float d1 = texelFetch(uDepthTex, p1, 0).r;
    float d2 = texelFetch(uDepthTex, p2, 0).r;
    float d3 = texelFetch(uDepthTex, p3, 0).r;

    ivec2 selectedPixel = p0;
    float closestDepth = d0;

    if (d1 < closestDepth) { selectedPixel = p1; closestDepth = d1; }
    if (d2 < closestDepth) { selectedPixel = p2; closestDepth = d2; }
    if (d3 < closestDepth) { selectedPixel = p3; closestDepth = d3; }

    FragNormal  = texelFetch(uNormalTex, selectedPixel, 0);
    FragDepth   = vec4(closestDepth, 0.0, 0.0, 1.0);
    FragAlbedo  = texelFetch(uAlbedoTex, selectedPixel, 0);
    FragOrm     = texelFetch(uOrmTex, selectedPixel, 0);
    FragDiffuse = texelFetch(uDiffuseTex, selectedPixel, 0);
}
