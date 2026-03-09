---
name: Bug report
about: Report a reproducible bug in r3d
title: "[BUG] ..."
labels: bug
assignees: Bigfoot71

---

Before submitting an issue, please make sure:
- the problem is reproducible
- you tested with the latest version of r3d if possible
- you included a minimal reproducible example
- you included logs if this looks like a graphics/driver issue

Incomplete bug reports may be closed.

---

## Describe the bug

A clear and concise description of what the bug is.

## Steps to reproduce

Steps to reproduce the behavior.

## Minimal reproducible example

Please provide the **smallest possible self-contained example** that reproduces the issue.
If you cannot provide a minimal example, explain why.

```c
#include <r3d/r3d.h>

int main(void)
{
    InitWindow(800, 600, "r3d bug-repro");
    SetTargetFPS(60);

    R3D_Init(800, 600);

    Camera3D camera = {
        .position = {5, 5, 5},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
        R3D_Begin(camera);

        // rendering code that triggers the issue

        R3D_End();
        EndDrawing();
    }

    R3D_Close();
    CloseWindow();

    return 0;
}
```

## Expected behavior

A clear and concise description of what you expected to happen.

## Actual behavior

Describe what actually happens instead.

## Screenshots / Videos

If applicable, add screenshots or videos to help explain the problem.

## Environment

Please complete the following information:

**r3d version**

- Version / tag / commit:

**raylib version**

- Version:

**Operating System**

- OS and version (e.g. Windows 11, Ubuntu 24.04, macOS 14):

**Compiler**

- e.g. gcc 13, clang 17, MSVC 19.x

## Graphics information (important for rendering issues)

If this is a rendering or GPU-related issue, please include:

**GPU**

- e.g. NVIDIA RTX 3070 / AMD RX 6800 / Intel UHD 770

**Driver version**

- e.g. 551.86

**Graphics backend**

- e.g. OpenGL 3.3 / OpenGL ES 3.0

Or paste the initialization logs, raylib prints useful GPU/backend information:

```
INFO: GL: OpenGL device information:
INFO:     > Vendor:   ...
INFO:     > Renderer: ...
INFO:     > Version:  ...
INFO:     > GLSL:     ...
```

## Additional context

Add any other context about the problem here.

Examples:

- Does the issue happen only in debug/release?
- Does it happen only on a specific GPU?
- Did it start after a specific commit?
- Any workaround found?
