/* r3d_environment.h -- R3D Environment Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_environment.h>
#include <raymath.h>

#include "./r3d_state.h"

// ========================================
// PUBLIC API
// ========================================

void R3D_SetBackgroundColor(Color color)
{
	R3D.env.backgroundColor.x = (float)color.r / 255;
	R3D.env.backgroundColor.y = (float)color.g / 255;
	R3D.env.backgroundColor.z = (float)color.b / 255;
	R3D.env.backgroundColor.w = (float)color.a / 255;
}

void R3D_SetAmbientColor(Color color)
{
	R3D.env.ambientColor = color;
	R3D.env.ambientLight.x = (float)color.r / 255 * R3D.env.ambientEnergy;
	R3D.env.ambientLight.y = (float)color.g / 255 * R3D.env.ambientEnergy;
	R3D.env.ambientLight.z = (float)color.b / 255 * R3D.env.ambientEnergy;
}

void R3D_SetAmbientEnergy(float energy)
{
	R3D.env.ambientEnergy = energy;
	R3D.env.ambientLight.x = (float)R3D.env.ambientColor.r / 255 * energy;
	R3D.env.ambientLight.y = (float)R3D.env.ambientColor.g / 255 * energy;
	R3D.env.ambientLight.z = (float)R3D.env.ambientColor.b / 255 * energy;
}

void R3D_EnableSkybox(R3D_Skybox skybox)
{
	R3D.env.sky = skybox;
	R3D.env.useSky = true;
}

void R3D_DisableSkybox(void)
{
	R3D.env.useSky = false;
}

void R3D_SetSkyboxRotation(float pitch, float yaw, float roll)
{
	R3D.env.quatSky = QuaternionFromEuler(pitch, yaw, roll);
}

Vector3 R3D_GetSkyboxRotation(void)
{
	return QuaternionToEuler(R3D.env.quatSky);
}

void R3D_SetSkyboxIntensity(float background, float ambient, float reflection)
{
	R3D.env.skyBackgroundIntensity = background;
	R3D.env.skyAmbientIntensity = ambient;
	R3D.env.skyReflectIntensity = reflection;
}

void R3D_GetSkyboxIntensity(float* background, float* ambient, float* reflection)
{
	if (background)	*background = R3D.env.skyBackgroundIntensity;
	if (ambient) *ambient = R3D.env.skyAmbientIntensity;
	if (reflection) *reflection = R3D.env.skyReflectIntensity;
}

void R3D_SetSSAO(bool enabled)
{
	R3D.env.ssaoEnabled = enabled;
}

bool R3D_GetSSAO(void)
{
	return R3D.env.ssaoEnabled;
}

void R3D_SetSSAORadius(float value)
{
	R3D.env.ssaoRadius = value;
}

float R3D_GetSSAORadius(void)
{
	return R3D.env.ssaoRadius;
}

void R3D_SetSSAOBias(float value)
{
	R3D.env.ssaoBias = value;
}

float R3D_GetSSAOBias(void)
{
	return R3D.env.ssaoBias;
}

void R3D_SetSSAOIterations(int value)
{
	R3D.env.ssaoIterations = value;
}

int R3D_GetSSAOIterations(void)
{
	return R3D.env.ssaoIterations;
}

void R3D_SetSSAOIntensity(float value)
{
	R3D.env.ssaoIntensity = value;
}

float R3D_GetSSAOIntensity(void)
{
	return R3D.env.ssaoIntensity;
}

void R3D_SetSSAOPower(float value)
{
	R3D.env.ssaoPower = value;
}

float R3D_GetSSAOPower(void)
{
	return R3D.env.ssaoPower;
}

void R3D_SetSSAOLightAffect(float value)
{
	R3D.env.ssaoLightAffect = value;
}

float R3D_GetSSAOLightAffect(void)
{
	return R3D.env.ssaoLightAffect;
}

void R3D_SetBloomMode(R3D_Bloom mode)
{
	R3D.env.bloomMode = mode;
}

R3D_Bloom R3D_GetBloomMode(void)
{
	return R3D.env.bloomMode;
}

void R3D_SetBloomLevels(int value)
{
	//R3D.env.bloomLevels = R3D.target.mipChainHs.count;
}

int R3D_GetBloomLevels(void)
{
	return R3D.env.bloomLevels;
}

void R3D_SetBloomIntensity(float value)
{
	R3D.env.bloomIntensity = value;
}

float R3D_GetBloomIntensity(void)
{
	return R3D.env.bloomIntensity;
}

void R3D_SetBloomFilterRadius(int value)
{
	R3D.env.bloomFilterRadius = value;
}

int R3D_GetBloomFilterRadius(void)
{
	return R3D.env.bloomFilterRadius;
}

void R3D_SetBloomThreshold(float value)
{
	R3D.env.bloomThreshold = value;

	r3d_calculate_bloom_prefilter_data();
}

float R3D_GetBloomThreshold(void)
{
	return R3D.env.bloomThreshold;
}

void R3D_SetBloomSoftThreshold(float value)
{
	R3D.env.bloomSoftThreshold = value;

	r3d_calculate_bloom_prefilter_data();
}

float R3D_GetBloomSoftThreshold(void)
{
	return R3D.env.bloomSoftThreshold;
}

void R3D_SetSSR(bool enabled)
{
	R3D.env.ssrEnabled = enabled;
}

bool R3D_GetSSR(void)
{
	return R3D.env.ssrEnabled;
}

void R3D_SetSSRMaxRaySteps(int maxRaySteps)
{
	R3D.env.ssrMaxRaySteps = maxRaySteps;
}

int R3D_GetSSRMaxRaySteps(void)
{
	return R3D.env.ssrMaxRaySteps;
}

void R3D_SetSSRBinarySearchSteps(int binarySearchSteps)
{
	R3D.env.ssrBinarySearchSteps = binarySearchSteps;
}

int R3D_GetSSRBinarySearchSteps(void)
{
	return R3D.env.ssrBinarySearchSteps;
}

void R3D_SetSSRRayMarchLength(float rayMarchLength)
{
	R3D.env.ssrRayMarchLength = rayMarchLength;
}

float R3D_GetSSRRayMarchLength(void)
{
	return R3D.env.ssrRayMarchLength;
}

void R3D_SetSSRDepthThickness(float depthThickness)
{
	R3D.env.ssrDepthThickness = depthThickness;
}

float R3D_GetSSRDepthThickness(void)
{
	return R3D.env.ssrDepthThickness;
}

void R3D_SetSSRDepthTolerance(float depthTolerance)
{
	R3D.env.ssrDepthTolerance = depthTolerance;
}

float R3D_GetSSRDepthTolerance(void)
{
	return R3D.env.ssrDepthTolerance;
}

void R3D_SetSSRScreenEdgeFade(float start, float end)
{
	R3D.env.ssrEdgeFadeStart = start;
	R3D.env.ssrEdgeFadeEnd = end;
}

void R3D_GetSSRScreenEdgeFade(float* start, float* end)
{
	if (start) *start = R3D.env.ssrEdgeFadeStart;
	if (end) *end = R3D.env.ssrEdgeFadeEnd;
}

void R3D_SetFogMode(R3D_Fog mode)
{
	R3D.env.fogMode = mode;
}

R3D_Fog R3D_GetFogMode(void)
{
	return R3D.env.fogMode;
}

void R3D_SetFogColor(Color color)
{
	R3D.env.fogColor.x = (float)color.r / 255;
	R3D.env.fogColor.y = (float)color.g / 255;
	R3D.env.fogColor.z = (float)color.b / 255;
}

Color R3D_GetFogColor(void)
{
	Color color = { 0 };
	color.r = (unsigned char)(R3D.env.fogColor.x * 255);
	color.g = (unsigned char)(R3D.env.fogColor.y * 255);
	color.b = (unsigned char)(R3D.env.fogColor.z * 255);
	color.a = 255;
	return color;
}

void R3D_SetFogStart(float value)
{
	R3D.env.fogStart = value;
}

float R3D_GetFogStart(void)
{
	return R3D.env.fogStart;
}

void R3D_SetFogEnd(float value)
{
	R3D.env.fogEnd = value;
}

float R3D_GetFogEnd(void)
{
	return R3D.env.fogEnd;
}

void R3D_SetFogDensity(float value)
{
	R3D.env.fogDensity = value;
}

float R3D_GetFogDensity(void)
{
	return R3D.env.fogDensity;
}

void R3D_SetFogSkyAffect(float value)
{
	R3D.env.fogSkyAffect = value;
}

float R3D_GetFogSkyAffect(void)
{
	return R3D.env.fogSkyAffect;
}

void R3D_SetTonemapMode(R3D_Tonemap mode)
{
	R3D.env.tonemapMode = mode;

	// NOTE: The output shader for this tonemap mode
	//       will be loaded during the next output pass
	//       in `R3D_End()`

	//if (R3D.shader.screen.output[mode].id == 0) {
	//	r3d_mod_shader_load_post_output(mode);
	//}
}

R3D_Tonemap R3D_GetTonemapMode(void)
{
	return R3D.env.tonemapMode;
}

void R3D_SetTonemapExposure(float value)
{
	R3D.env.tonemapExposure = value;
}

float R3D_GetTonemapExposure(void)
{
	return R3D.env.tonemapExposure;
}

void R3D_SetTonemapWhite(float value)
{
	R3D.env.tonemapWhite = value;
}

float R3D_GetTonemapWhite(void)
{
	return R3D.env.tonemapExposure;
}

void R3D_SetBrightness(float value)
{
	R3D.env.brightness = value;
}

float R3D_GetBrightness(void)
{
	return R3D.env.brightness;
}

void R3D_SetContrast(float value)
{
	R3D.env.contrast = value;
}

float R3D_GetContrast(void)
{
	return R3D.env.contrast;
}

void R3D_SetSaturation(float value)
{
	R3D.env.saturation = value;
}

float R3D_GetSaturation(void)
{
	return R3D.env.saturation;
}

void R3D_SetDofMode(R3D_Dof mode)
{
	R3D.env.dofMode = mode;
}

R3D_Dof R3D_GetDofMode(void)
{
	return R3D.env.dofMode;
}

void R3D_SetDofFocusPoint(float value)
{
	R3D.env.dofFocusPoint = value;
}

float R3D_GetDofFocusPoint(void)
{
	return R3D.env.dofFocusPoint;
}

void R3D_SetDofFocusScale(float value)
{
	// clamp value between 0.0 and 10.0
	R3D.env.dofFocusScale = Clamp(value, 0.0f, 10.0f);
}

float R3D_GetDofFocusScale(void)
{
	return R3D.env.dofFocusScale;
}

void R3D_SetDofMaxBlurSize(float value)
{
	R3D.env.dofMaxBlurSize = Clamp(value, 0.0f, 50.0f);
}

float R3D_GetDofMaxBlurSize(void)
{
	return R3D.env.dofMaxBlurSize;
}

void R3D_SetDofDebugMode(bool enabled)
{
	R3D.env.dofDebugMode = enabled;
}

bool R3D_GetDofDebugMode(void)
{
	return R3D.env.dofDebugMode;
}
