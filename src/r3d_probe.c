/* r3d_probe.c -- R3D Probe Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_probe.h>
#include <r3d_config.h>
#include <raymath.h>
#include <stddef.h>

#include "./modules/r3d_env.h"

// ========================================
// HELPER MACROS
// ========================================

#define GET_PROBE_OR_RETURN(var_name, id, ...)  \
    r3d_env_probe_t* var_name;                  \
    do {                                        \
        var_name = r3d_env_probe_get(id);       \
        if (var_name == NULL) {                 \
            R3D_TRACELOG(LOG_ERROR, "Invalid Probe [ID %i] given to '%s'", id, __func__);  \
            return __VA_ARGS__;                 \
        }                                       \
    } while(0)

// ========================================
// PUBLIC API
// ========================================

R3D_Probe R3D_CreateProbe(R3D_ProbeFlag flags)
{
    return r3d_env_probe_new(flags);
}

void R3D_DestroyProbe(R3D_Probe id)
{
    r3d_env_probe_delete(id);
}

bool R3D_IsProbeExist(R3D_Probe id)
{
    return r3d_env_probe_is_valid(id);
}

R3D_ProbeFlag R3D_GetProbeType(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, 0);
    return probe->flags;
}

bool R3D_IsProbeActive(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, 0);
    return probe->enabled;
}

void R3D_SetProbeActive(R3D_Probe id, bool active)
{
    GET_PROBE_OR_RETURN(probe, id);

    if (probe->enabled == active) return;
    if (active) probe->state.sceneShouldBeUpdated = true;

    probe->enabled = active;
}

R3D_ProbeUpdateMode R3D_GetProbeUpdateMode(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, 0);
    return probe->state.updateMode;
}

void R3D_SetProbeUpdateMode(R3D_Probe id, R3D_ProbeUpdateMode mode)
{
    GET_PROBE_OR_RETURN(probe, id);
    probe->state.updateMode = mode;
}

bool R3D_GetProbeInterior(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, false);
    return probe->interior;
}

void R3D_SetProbeInterior(R3D_Probe id, bool active)
{
    GET_PROBE_OR_RETURN(probe, id);

    if (probe->interior != active) {
        probe->state.sceneShouldBeUpdated = true;
        probe->interior = active;
    }
}

bool R3D_GetProbeShadows(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, false);
    return probe->shadows;
}

void R3D_SetProbeShadows(R3D_Probe id, bool active)
{
    GET_PROBE_OR_RETURN(probe, id);

    if (probe->shadows != active) {
        probe->state.sceneShouldBeUpdated = true;
        probe->shadows = active;
    }
}

Vector3 R3D_GetProbePosition(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, (Vector3){0});
    return probe->position;
}

void R3D_SetProbePosition(R3D_Probe id, Vector3 position)
{
    GET_PROBE_OR_RETURN(probe, id);

    if (!Vector3Equals(probe->position, position)) {
        probe->state.matrixShouldBeUpdated = true;
        probe->state.sceneShouldBeUpdated = true;
        probe->position = position;
    }
}

float R3D_GetProbeRange(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, 0.0f);
    return probe->range;
}

void R3D_SetProbeRange(R3D_Probe id, float range)
{
    GET_PROBE_OR_RETURN(probe, id);

    if (probe->range != range) {
        probe->state.matrixShouldBeUpdated = true;
        probe->state.sceneShouldBeUpdated = true;
        probe->range = range;
    }
}

float R3D_GetProbeFalloff(R3D_Probe id)
{
    GET_PROBE_OR_RETURN(probe, id, 0.0f);
    return probe->falloff;
}

void R3D_SetProbeFalloff(R3D_Probe id, float falloff)
{
    GET_PROBE_OR_RETURN(probe, id);

    if (probe->falloff != falloff) {
        probe->state.sceneShouldBeUpdated = true;
        probe->falloff = falloff;
    }
}
