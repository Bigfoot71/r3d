# Sky Shaders

Sky shaders are used to procedurally generate skybox cubemaps. Unlike screen shaders, they do not process a rendered frame, they render each face of a cubemap from scratch.

## Table of Contents

- [Overview](#overview)
- [Entry Point](#entry-point)
- [Built-in Variables](#built-in-variables)
- [Helper Functions](#helper-functions)
- [Uniforms](#uniforms)
- [Usage](#usage)
- [Best Practices](#best-practices)
- [Quick Reference](#quick-reference)

---

## Overview

A sky shader runs once per texel of a cubemap to compute the sky color seen from that direction. R3D renders all six faces of the cubemap using an internal unit cube, calling your `fragment()` function for each pixel.

Sky shaders are not used during the normal frame rendering process, they are invoked explicitly to generate or update a `R3D_Cubemap`.

### Basic Example

```glsl
void fragment() {
    // Simple gradient sky: blue at horizon, dark at zenith
    float t = max(EYEDIR.y, 0.0);
    COLOR = mix(vec3(0.5, 0.7, 1.0), vec3(0.1, 0.2, 0.5), t);
}
```

### Loading a Shader

```c
// From file
R3D_SkyShader* shader = R3D_LoadSkyShader("sky.glsl");

// From memory
const char* code = "void fragment() { COLOR = vec3(0.2, 0.4, 0.8); }";
R3D_SkyShader* shader = R3D_LoadSkyShaderFromMemory(code);

// Don't forget to unload when done
R3D_UnloadSkyShader(shader);
```

---

## Entry Point

Sky shaders have only **one required entry point**: `fragment()`. There is no vertex stage, and consequently, no varyings.

### Fragment Stage

Runs once per cubemap texel to compute the sky color for that direction.

```glsl
void fragment() {
    // Use EYEDIR to determine sky color based on view direction
    vec3 sunDir = normalize(vec3(0.5, 0.8, 0.3));
    float sun = pow(max(dot(EYEDIR, sunDir), 0.0), 64.0);
    COLOR = mix(vec3(0.1, 0.3, 0.8), vec3(1.0, 0.9, 0.6), sun);
}
```

---

## Built-in Variables

Sky shaders provide built-in variables describing the current cubemap texel:

| Variable | Type | Description |
|----------|------|-------------|
| `POSITION` | `vec3` | Interpolated local position on the unit cube |
| `TEXCOORD` | `vec2` | 2D texture coordinates on the current cube face (0.0 to 1.0) |
| `EYEDIR` | `vec3` | Normalized direction vector into the cubemap |
| `FRAME_INDEX` | `int` | Index incremented at each frame |
| `TIME` | `float` | Time provided by raylib's `GetTime()` |
| `COLOR` | `vec3` | Output color (write to this) |

### Key Variable: `EYEDIR`

`EYEDIR` is the most commonly useful variable. It is the normalized version of `POSITION` and represents the 3D direction from the origin toward the current texel in the cubemap. Use it to:

- Determine sky color based on elevation (`EYEDIR.y`)
- Compute sun/moon angle via `dot(EYEDIR, lightDir)`
- Sample an equirectangular texture using the provided helper

```glsl
void fragment() {
    // Sky gradient based on elevation
    float horizon = smoothstep(-0.05, 0.1, EYEDIR.y);
    COLOR = mix(vec3(0.8, 0.6, 0.4), vec3(0.2, 0.4, 0.9), horizon);
}
```

### `TEXCOORD` vs `EYEDIR`

- Use `EYEDIR` for 3D directional effects (gradients, sun, stars, procedural atmosphere).
- Use `TEXCOORD` when you need 2D face-local coordinates, for example when sampling a per-face texture or applying a per-face pattern.

### `TIME` and `FRAME_INDEX`

These work identically to their equivalents in surface and screen shaders. `TIME` is useful for animating sky conditions (moving clouds, day/night cycle), and `FRAME_INDEX` can drive frame-dependent noise effects when updating the cubemap each frame.

---

## Helper Functions

Sky shaders include a built-in helper to convert a direction vector to equirectangular (spherical) UV coordinates:

```glsl
vec2 GetSphericalCoord(vec3 direction);
```

Returns UV coordinates suitable for sampling an equirectangular (panoramic) texture from a direction vector.

**Example:**

```glsl
uniform sampler2D u_panorama;

void fragment() {
    vec2 uv = GetSphericalCoord(EYEDIR);
    COLOR = texture(u_panorama, uv).rgb;
}
```

---

## Uniforms

Sky shaders support the same uniform system as surface and screen shaders, with identical limits and behavior.

### Supported Types

**Values:** `bool`, `int`, `float`, `vec2`, `vec3`, `vec4`, `mat2`, `mat3`, `mat4`

**Samplers:** `sampler1D`, `sampler2D`, `sampler3D`, `samplerCube`

### Limits

- **Maximum uniform values:** 16 by default (configurable via `R3D_MAX_SHADER_UNIFORMS`)
- **Maximum samplers:** 4 by default (configurable via `R3D_MAX_SHADER_SAMPLERS`)

### Setting Uniforms from C

```c
float time = GetTime();
R3D_SetSkyShaderUniform(shader, "u_time", &time);

Texture2D panorama = LoadTexture("sky.hdr");
R3D_SetSkyShaderSampler(shader, "u_panorama", panorama);
```

---

## Usage

Sky shaders are used exclusively through two functions:

```c
R3D_Cubemap R3D_GenCustomSky(int size, R3D_SkyShader* shader);
void R3D_UpdateCustomSky(R3D_Cubemap* cubemap, R3D_SkyShader* shader);
```

- **`R3D_GenCustomSky`** allocates a new cubemap and renders all six faces using the provided shader.
- **`R3D_UpdateCustomSky`** re-renders an existing cubemap in place.

### Example

```c
R3D_SkyShader* shader = R3D_LoadSkyShader("sky.glsl");

// Generate once at startup and set it to the environment
R3D_Cubemap sky = R3D_GenCustomSky(512, shader);
R3D_GetEnvironment()->background.sky = sky;

float time = 0.0f;

while (!WindowShouldClose()) {
    // Update sky every frame for animated effects
    time += GetFrameTime();
    R3D_SetSkyShaderUniform(shader, "u_time", &time);
    R3D_UpdateCustomSky(&sky, shader);

    R3D_Begin();
    // ... draw scene with sky cubemap ...
    R3D_End();
}

R3D_UnloadSkyShader(shader);
```

> **Note:** Updating a cubemap every frame can be expensive for large sizes. Consider updating at a lower frequency, or using a smaller size (e.g. 64–128) when details are not critical.

---

## Best Practices

### Performance

1. **Choose an appropriate cubemap size:** 64 is enough for smooth gradients and distant lighting; 256–512 is needed for sharp sun discs or detailed clouds.
2. **Update selectively:** Only call `R3D_UpdateCustomSky` when the sky actually changes (new time of day, weather, etc.).
3. **Avoid heavy loops:** Keep procedural noise and atmospheric scattering computations minimal or pre-baked into textures.
4. **Cache expensive values:** Pre-compute quantities like `dot(EYEDIR, sunDir)` once and reuse them.

### Quality

1. **Normalize directions before use:** `EYEDIR` is already normalized, but any derived direction (reflected rays, sun direction) should be explicitly normalized.
2. **Guard against negative `EYEDIR.y`:** When computing atmospheric effects based on elevation, clamp or check `EYEDIR.y` to avoid artifacts below the horizon.
3. **Use `GetSphericalCoord` for panoramas:** Manually computing spherical coordinates is error-prone; prefer the built-in helper.

### Organization

1. **One shader per sky type:** Separate procedural sky, starfield, and panoramic sky into distinct shaders.
2. **Document parameter ranges:** Comment expected ranges for uniforms like sun elevation or cloud density.

---

## Quick Reference

### Loading/Unloading
```c
R3D_SkyShader* R3D_LoadSkyShader(const char* filePath);
R3D_SkyShader* R3D_LoadSkyShaderFromMemory(const char* code);
void R3D_UnloadSkyShader(R3D_SkyShader* shader);
```

### Setting Uniforms
```c
void R3D_SetSkyShaderUniform(R3D_SkyShader* shader, const char* name, const void* value);
void R3D_SetSkyShaderSampler(R3D_SkyShader* shader, const char* name, Texture texture);
```

### Generating/Updating Cubemaps
```c
R3D_Cubemap R3D_GenCustomSky(int size, R3D_SkyShader* shader);
void R3D_UpdateCustomSky(R3D_Cubemap* cubemap, R3D_SkyShader* shader);
```

### Shader Structure
```glsl
uniform <type> <name>;      // Optional: uniforms

void fragment() {           // Required: fragment stage
    // Read: POSITION, TEXCOORD, EYEDIR, TIME, FRAME_INDEX
    // Helper: GetSphericalCoord(vec3 direction) -> vec2
    // Write: COLOR
}
```

### Built-in Variables
```glsl
// Input (read-only)
vec3 POSITION;      // Local position on the unit cube
vec2 TEXCOORD;      // UV coordinates on the current cube face [0..1]
vec3 EYEDIR;        // Normalized direction into the cubemap
int FRAME_INDEX;    // Frame counter
float TIME;         // Elapsed time

// Output (write)
vec3 COLOR;         // Output sky color for this texel
```

### Helper Functions
```glsl
vec2 GetSphericalCoord(vec3 direction); // Direction → equirectangular UV
```
