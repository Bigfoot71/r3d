/* r3d_lighting.h -- R3D Lighting Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_lighting.h>
#include <raymath.h>
#include <rlgl.h>

#include "./details/r3d_light.h"
#include "./r3d_state.h"

// ========================================
// HELPER MACROS
// ========================================

#define r3d_get_and_check_light(var_name, id, ...)                  \
    r3d_light_t* var_name;                                          \
{                                                                   \
    var_name = r3d_registry_get(&R3D.container.rLights, id);  \
    if (light == NULL) {                                            \
        TraceLog(LOG_ERROR, "Light [ID %i] is not valid", id);      \
        return __VA_ARGS__;                                         \
    }                                                               \
}

// ========================================
// PUBLIC API
// ========================================

R3D_Light R3D_CreateLight(R3D_LightType type)
{
    R3D_Light id = r3d_registry_add(&R3D.container.rLights, NULL);
    r3d_light_t* light = r3d_registry_get(&R3D.container.rLights, id);

    r3d_light_init(light, type);

    return id;
}

void R3D_DestroyLight(R3D_Light id)
{
    r3d_get_and_check_light(light, id);

    if (light->shadow.map.id != 0) {
        r3d_light_destroy_shadow_map(light);
    }

    r3d_registry_remove(&R3D.container.rLights, id);
}

bool R3D_IsLightExist(R3D_Light id)
{
    r3d_get_and_check_light(light, id, false);
    return true;
}

R3D_LightType R3D_GetLightType(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->type;
}

bool R3D_IsLightActive(R3D_Light id)
{
    r3d_get_and_check_light(light, id, false);
    return light->enabled;
}

void R3D_ToggleLight(R3D_Light id)
{
    r3d_get_and_check_light(light, id);
    light->enabled = !light->enabled;

    if (light->enabled && light->shadow.enabled) {
        light->shadow.updateConf.shouldUpdate = true;
    }
}

void R3D_SetLightActive(R3D_Light id, bool active)
{
    r3d_get_and_check_light(light, id);

    if (light->enabled == active) {
        return;
    }

    if (active && light->shadow.enabled) {
        light->shadow.updateConf.shouldUpdate = true;
    }

    light->enabled = active;
}

Color R3D_GetLightColor(R3D_Light id)
{
    r3d_get_and_check_light(light, id, BLANK);
    return (Color) {
        (unsigned char)Clamp(light->color.x * 255, 0, 255),
        (unsigned char)Clamp(light->color.y * 255, 0, 255),
        (unsigned char)Clamp(light->color.z * 255, 0, 255),
        255
    };
}

Vector3 R3D_GetLightColorV(R3D_Light id)
{
    r3d_get_and_check_light(light, id, (Vector3) { 0 });
    return light->color;
}

void R3D_SetLightColor(R3D_Light id, Color color)
{
    r3d_get_and_check_light(light, id);
    light->color.x = color.r / 255.0f;
    light->color.y = color.g / 255.0f;
    light->color.z = color.b / 255.0f;
}

void R3D_SetLightColorV(R3D_Light id, Vector3 color)
{
    r3d_get_and_check_light(light, id);
    light->color = color;
}

Vector3 R3D_GetLightPosition(R3D_Light id)
{
    r3d_get_and_check_light(light, id, (Vector3) { 0 });
    return light->position;
}

void R3D_SetLightPosition(R3D_Light id, Vector3 position)
{
    r3d_get_and_check_light(light, id);
    if (light->type == R3D_LIGHT_DIR) {
        TraceLog(LOG_WARNING, "R3D: Can't set position for light [ID %i]; it's directional and position is set automatically", id);
        return;
    }
    light->position = position;
}

Vector3 R3D_GetLightDirection(R3D_Light id)
{
    r3d_get_and_check_light(light, id, (Vector3) { 0 });
    return light->direction;
}

void R3D_SetLightDirection(R3D_Light id, Vector3 direction)
{
    r3d_get_and_check_light(light, id);
    if (light->type == R3D_LIGHT_OMNI) {
        TraceLog(LOG_WARNING, "R3D: Can't set direction for light [ID %i]; it's omni-directional and doesn't have a direction", id);
        return;
    }
    light->direction = Vector3Normalize(direction);
}

void R3D_LightLookAt(R3D_Light id, Vector3 position, Vector3 target)
{
    r3d_get_and_check_light(light, id);
    if (light->type != R3D_LIGHT_OMNI) {
        light->direction = Vector3Normalize(Vector3Subtract(target, position));
    }
    if (light->type != R3D_LIGHT_DIR) {
        light->position = position;
    }
}

float R3D_GetLightEnergy(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->energy;
}

void R3D_SetLightEnergy(R3D_Light id, float energy)
{
    r3d_get_and_check_light(light, id);
    light->energy = energy;
}

float R3D_GetLightSpecular(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->specular;
}

void R3D_SetLightSpecular(R3D_Light id, float specular)
{
    r3d_get_and_check_light(light, id);
    light->specular = specular;
}

float R3D_GetLightRange(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->range;
}

void R3D_SetLightRange(R3D_Light id, float range)
{
    r3d_get_and_check_light(light, id);
    light->range = range;
}

float R3D_GetLightAttenuation(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return 1.0f / light->attenuation;
}

void R3D_SetLightAttenuation(R3D_Light id, float attenuation)
{
    r3d_get_and_check_light(light, id);
    light->attenuation = (attenuation > 1e-4f) ? 1.0f / attenuation : 10000.0f;
}

float R3D_GetLightInnerCutOff(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return acosf(light->innerCutOff) * RAD2DEG;
}

void R3D_SetLightInnerCutOff(R3D_Light id, float degrees)
{
    r3d_get_and_check_light(light, id);
    light->innerCutOff = cosf(degrees * DEG2RAD);
}

float R3D_GetLightOuterCutOff(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return acosf(light->outerCutOff) * RAD2DEG;
}

void R3D_SetLightOuterCutOff(R3D_Light id, float degrees)
{
    r3d_get_and_check_light(light, id);
    light->outerCutOff = cosf(degrees * DEG2RAD);
}

void R3D_EnableShadow(R3D_Light id, int resolution)
{
    r3d_get_and_check_light(light, id);

    if (light->shadow.map.id != 0) {
        if (resolution > 0 && light->shadow.map.resolution != resolution) {
            r3d_light_destroy_shadow_map(light);
            r3d_light_create_shadow_map(light, resolution);
        }
    }
    else {
        if (resolution == 0) resolution = 1024;
        r3d_light_create_shadow_map(light, resolution);
    }

    light->shadow.enabled = true;
}

void R3D_DisableShadow(R3D_Light id, bool destroyMap)
{
    r3d_get_and_check_light(light, id);

    if (destroyMap) {
        r3d_light_destroy_shadow_map(light);
        memset(&light->shadow.map, 0, sizeof(r3d_shadow_map_t));
    }

    light->shadow.enabled = false;
}

bool R3D_IsShadowEnabled(R3D_Light id)
{
    r3d_get_and_check_light(light, id, false);
    return light->shadow.enabled;
}

bool R3D_HasShadowMap(R3D_Light id)
{
    r3d_get_and_check_light(light, id, false);
    return light->shadow.map.id != 0;
}

R3D_ShadowUpdateMode R3D_GetShadowUpdateMode(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->shadow.updateConf.mode;
}

void R3D_SetShadowUpdateMode(R3D_Light id, R3D_ShadowUpdateMode mode)
{
    r3d_get_and_check_light(light, id);
    light->shadow.updateConf.mode = mode;
}

int R3D_GetShadowUpdateFrequency(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return (int)(light->shadow.updateConf.frequencySec * 1000);
}

void R3D_SetShadowUpdateFrequency(R3D_Light id, int msec)
{
    r3d_get_and_check_light(light, id);
    light->shadow.updateConf.frequencySec = (float)msec / 1000;

}

void R3D_UpdateShadowMap(R3D_Light id)
{
    r3d_get_and_check_light(light, id);
    light->shadow.updateConf.shouldUpdate = true;
}

float R3D_GetShadowSoftness(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->shadow.softness;
}

void R3D_SetShadowSoftness(R3D_Light id, float softness)
{
    r3d_get_and_check_light(light, id);
    light->shadow.softness = softness * light->shadow.map.texelSize;
}

float R3D_GetShadowDepthBias(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->shadow.depthBias;
}

void R3D_SetShadowDepthBias(R3D_Light id, float value)
{
    r3d_get_and_check_light(light, id);
    light->shadow.depthBias = value;
}

float R3D_GetShadowSlopeBias(R3D_Light id)
{
    r3d_get_and_check_light(light, id, 0);
    return light->shadow.slopeBias;
}

void R3D_SetShadowSlopeBias(R3D_Light id, float value)
{
    r3d_get_and_check_light(light, id);
    light->shadow.slopeBias = value;
}

BoundingBox R3D_GetLightBoundingBox(R3D_Light id)
{
    r3d_get_and_check_light(light, id, (BoundingBox) { 0 });
    return r3d_light_get_bounding_box(light);
}

void R3D_DrawLightShape(R3D_Light id)
{
    r3d_get_and_check_light(light, id);

    if (light->type == R3D_LIGHT_DIR) {
        return;
    }

    Color color = {
        (unsigned char)(light->color.x * 255),
        (unsigned char)(light->color.y * 255),
        (unsigned char)(light->color.z * 255),
        100
    };

    DrawSphereEx(light->position, 0.25f, 4, 8, color);

    if (light->type == R3D_LIGHT_SPOT)
    {
        // Calculation of the circle radius (bottom of the spotlight) and the position of the base
        float radius = fabsf(light->range * light->outerCutOff);
        Vector3 spotDir = R3D_GetLightDirection(1);
        Vector3 basePos = Vector3Add(light->position, Vector3Scale(spotDir, light->range));

        // Calculation of an orthonormal reference frame for the circle plane
        Vector3 right = { 0 };
        if (fabsf(spotDir.x) < fabsf(spotDir.y) && fabsf(spotDir.x) < fabsf(spotDir.z)) {
            right = (Vector3){ 0, -spotDir.z, spotDir.y };
        }
        else if (fabsf(spotDir.y) < fabsf(spotDir.z)) {
            right = (Vector3){ -spotDir.z, 0, spotDir.x };
        }
        else {
            right = (Vector3){ -spotDir.y, spotDir.x, 0 };
        }

        right = Vector3Normalize(right);
        Vector3 up = Vector3CrossProduct(spotDir, right);
        up = Vector3Normalize(up);

        // Calculation of the 4 cardinal points on the circle (angles 0, 90, 180, and 270 degrees)
        Vector3 circlePoints[4];
        float angles[4] = { 0, DEG2RAD * 90.0f, DEG2RAD * 180.0f, DEG2RAD * 270.0f };
        for (int i = 0; i < 4; i++) {
            circlePoints[i].x = basePos.x + radius * (cosf(angles[i]) * right.x + sinf(angles[i]) * up.x);
            circlePoints[i].y = basePos.y + radius * (cosf(angles[i]) * right.y + sinf(angles[i]) * up.y);
            circlePoints[i].z = basePos.z + radius * (cosf(angles[i]) * right.z + sinf(angles[i]) * up.z);
        }

        // Draw 4 lines from the light position to each of the 4 points on the circle
        rlBegin(RL_LINES);
        {
            rlColor4ub(color.r, color.g, color.b, color.a);
            for (int i = 0; i < 4; i++) {
                rlVertex3f(light->position.x, light->position.y, light->position.z);
                rlVertex3f(circlePoints[i].x, circlePoints[i].y, circlePoints[i].z);
            }
        }
        rlEnd();

        // Draw the circle at the base with 32 segments
        rlBegin(RL_LINES);
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            const int SEGMENTS = 32;
            const float STEP = 360.0f / SEGMENTS;

            for (int i = 0; i < SEGMENTS; i++) {
                float angle1 = i * STEP * DEG2RAD;
                float angle2 = (i + 1) * STEP * DEG2RAD;

                Vector3 p1 = {
                    basePos.x + radius * (cosf(angle1) * right.x + sinf(angle1) * up.x),
                    basePos.y + radius * (cosf(angle1) * right.y + sinf(angle1) * up.y),
                    basePos.z + radius * (cosf(angle1) * right.z + sinf(angle1) * up.z)
                };
                Vector3 p2 = {
                    basePos.x + radius * (cosf(angle2) * right.x + sinf(angle2) * up.x),
                    basePos.y + radius * (cosf(angle2) * right.y + sinf(angle2) * up.y),
                    basePos.z + radius * (cosf(angle2) * right.z + sinf(angle2) * up.z)
                };

                rlVertex3f(p1.x, p1.y, p1.z);
                rlVertex3f(p2.x, p2.y, p2.z);
            }
        }
        rlEnd();
    }
    else if (light->type == R3D_LIGHT_OMNI)
    {
        const int SEGMENTS = 32;
        const float STEP = 360.0f / SEGMENTS;

        rlBegin(RL_LINES);
        {
            rlColor4ub(color.r, color.g, color.b, color.a);

            for (int i = 0; i < SEGMENTS; i++) {
                float angle1 = i * STEP * DEG2RAD;
                float angle2 = (i + 1) * STEP * DEG2RAD;

                // Circle around X-axis (YZ plane)
                rlVertex3f(light->position.x, light->position.y + light->range * cosf(angle1), light->position.z + light->range * sinf(angle1));
                rlVertex3f(light->position.x, light->position.y + light->range * cosf(angle2), light->position.z + light->range * sinf(angle2));

                // Circle around Y-axis (XZ plane)
                rlVertex3f(light->position.x + light->range * cosf(angle1), light->position.y, light->position.z + light->range * sinf(angle1));
                rlVertex3f(light->position.x + light->range * cosf(angle2), light->position.y, light->position.z + light->range * sinf(angle2));

                // Circle around Z-axis (XY plane)
                rlVertex3f(light->position.x + light->range * cosf(angle1), light->position.y + light->range * sinf(angle1), light->position.z);
                rlVertex3f(light->position.x + light->range * cosf(angle2), light->position.y + light->range * sinf(angle2), light->position.z);
            }
        }
        rlEnd();
    }
}
