/* cubemap_irradiance.frag -- Irradiance cubemap generation fragment shader
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Inlcudes === */

#include "../include/math.glsl"

/* === Varyings === */

in vec3 vPosition;

/* === Uniforms === */

uniform samplerCube uSourceTex;

/* === Fragments === */

out vec4 FragColor;

/* === Program === */

void main()
{
    vec3 N = normalize(vPosition);
    mat3 OBN = M_OrthonormalBasis(N);

    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * M_PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * M_PI; theta += sampleDelta) {
            vec3 sampleVec = OBN * vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            irradiance += texture(uSourceTex, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = M_PI * irradiance * (1.0 / float(nrSamples));
    FragColor = vec4(irradiance, 1.0);
}
