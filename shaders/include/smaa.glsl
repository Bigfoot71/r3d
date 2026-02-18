uniform vec4 uMetrics;

#define SMAA_GLSL_3
#define SMAA_RT_METRICS uMetrics
#define SMAA_PRESET_HIGH

#include "../external/SMAA.hlsl"
