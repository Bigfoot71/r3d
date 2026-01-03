/* r3d_platform.h -- Platform definitions.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_API_H
#define R3D_API_H

#if defined(_WIN32)
#   if defined(__TINYC__)
#       define __declspec(x) __attribute__((x))
#   endif
#   if defined(R3D_BUILD_SHARED)
#       define R3DAPI __declspec(dllexport)
#   elif defined(R3D_USE_SHARED)
#       define R3DAPI __declspec(dllimport)
#   endif
#else
#   if defined(R3D_BUILD_SHARED)
#       define R3DAPI __attribute__((visibility("default")))
#   endif
#endif

#ifndef R3DAPI
#   define R3DAPI extern
#endif

#ifndef R3D_LITERAL
#   ifdef __cplusplus
#       define R3D_LITERAL(type) type
#   else
#       define R3D_LITERAL(type) (type)
#   endif
#endif

#endif // R3D_API_H
