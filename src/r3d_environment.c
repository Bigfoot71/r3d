/* r3d_environment.h -- R3D Environment Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_environment.h>
#include <raymath.h>

#include "./r3d_core_state.h"

// ========================================
// PUBLIC API
// ========================================

R3DAPI R3D_Environment* R3D_GetEnvironment(void)
{
	return &R3D.environment;
}

R3DAPI void R3D_SetEnvironment(const R3D_Environment* env)
{
	R3D.environment = *env;
}
