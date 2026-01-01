#ifndef R3D_DETAILS_MACROS_H
#define R3D_DETAILS_MACROS_H

#ifndef MIN
#   define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#   define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef CLAMP
#   define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))
#endif

#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#endif

#endif // R3D_DETAILS_MACROS_H
