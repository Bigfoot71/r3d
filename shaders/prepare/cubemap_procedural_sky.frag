/* cubemap_procedural_sky.frag -- Procedural sky generation fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

in vec3 vPosition;

/* === Uniforms === */

uniform vec3 uSkyTopColor;
uniform vec3 uSkyHorizonColor;
uniform float uSkyHorizonCurve;
uniform float uSkyEnergy;

uniform vec3 uGroundBottomColor;
uniform vec3 uGroundHorizonColor;
uniform float uGroundHorizonCurve;
uniform float uGroundEnergy;

uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform float uSunSize;
uniform float uSunCurve;
uniform float uSunEnergy;

/* === Fragments === */

out vec4 FragColor;

/* === Program === */

void main()
{
    /* --- Normalization of ray direction --- */

    vec3 eyeDir = normalize(vPosition);
    vec3 sunDir = normalize(uSunDirection);

    /* --- Vertical angle calculation --- */

    float verticalAngle = acos(clamp(eyeDir.y, -1.0, 1.0));

    /* --- Sky gradient (above the horizon) --- */

    vec3 color;

    if (eyeDir.y >= 0.0) {
        float c = (1.0 - verticalAngle / (M_PI * 0.5));
        float skyGradient = clamp(1.0 - pow(1.0 - c, 1.0 / max(uSkyHorizonCurve, 0.001)), 0.0, 1.0);
        color = mix(uSkyHorizonColor, uSkyTopColor, skyGradient) * uSkyEnergy;
    }
    else {
        float c = (verticalAngle - (M_PI * 0.5)) / (M_PI * 0.5);
        float groundGradient = clamp(1.0 - pow(1.0 - c, 1.0 / max(uGroundHorizonCurve, 0.001)), 0.0, 1.0);
        color = mix(uGroundHorizonColor, uGroundBottomColor, groundGradient) * uGroundEnergy;
    }

    /* --- Sun contribution --- */

    float sunAngle = acos(dot(sunDir, eyeDir));

    if (sunAngle < uSunSize) {
        color = uSunColor * uSunEnergy;
    }
    else if (sunAngle < uSunSize * 10.0) {
        float c2 = (sunAngle - uSunSize) / (uSunSize * 10.0 - uSunSize);
        float sunFade = clamp(1.0 - pow(1.0 - c2, 1.0 / max(uSunCurve, 0.001)), 0.0, 1.0);
        color = mix(uSunColor * uSunEnergy, color, sunFade);
    }

    /* --- Output --- */

    FragColor = vec4(color, 1.0);
}
