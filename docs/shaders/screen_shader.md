# Screen Shaders

Screen shaders are post-processing effects applied to the entire rendered frame.
Unlike surface shaders, they operate on the final image rather than individual objects.

## Table of Contents

- [Overview](#overview)
- [Entry Point](#entry-point)
- [Built-in Variables](#built-in-variables)
- [Helper Functions](#helper-functions)
- [Uniforms](#uniforms)
- [Shader Chains](#shader-chains)
- [Best Practices](#best-practices)
- [Quick Reference](#quick-reference)

---

## Overview

Screen shaders process the entire rendered frame as a 2D image. They run after all 3D rendering is complete and can read from depth, color, and geometry buffers to create post-processing effects.

### Basic Example

```glsl
void fragment() {
    COLOR = SampleColor(TEXCOORD) * vec3(1.0, 0.5, 0.5); // Red tint
}
```

### Loading a Shader

```c
// From file
R3D_ScreenShader* shader = R3D_LoadScreenShader("vignette.glsl");

// From memory
const char* code = "void fragment() { COLOR = vec3(1.0 - SampleColor(TEXCOORD)); }";
R3D_ScreenShader* shader = R3D_LoadScreenShaderFromMemory(code);

// Don't forget to unload when done
R3D_UnloadScreenShader(shader);
```

---

## Entry Point

Screen shaders have only **one required entry point**: `fragment()`. There is no vertex stage, and consequently, no varyings.

### Fragment Stage

Runs once per screen pixel to compute the final output color.

```glsl
void fragment() {
    // Read input color
    vec3 color = SampleColor(TEXCOORD);
    
    // Apply effect
    color = vec3(dot(color, vec3(0.299, 0.587, 0.114))); // Grayscale conversion
    
    // Write output
    COLOR = color;
}
```

---

## Built-in Variables

Screen shaders provide built-in variables for screen-space operations:

| Variable | Type | Description |
|----------|------|-------------|
| `TEXCOORD` | `vec2` | Normalized texture coordinates (0.0 to 1.0) |
| `PIXCOORD` | `ivec2` | Integer pixel coordinates (0 to resolution-1) |
| `TEXEL_SIZE` | `vec2` | Size of one texel (1.0 / RESOLUTION) |
| `RESOLUTION` | `vec2` | Screen resolution in pixels |
| `ASPECT` | `float` | Screen aspect ratio |
| `COLOR` | `vec3` | Output color (write to this) |

### Usage Examples

**Texture coordinates:**
```glsl
void fragment() {
    // Sample at current screen position
    COLOR = SampleColor(TEXCOORD);
}
```

**Pixel coordinates:**
```glsl
void fragment() {
    // Checkerboard pattern
    int checker = (PIXCOORD.x / 8 + PIXCOORD.y / 8) % 2;
    COLOR = SampleColor(TEXCOORD) * (checker == 0 ? 1.0 : 0.5);
}
```

**Resolution-aware effects:**
```glsl
void fragment() {
    // Blur using texel size for offset
    vec3 color = vec3(0.0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            color += FetchColor(PIXCOORD + ivec2(x, y));
        }
    }
    COLOR = color / 9.0; // Average of 3x3 grid
}
```

---

## Helper Functions

Screen shaders provide convenience functions for accessing frame data. Each function comes in two variants: `Fetch` (uses integer pixel coordinates) and `Sample` (uses normalized texture coordinates).

### Color Sampling

```glsl
vec3 FetchColor(ivec2 pixCoord);  // Fast, no filtering
vec3 SampleColor(vec2 texCoord);  // Bilinear filtering
```

**Example:**
```glsl
void fragment() {
    // Fetch exact pixel (faster)
    vec3 center = FetchColor(PIXCOORD);
    
    // Sample with filtering (smoother)
    vec3 blurred = SampleColor(TEXCOORD + vec2(0.01, 0.0));
    
    COLOR = mix(center, blurred, 0.5);
}
```

### Depth Sampling

Returns linear depth values from the depth buffer.

```glsl
float FetchDepth(ivec2 pixCoord);    // linear depth [near, far]
float SampleDepth(vec2 texCoord);

float FetchDepth01(ivec2 pixCoord);  // linear depth normalized [0, 1]
float SampleDepth01(vec2 texCoord);
```

**Example:**
```glsl
void fragment() {
    vec3 color = SampleColor(TEXCOORD);

    const float outline_size = 1.5;
    vec2 px = TEXEL_SIZE * outline_size;

    // Edge detection using depth
    float d  = SampleDepth(TEXCOORD);
    float dx1 = abs(d - SampleDepth(TEXCOORD + vec2(px.x, 0)));
    float dx2 = abs(d - SampleDepth(TEXCOORD - vec2(px.x, 0)));
    float dy1 = abs(d - SampleDepth(TEXCOORD + vec2(0, px.y)));
    float dy2 = abs(d - SampleDepth(TEXCOORD - vec2(0, px.y)));

    float edge = step(0.5, max(max(dx1, dx2), max(dy1, dy2)));

    COLOR = mix(color, vec3(0.0), edge);
}
```

### Position Sampling

Returns view-space position (camera-relative coordinates).

```glsl
vec3 FetchPosition(ivec2 pixCoord);
vec3 SamplePosition(vec2 texCoord);
```

**Example:**
```glsl
void fragment() {
    vec3 position = SamplePosition(TEXCOORD);
    
    // Distance from camera
    float distance = length(position);
    
    // Depth-based effect
    vec3 color = SampleColor(TEXCOORD);
    COLOR = color * (1.0 - smoothstep(10.0, 50.0, distance));
}
```

### Normal Sampling

Returns view-space surface normal.

```glsl
vec3 FetchNormal(ivec2 pixCoord);
vec3 SampleNormal(vec2 texCoord);
```

**Example:**
```glsl
void fragment() {
    vec3 normal = SampleNormal(TEXCOORD);
    
    // Edge detection using normals
    vec3 normal_right = SampleNormal(TEXCOORD + vec2(TEXEL_SIZE.x, 0.0));
    float edge = length(normal - normal_right);
    
    vec3 color = SampleColor(TEXCOORD);
    COLOR = mix(color, vec3(0.0), edge * 10.0);
}
```

### Fetch vs Sample

- **`Fetch`**: Uses integer pixel coordinates, no filtering, faster
  - Best for: Exact pixel reads, performance-critical code
  - Use with: `PIXCOORD`

- **`Sample`**: Uses normalized coordinates, bilinear filtering, smoother
  - Best for: Smooth effects, interpolated values
  - Use with: `TEXCOORD`

---

## Uniforms

Screen shaders support the same uniform system as surface shaders, with the same limits and behavior.

### Supported Types

**Values:**
- Scalars: `bool`, `int`, `float`
- Vectors: `vec2`, `vec3`, `vec4`
- Matrices: `mat2`, `mat3`, `mat4`

**Samplers:**
- `sampler1D`, `sampler2D`, `sampler3D`, `samplerCube`

### Limits

- **Maximum uniform values:** 16 by default (configurable via `R3D_MAX_SHADER_UNIFORMS`)
- **Maximum samplers:** 4 by default (configurable via `R3D_MAX_SHADER_SAMPLERS`)

### Example

```glsl
uniform float u_intensity;
uniform sampler2D u_lut;

void fragment() {
    vec3 color = SampleColor(TEXCOORD);
    
    // Apply color grading
    color = texture(u_lut, color.rg).rgb;
    
    // Apply intensity
    COLOR = mix(SampleColor(TEXCOORD), color, u_intensity);
}
```

```c
float intensity = 0.5f;
Texture2D lut = LoadTexture("color_lut.png");

R3D_SetScreenShaderUniform(shader, "u_intensity", &intensity);
R3D_SetScreenShaderSampler(shader, "u_lut", lut);
```

---

## Shader Chains

Screen shaders can be chained, R3D executes them using internal ping-pong buffers, avoiding extra buffers like with a `RenderTexture`.

### Setting Up a Chain

```c
void R3D_SetScreenShaderChain(R3D_ScreenShader** shaders, int count);
```

- **Maximum shaders:** Defined by `R3D_MAX_SCREEN_SHADERS` (default: 8)
- Shaders execute in the order specified
- Each shader receives the output of the previous shader
- `NULL` entries in the array are safely ignored
- Passing `shaders = NULL` or `count = 0` disables all screen shaders

### Example

```c
// Create shaders
R3D_ScreenShader* outline = R3D_LoadScreenShader("outline.glsl");
R3D_ScreenShader* fisheye = R3D_LoadScreenShader("fisheye.glsl");
R3D_ScreenShader* grain = R3D_LoadScreenShader("grain.glsl");

// Set up chain
R3D_ScreenShader* chain[] = {outline, fisheye, grain};
R3D_SetScreenShaderChain(chain, 3);

// Render loop
while (!WindowShouldClose()) {
    R3D_Begin();
    // ... draw 3D scene ...
    R3D_End(); // Screen shaders execute here
}

// Disable screen shaders (not mandatory at the end of the program)
R3D_SetScreenShaderChain(NULL, 0);

// Cleanup
R3D_UnloadScreenShader(fisheye);
R3D_UnloadScreenShader(grain);
R3D_UnloadScreenShader(outline);
```

---

## Best Practices

### Performance

1. **Use Fetch when possible:** `FetchColor(PIXCOORD)` is faster than `SampleColor(TEXCOORD)` when filtering isn't needed
2. **Minimize texture samples:** Cache sampled values if used multiple times
3. **Keep chains short:** Each shader in the chain adds overhead
4. **Avoid heavy loops:** Keep iterations bounded and minimal
5. **Leverage built-ins:** Use `TEXEL_SIZE` instead of computing `1.0 / RESOLUTION`

### Quality

1. **Respect aspect ratio:** Use `ASPECT` for aspect-aware effects
2. **Test at different resolutions:** Effects should scale properly
3. **Use linear filtering wisely:** Sample (filtered) for smooth effects, Fetch (unfiltered) for sharp details
4. **Consider edge cases:** Check behavior at screen edges (TEXCOORD = 0.0 or 1.0)

### Organization

1. **One effect per shader:** Keep shaders focused and reusable
2. **Chain for complexity:** Combine simple shaders instead of creating monolithic ones
3. **Name meaningfully:** Use clear, descriptive names for shaders and uniforms
4. **Document parameters:** Comment uniform purposes and expected ranges

### Example: Well-Structured Effect

**vignette.glsl:**
```glsl
// Simple vignette effect
// u_intensity: Controls vignette strength (0.0 = none, 1.0 = full)
// u_radius: Controls vignette size (0.0 = tight, 1.0 = wide)

uniform float u_intensity;
uniform float u_radius;

void fragment() {
    vec3 color = SampleColor(TEXCOORD);

    // Calculate distance from center
    vec2 center = TEXCOORD - vec2(0.5);
    center.x *= ASPECT; // Aspect correction
    float dist = length(center);

    // Apply vignette
    float vignette = smoothstep(u_radius, u_radius * 0.5, dist);
    COLOR = color * mix(1.0, vignette, u_intensity);
}
```

---

## Quick Reference

### Loading/Unloading
```c
R3D_ScreenShader* R3D_LoadScreenShader(const char* filePath);
R3D_ScreenShader* R3D_LoadScreenShaderFromMemory(const char* code);
void R3D_UnloadScreenShader(R3D_ScreenShader* shader);
```

### Setting Uniforms
```c
void R3D_SetScreenShaderUniform(R3D_ScreenShader* shader, const char* name, const void* value);
void R3D_SetScreenShaderSampler(R3D_ScreenShader* shader, const char* name, Texture texture);
```

### Shader Chain
```c
void R3D_SetScreenShaderChain(R3D_ScreenShader** shaders, int count);
```

### Shader Structure
```glsl
uniform <type> <name>;          // Optional: uniforms

void fragment() {               // Required: fragment stage
    // Read: TEXCOORD, PIXCOORD, RESOLUTION, TEXEL_SIZE, ASPECT
    // Sample: SampleColor(), SampleDepth(), SamplePosition(), SampleNormal()
    // Fetch: FetchColor(), FetchDepth(), FetchPosition(), FetchNormal()
    // Write: COLOR
}
```

### Built-in Variables
```glsl
// Input (read-only)
vec2 TEXCOORD;      // Normalized coordinates [0..1]
ivec2 PIXCOORD;     // Integer pixel coordinates
vec2 TEXEL_SIZE;    // Size of one pixel (1.0 / RESOLUTION)
vec2 RESOLUTION;    // Screen resolution
float ASPECT;       // Screen aspect ratio

// Output (write)
vec3 COLOR;         // Final pixel color
```

### Helper Functions
```glsl
// Color
vec3 FetchColor(ivec2 pixCoord);
vec3 SampleColor(vec2 texCoord);

// Depth (linear, near to far)
float FetchDepth(ivec2 pixCoord);
float SampleDepth(vec2 texCoord);

// Depth (linear normalized, 0 to 1)
float FetchDepth01(ivec2 pixCoord);
float SampleDepth01(vec2 texCoord);

// Position (view-space)
vec3 FetchPosition(ivec2 pixCoord);
vec3 SamplePosition(vec2 texCoord);

// Normal (view-space)
vec3 FetchNormal(ivec2 pixCoord);
vec3 SampleNormal(vec2 texCoord);
```
