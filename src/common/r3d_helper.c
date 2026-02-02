/* r3d_helper.c -- Common R3D Helpers
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_helper.h"
#include <r3d_config.h>
#include <raylib.h>

#ifdef _WIN32
#   define NOGDI
#   define NOUSER
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef near
#   undef far
#elif defined(__linux__) || defined(__APPLE__)
#   include <unistd.h>
#else
#   error "Oops, platform not supported by R3D"
#endif

// ========================================
// HELPER FUNCTIONS
// ========================================

int r3d_get_cpu_count(void)
{
    static int numCPUs = 0;

    if (numCPUs > 0) {
        return numCPUs;
    }

    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        numCPUs = sysinfo.dwNumberOfProcessors;
    #elif defined(__linux__) || defined(__APPLE__)
        numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    #endif

    if (numCPUs < 1) {
        R3D_TRACELOG(LOG_WARNING, "Failed to detect CPU count, defaulting to 1 thread");
        numCPUs = 1;
    }

    return numCPUs;
}
