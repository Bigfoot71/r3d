#version 330 core

uniform sampler2D uMeasuredLogLumTex;
uniform sampler2D uPrevAutoExposureTex;

uniform float uDeltaTime;
uniform float uMinLogLum;
uniform float uMaxLogLum;
uniform float uSpeedUp;
uniform float uSpeedDown;
uniform float uExposureCompLog;

out vec4 FragColor;

void main()
{
    const float LOG_MIDDLE_GRAY = -1.7147984280919266; // log(0.18)

    float measuredLogLum = texelFetch(uMeasuredLogLumTex, ivec2(0, 0), 0).r;
    float prevLogLum = texelFetch(uPrevAutoExposureTex, ivec2(0, 0), 0).g;

    measuredLogLum = clamp(measuredLogLum, uMinLogLum, uMaxLogLum);

    float speed = measuredLogLum > prevLogLum ? uSpeedUp : uSpeedDown;
    float blend = 1.0 - exp(-uDeltaTime * speed);

    float adaptedLogLum = mix(prevLogLum, measuredLogLum, blend);
    float exposure = exp(LOG_MIDDLE_GRAY - adaptedLogLum + uExposureCompLog);

    FragColor = vec4(exposure, adaptedLogLum, 0.0, 1.0);
}
