/* r3d_lighting.c -- R3D Lighting Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_lighting.h>
#include <r3d/r3d_color.h>
#include <r3d_config.h>
#include <raymath.h>
#include <stddef.h>
#include <rlgl.h>

#include "./modules/r3d_light.h"
#include "./common/r3d_math.h"

// ========================================
// HELPER MACROS
// ========================================

#define GET_LIGHT_OR_RETURN(var_name, id, ...)  \
    r3d_light_t* var_name;                      \
    do {                                        \
        var_name = r3d_light_get(id);           \
        if (var_name == NULL) {                 \
            R3D_TRACELOG(LOG_ERROR, "Invalid light [ID %i] given to '%s'", id, __func__);  \
            return __VA_ARGS__;                 \
        }                                       \
    } while(0)

// ========================================
// PUBLIC API
// ========================================

// ----------------------------------------
// Lights Config Functions
// ----------------------------------------

R3D_Light R3D_CreateLight(R3D_LightType type)
{
    return r3d_light_new(type);
}

void R3D_DestroyLight(R3D_Light id)
{
    r3d_light_delete(id);
}

bool R3D_IsLightValid(R3D_Light id)
{
    return r3d_light_is_valid(id);
}

R3D_LightType R3D_GetLightType(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->type;
}

bool R3D_IsLightEnabled(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, false);
    return light->enabled;
}

void R3D_ToggleLight(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->enabled = !light->enabled;

    if (light->enabled && light->shadowLayer >= 0) {
        light->state.shadowShouldBeUpdated = true;
    }
}

void R3D_EnableLight(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);
    if (light->enabled) return;

    if (light->shadowLayer >= 0) {
        light->state.shadowShouldBeUpdated = true;
    }
    light->enabled = true;
}

void R3D_DisableLight(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);
    if (!light->enabled) return;
    light->enabled = false;
}

Color R3D_GetLightColor(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, BLANK);
    return r3d_color_linear_to_srgb_vec3(light->color);
}

void R3D_SetLightColor(R3D_Light id, Color color)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->color = r3d_color_srgb_to_linear_vec3(color);
}

Vector3 R3D_GetLightColorLinear(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, (Vector3) {0});
    return light->color;
}

void R3D_SetLightColorLinear(R3D_Light id, Vector3 color)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->color = color;
}

void R3D_SetLightTemperature(R3D_Light id, float kelvin)
{
    R3D_SetLightColor(id, R3D_ColorFromTemperature(kelvin));
}

Vector3 R3D_GetLightPosition(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, (Vector3) {0});
    return light->position;
}

void R3D_SetLightPosition(R3D_Light id, Vector3 position)
{
    GET_LIGHT_OR_RETURN(light, id);

    if (Vector3Equals(position, light->position)) {
        return;
    }

    // Position is dummy and unused for directional lights
    if (light->type != R3D_LIGHT_DIR) {
        light->state.matrixShouldBeUpdated = true;
    }
    light->position = position;
}

Vector3 R3D_GetLightDirection(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, (Vector3) {0});
    return light->direction;
}

void R3D_SetLightDirection(R3D_Light id, Vector3 direction)
{
    GET_LIGHT_OR_RETURN(light, id);

    if (light->type == R3D_LIGHT_OMNI) {
        R3D_TRACELOG(LOG_WARNING, "Can't set direction for light [ID %i]; it's omni-directional and doesn't have a direction", id);
        return;
    }

    light->state.matrixShouldBeUpdated = true;
    light->direction = Vector3Normalize(direction);
}

void R3D_SetLightTarget(R3D_Light id, Vector3 position, Vector3 target)
{
    GET_LIGHT_OR_RETURN(light, id);

    if (light->type != R3D_LIGHT_OMNI) {
        light->direction = Vector3Normalize(Vector3Subtract(target, position));
    }
    light->state.matrixShouldBeUpdated = true;
    light->position = position;
}

float R3D_GetLightEnergy(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->energy;
}

void R3D_SetLightEnergy(R3D_Light id, float energy)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->energy = energy;
}

float R3D_GetLightLumen(R3D_Light id)
{
    return R3D_EnergyToLumens(R3D_GetLightEnergy(id), 1.0f);
}

void R3D_SetLightLumen(R3D_Light id, float lumens)
{
    R3D_SetLightEnergy(id, R3D_LumensToEnergy(lumens, 1.0f));
}

float R3D_GetLightSpecular(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->specular;
}

void R3D_SetLightSpecular(R3D_Light id, float specular)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->specular = specular;
}

float R3D_GetLightRange(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->range;
}

void R3D_SetLightRange(R3D_Light id, float range)
{
    GET_LIGHT_OR_RETURN(light, id);

    light->state.matrixShouldBeUpdated = true;
    light->range = range;
}

float R3D_GetLightFalloff(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->falloff;
}

void R3D_SetLightFalloff(R3D_Light id, float falloff)
{
    GET_LIGHT_OR_RETURN(light, id);
    if (light->type == R3D_LIGHT_DIR) {
        R3D_TRACELOG(LOG_WARNING, "Can't set falloff for light [ID %i]; it's directional and doesn't have falloff", id);
        return;
    }
    light->falloff = (falloff <= 0.0f) ? 1.0f : falloff;
}

void R3D_GetLightAngle(R3D_Light id, float* inner, float* outer)
{
    GET_LIGHT_OR_RETURN(light, id);

    if (inner) *inner = acosf(light->innerCutOff) * RAD2DEG;
    if (outer) *outer = acosf(light->outerCutOff) * RAD2DEG;
}

void R3D_SetLightAngle(R3D_Light id, float inner, float outer)
{
    GET_LIGHT_OR_RETURN(light, id);
    if (light->type == R3D_LIGHT_DIR || light->type == R3D_LIGHT_OMNI) {
        R3D_TRACELOG(LOG_WARNING, "Can't set angle for light [ID %i]; it's directional or omni and doesn't have angle attenuation", id);
        return;
    }

    if (inner > outer) {
        float tmp = inner;
        inner = outer;
        outer = tmp;
    }

    float i = cosf(inner * DEG2RAD);
    float o = cosf(outer * DEG2RAD);

    if (fabsf(o - light->outerCutOff) > 1e-4f) {
        light->state.matrixShouldBeUpdated = true;
    }

    light->innerCutOff = i;
    light->outerCutOff = o;
}

float R3D_GetLightFogEnergy(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0.0f);
    return light->fogEnergy;
}

void R3D_SetLightFogEnergy(R3D_Light id, float energy)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->fogEnergy = energy;
}

// ----------------------------------------
// Shadow Config Functions
// ----------------------------------------

bool R3D_IsShadowEnabled(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, false);
    return light->shadowLayer >= 0;
}

void R3D_EnableShadow(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);

    if (!r3d_light_enable_shadows(light)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to enable shadows for light [ID %i]", id);
    }
}

void R3D_DisableShadow(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);
    r3d_light_disable_shadows(light);
}

R3D_ShadowUpdateMode R3D_GetShadowUpdateMode(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->state.shadowUpdate;
}

void R3D_SetShadowUpdateMode(R3D_Light id, R3D_ShadowUpdateMode mode)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->state.shadowUpdate = mode;
}

float R3D_GetShadowUpdateInterval(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0.0f);
    return light->state.shadowUpdateInterval;
}

void R3D_SetShadowUpdateInterval(R3D_Light id, float sec)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->state.shadowUpdateInterval = sec;
}

void R3D_UpdateShadowMap(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->state.shadowShouldBeUpdated = true;
}

float R3D_GetShadowSoftness(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->shadowSoftness * r3d_light_shadow_get_size(light->type);
}

void R3D_SetShadowSoftness(R3D_Light id, float softness)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->shadowSoftness = softness / r3d_light_shadow_get_size(light->type);
}

float R3D_GetShadowOpacity(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0.0f);
    return light->shadowOpacity;
}

void R3D_SetShadowOpacity(R3D_Light id, float opacity)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->shadowOpacity = opacity;
}

float R3D_GetShadowDepthBias(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->shadowDepthBias;
}

void R3D_SetShadowDepthBias(R3D_Light id, float value)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->shadowDepthBias = value;
}

float R3D_GetShadowSlopeBias(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->shadowSlopeBias;
}

void R3D_SetShadowSlopeBias(R3D_Light id, float value)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->shadowSlopeBias = value;
}

R3D_Layer R3D_GetShadowCasterMask(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, 0);
    return light->casterMask;
}

void R3D_SetShadowCasterMask(R3D_Light id, R3D_Layer cullMask)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->casterMask = cullMask;
}

void R3D_EnableShadowCasterLayers(R3D_Light id, R3D_Layer layerMask)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->casterMask |= layerMask;
}

void R3D_DisableShadowCasterLayers(R3D_Light id, R3D_Layer layerMask)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->casterMask &= ~layerMask;
}

void R3D_ToggleShadowCasterLayers(R3D_Light id, R3D_Layer layerMask)
{
    GET_LIGHT_OR_RETURN(light, id);
    light->casterMask ^= layerMask;
}

bool R3D_IsShadowCasterLayerVisible(R3D_Light id, R3D_Layer layerMask)
{
    GET_LIGHT_OR_RETURN(light, id, false);
    return (light->casterMask & layerMask) != 0;
}

// ----------------------------------------
// Light Helper Functions
// ----------------------------------------

BoundingBox R3D_GetLightBoundingBox(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id, (BoundingBox) {0});
    return light->aabb;
}

static void r3d_draw_light_dir_debug(const r3d_light_t* light, Color color)
{
    // Arrow parameters
    const float ARROW_LENGTH  = 1.5f;
    const float ARROW_HEAD    = 0.3f;
    const float ARROW_SPREAD  = 0.5f;
    const int   GRID_HALF     = 1;     // [-1, 0, 1] -> 3x3 grid

    Vector3 dir = Vector3Normalize(light->direction);

    // Build orthonormal basis around direction
    Vector3 ref = (fabsf(dir.y) < 0.999f) ? (Vector3){0,1,0} : (Vector3){1,0,0};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, ref));
    Vector3 up = Vector3CrossProduct(right, dir);

    Vector3 origin = light->position;

    for (int i = -GRID_HALF; i <= GRID_HALF; i++) {
        for (int j = -GRID_HALF; j <= GRID_HALF; j++) {
            // Offset arrow origin on the plane perpendicular to direction
            Vector3 offset = Vector3Add(
                Vector3Scale(right, (float)i * ARROW_SPREAD),
                Vector3Scale(up, (float)j * ARROW_SPREAD)
            );
            Vector3 from = Vector3Add(origin, offset);
            Vector3 to = Vector3Add(from, Vector3Scale(dir, ARROW_LENGTH));

            // Arrow shaft
            DrawLine3D(from, to, color);

            // Arrow head; 4 lines forming a cross-cap
            Vector3 head_base = Vector3Add(from, Vector3Scale(dir, ARROW_LENGTH - ARROW_HEAD));
            DrawLine3D(head_base, Vector3Add(head_base, Vector3Scale(right, ARROW_HEAD * 0.5f)), color);
            DrawLine3D(head_base, Vector3Add(head_base, Vector3Scale(right, -ARROW_HEAD * 0.5f)), color);
            DrawLine3D(head_base, Vector3Add(head_base, Vector3Scale(up, ARROW_HEAD * 0.5f)), color);
            DrawLine3D(head_base, Vector3Add(head_base, Vector3Scale(up, -ARROW_HEAD * 0.5f)), color);
        }
    }
}

static void r3d_draw_light_spot_debug(const r3d_light_t* light, Color color)
{
    const int SEGMENTS = 32;

    Vector3 pos = light->position;
    Vector3 dir = Vector3Normalize(light->direction);

    // Build orthonormal basis
    Vector3 ref = (fabsf(dir.y) < 0.999f) ? (Vector3){0,1,0} : (Vector3){1,0,0};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(dir, ref));
    Vector3 up = Vector3CrossProduct(right, dir);

    // Draw inner and outer cone rings + lines from apex
    float cutoffs[2] = {light->innerCutOff, light->outerCutOff};
    for (int c = 0; c < 2; c++) {
        float radius = fabsf(light->range * cutoffs[c]);
        Vector3 base = Vector3Add(pos, Vector3Scale(dir, light->range));

        // Ring
        rlBegin(RL_LINES);
        rlColor4ub(color.r, color.g, color.b, color.a);
        const float step = (2.0f * PI) / SEGMENTS;
        for (int i = 0; i < SEGMENTS; i++) {
            float a1 = i * step, a2 = (i + 1) * step;
            Vector3 p1 = Vector3Add(base, Vector3Add(
                Vector3Scale(right, cosf(a1) * radius),
                Vector3Scale(up,    sinf(a1) * radius)));
            Vector3 p2 = Vector3Add(base, Vector3Add(
                Vector3Scale(right, cosf(a2) * radius),
                Vector3Scale(up,    sinf(a2) * radius)));
            rlVertex3f(p1.x, p1.y, p1.z);
            rlVertex3f(p2.x, p2.y, p2.z);
        }
        rlEnd();

        // 4 lines from apex to ring (cardinal points only)
        float angles[4] = { 0, PI * 0.5f, PI, PI * 1.5f };
        for (int i = 0; i < 4; i++) {
            Vector3 rim = Vector3Add(base, Vector3Add(
                Vector3Scale(right, cosf(angles[i]) * radius),
                Vector3Scale(up,    sinf(angles[i]) * radius)));
            DrawLine3D(pos, rim, color);
        }
    }

    // Small sphere at apex
    DrawSphereEx(pos, 0.05f, 4, 8, color);
}

static void r3d_draw_light_omni_debug(const r3d_light_t* light, Color color)
{
    const int   SEGMENTS = 32;
    const float STEP     = (2.0f * PI) / SEGMENTS;

    Vector3 pos = light->position;
    float range = light->range;

    // 3 orthogonal circles (XY, XZ, YZ planes)
    rlBegin(RL_LINES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    for (int i = 0; i < SEGMENTS; i++) {
        float a1 = i * STEP, a2 = (i + 1) * STEP;
        float c1 = cosf(a1) * range, s1 = sinf(a1) * range;
        float c2 = cosf(a2) * range, s2 = sinf(a2) * range;

        // XY plane
        rlVertex3f(pos.x + c1, pos.y + s1, pos.z);
        rlVertex3f(pos.x + c2, pos.y + s2, pos.z);
        // XZ plane
        rlVertex3f(pos.x + c1, pos.y, pos.z + s1);
        rlVertex3f(pos.x + c2, pos.y, pos.z + s2);
        // YZ plane
        rlVertex3f(pos.x, pos.y + c1, pos.z + s1);
        rlVertex3f(pos.x, pos.y + c2, pos.z + s2);
    }
    rlEnd();

    // Small sphere at center
    DrawSphereEx(pos, 0.05f, 4, 8, color);
}

void R3D_DrawLightDebug(R3D_Light id)
{
    GET_LIGHT_OR_RETURN(light, id);

    Color color = {
        (uint8_t)(light->color.x * 255),
        (uint8_t)(light->color.y * 255),
        (uint8_t)(light->color.z * 255),
        200
    };

    switch (light->type) {
    case R3D_LIGHT_DIR:
        r3d_draw_light_dir_debug(light, color);
        break;
    case R3D_LIGHT_SPOT:
        r3d_draw_light_spot_debug(light, color);
        break;
    case R3D_LIGHT_OMNI:
        r3d_draw_light_omni_debug(light, color);
        break;
    default:
        break;
    }
}

// ----------------------------------------
// LIGHTING: Math Helper Functions
// ----------------------------------------

float R3D_LumensToEnergy(float lumens, float referenceDistance)
{
    return lumens / (4.0f * PI * referenceDistance * referenceDistance);
}

float R3D_EnergyToLumens(float energy, float referenceDistance)
{
    return energy * (4.0f * PI * referenceDistance * referenceDistance);
}
