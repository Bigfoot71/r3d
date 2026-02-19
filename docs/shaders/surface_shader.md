# Surface Shaders

Surface shaders are custom shaders that can be applied to materials and decals in R3D. They provide a simplified way to write GLSL code by abstracting away the complexity of multiple render passes.

## Table of Contents

- [Overview](#overview)
- [Entry Points](#entry-points)
- [Built-in Variables](#built-in-variables)
- [Varyings](#varyings)
- [Uniforms](#uniforms)
- [Material Sampling](#material-sampling)
- [Usage Hints](#usage-hints)
- [Best Practices](#best-practices)
- [Quick Reference](#quick-reference)

---

## Overview

A surface shader is a simplified shader interface that allows you to modify vertex and fragment behavior without worrying about the underlying render pipeline. R3D automatically handles multiple render passes (opaque, transparent, shadows, etc.) from a single shader definition.

### Basic Example

```glsl
uniform float u_time;

void fragment() {
    ALBEDO *= 0.5 + 0.5 * sin(u_time);
}
```

### Loading a Shader

```c
// From file
R3D_SurfaceShader* shader = R3D_LoadSurfaceShader("my_shader.glsl");

// From memory
const char* code = "void fragment() { ALBEDO = vec3(1.0, 0.0, 0.0); }";
R3D_SurfaceShader* shader = R3D_LoadSurfaceShaderFromMemory(code);

// Don't forget to unload when done
R3D_UnloadSurfaceShader(shader);
```

---

## Entry Points

Surface shaders have two optional entry points: `vertex()` and `fragment()`. At least one must be defined.

### Vertex Stage

Runs once per vertex, before rasterization. Use it to modify vertex positions, colors, or pass data to the fragment stage.

```glsl
void vertex() {
    POSITION.y += sin(POSITION.x * 10.0) * 0.1;
}
```

### Fragment Stage

Runs once per pixel. Use it to modify final surface properties like albedo, roughness, or emission.

```glsl
void fragment() {
    ALBEDO = vec3(1.0, 0.0, 0.0); // Red surface
    ROUGHNESS = 0.5;
}
```

### Both Stages

You can define both stages to create complex effects:

```glsl
varying float v_height;

void vertex() {
    v_height = POSITION.y;
}

void fragment() {
    ALBEDO = mix(vec3(0.0, 0.5, 0.0), vec3(1.0, 1.0, 1.0), v_height);
}
```

---

## Built-in Variables

Built-in variables are pre-defined values you can read and modify in your shader. They can only be accessed within their corresponding entry point.

### Vertex Stage

All vertex-stage variables are initialized with local (pre-transformation) attribute values:

| Variable | Type | Description |
|----------|------|-------------|
| `MATRIX_MODEL` | `mat4` | Model matrix (read-only). |
| `MATRIX_NORMAL` | `mat3` | Normal matrix (read-only). |
| `MATRIX_INV_VIEW` | `mat4` | Inverse view matrix (read-only). |
| `MATRIX_VIEW_PROJECTION` | `mat4` | View-projection matrix (read-only). |
| `POSITION` | `vec3` | Vertex position (local space) |
| `TEXCOORD` | `vec2` | Texture coordinates |
| `NORMAL` | `vec3` | Vertex normal (local space) |
| `TANGENT` | `vec4` | Vertex tangent (w = handedness) |
| `COLOR` | `vec4` | Vertex color |
| `EMISSION` | `vec3` | Vertex emission |
| `INSTANCE_POSITION` | `vec3` | Instance position (world space) |
| `INSTANCE_ROTATION` | `vec4` | Instance rotation (quaternion: x, y, z, w) |
| `INSTANCE_SCALE` | `vec3` | Instance scale |
| `INSTANCE_COLOR` | `vec4` | Instance color |
| `INSTANCE_CUSTOM` | `vec4` | Custom user-defined instance data |
| `FRAME_INDEX` | `int` | Index incremented at each frame |
| `TIME` | `float` | Time provided by the raylib's `GetTime()` |

> [!WARNING]
> Mesh attributes (`POSITION`, `NORMAL`, etc.) are provided in **local space** and must remain in local space.
> You should **not manually apply model, view, or projection transformations** to them.

**Instance Variables:**

The `INSTANCE_*` variables are always available, even for non-instanced rendering.
When instancing is not used or when an instance buffer doesn't define certain attributes, they default to:

```glsl
INSTANCE_POSITION = vec3(0.0);
INSTANCE_ROTATION = vec4(0.0, 0.0, 0.0, 1.0);  // Identity quaternion
INSTANCE_SCALE = vec3(1.0);
INSTANCE_COLOR = vec4(1.0);
INSTANCE_CUSTOM = vec4(0.0);
```

You can modify mesh-local attributes (`POSITION`, `NORMAL`, etc.) and instance attributes (`INSTANCE_*`) independently.
R3D automatically composes them internally, so you don't need to manually combine them.

**Custom Instance Data:**

`INSTANCE_CUSTOM` is reserved for user-defined data. Unlike other instance attributes, it has no predefined meaning and can store any data you need.
If you want to use it in the fragment stage, pass it through a varying:

```glsl
varying vec4 v_custom_data;

void vertex() {
    // Use custom data however you want
    v_custom_data = INSTANCE_CUSTOM;
    
    // Example: use as animation offset
    POSITION.y += INSTANCE_CUSTOM.x * sin(INSTANCE_CUSTOM.y);
}

void fragment() {
    // Access custom data passed from vertex stage
    ALBEDO *= v_custom_data.rgb;
}
```

**Example:**
```glsl
void vertex() {
    // Modify local mesh attributes
    POSITION *= 1.5; // Scale vertex position
    COLOR.rgb *= 0.5; // Darken vertex color
    
    // Modify instance attributes separately
    INSTANCE_SCALE *= 2.0; // Double instance scale
    INSTANCE_COLOR.a *= 0.8; // Make instance more transparent
    
    // R3D will compose these automatically
}
```

### Fragment Stage

Fragment-stage variables are pre-initialized with material values (unless `R3D_NO_AUTO_FETCH` is defined):

| Variable | Type | Description |
|----------|------|-------------|
| `TEXCOORD` | `vec2` | Interpolated texture coordinates |
| `NORMAL` | `vec3` | Surface normal (world space) |
| `TANGENT` | `vec3` | Surface tangent |
| `BITANGENT` | `vec3` | Surface bitangent |
| `ALBEDO` | `vec3` | Base color |
| `ALPHA` | `float` | Transparency |
| `EMISSION` | `vec3` | Emissive color |
| `NORMAL_MAP` | `vec3` | Normal map value |
| `OCCLUSION` | `float` | Ambient occlusion |
| `ROUGHNESS` | `float` | Surface roughness (0 = smooth, 1 = rough) |
| `METALNESS` | `float` | Metallic property (0 = dielectric, 1 = metal) |
| `FRAME_INDEX` | `int` | Index incremented at each frame |
| `TIME` | `float` | Time provided by the raylib's `GetTime()` |

**Example:**
```glsl
void fragment() {
    ALBEDO = vec3(1.0, 0.0, 0.0); // Red surface
    ROUGHNESS = 0.2; // Shiny
    METALNESS = 1.0; // Metallic
}
```

### Important Notes

- Built-in variables are **scoped to their entry point**. You cannot access `ALBEDO` in `vertex()` or `POSITION` in `fragment()`.
- If you need to pass data between stages, use [varyings](#varyings).
- To use built-in variables in helper functions, pass them as parameters:

```glsl
vec3 darken(vec3 color, float amount) {
    return color * amount;
}

void fragment() {
    ALBEDO = darken(ALBEDO, 0.5);
}
```

---

## Varyings

Varyings allow you to pass data from the vertex stage to the fragment stage. They are automatically interpolated across the triangle.

### Basic Usage

```glsl
varying float v_height;

void vertex() {
    v_height = POSITION.y;
}

void fragment() {
    ALBEDO = mix(vec3(0.2, 0.8, 0.2), vec3(1.0), v_height);
}
```

### Interpolation Qualifiers

You can control how varyings are interpolated using qualifiers:

| Qualifier | Description |
|-----------|-------------|
| `flat` | No interpolation (use value from provoking vertex) |
| `smooth` | Perspective-correct interpolation (default) |
| `noperspective` | Linear interpolation in screen space |

**Example:**
```glsl
flat varying int v_material_id;
noperspective varying vec2 v_screen_uv;

void vertex() {
    v_material_id = 1;
    v_screen_uv = TEXCOORD;
}

void fragment() {
    if (v_material_id == 1) {
        ALBEDO = texture(u_texture, v_screen_uv).rgb;
    }
}
```

### Limits

- **Maximum varyings:** 32 (hardware permitting)
- In practice, you'll rarely need more than a handful

---

## Uniforms

Uniforms are constant values that can be set from your C code. They remain constant across all vertices/fragments in a draw call.

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

### Declaring Uniforms

```glsl
uniform float u_time;
uniform vec3 u_color;
uniform mat4 u_transform;
uniform sampler2D u_texture;
```

### Setting Uniforms from C

**Values:**
```c
float time = GetTime();
R3D_SetSurfaceShaderUniform(shader, "u_time", &time);

Vector3 color = {1.0f, 0.0f, 0.0f};
R3D_SetSurfaceShaderUniform(shader, "u_color", &color);

// For booleans, use int (4 bytes)
int flag = 1;  // true
R3D_SetSurfaceShaderUniform(shader, "u_flag", &flag);
```

**Samplers:**
```c
Texture2D texture = LoadTexture("texture.png");
R3D_SetSurfaceShaderSampler(shader, "u_texture", texture);
```

### Important Notes

- **Default values:** All uniforms default to zero (samplers have no default texture)
- **Boolean handling:** When setting `bool` uniforms from C, pass an `int` (4 bytes). Non-zero values are `true`, zero is `false`.
- **Persistence:** Uniform values persist across frames until changed
- **Per-shader state:** Each shader maintains its own uniform state
- **Update timing:** Uniforms are uploaded to GPU only when needed (marked dirty)
- **No per-draw updates:** You cannot change uniforms between draw calls within a single frame. Since rendering happens at `R3D_End()`, only the last uniform value set before `R3D_End()` will be used.

### Example

```glsl
uniform float u_time;
uniform sampler2D u_noise;
uniform bool u_enable_effect;

void fragment() {
    if (u_enable_effect) {
        vec2 uv = TEXCOORD + texture(u_noise, TEXCOORD * 2.0).xy * 0.1;
        ALBEDO *= 0.5 + 0.5 * sin(u_time + uv.x * 10.0);
    }
}
```

```c
float time = 0.0f;
Texture2D noise = LoadTexture("noise.png");
int enableEffect = 1;

R3D_SetSurfaceShaderSampler(shader, "u_noise", noise);
R3D_SetSurfaceShaderUniform(shader, "u_enable_effect", &enableEffect);

while (!WindowShouldClose()) {
    time += GetFrameTime();
    R3D_SetSurfaceShaderUniform(shader, "u_time", &time);
    
    R3D_Begin();
    // ... draw with shader ...
    R3D_End();
}
```

---

## Material Sampling

By default, material textures (albedo, normal, ORM) are automatically sampled and available as built-in variables in the fragment stage.

### Disabling Auto-Fetch

If you want to start with zero values and sample materials manually, define:

```glsl
#define R3D_NO_AUTO_FETCH
```

This sets `ALBEDO`, `NORMAL_MAP`, and `OCCLUSION`/`ROUGHNESS`/`METALNESS` to zero.

### Manual Sampling Functions

```glsl
vec4 SampleAlbedo(vec2 texCoord);
vec3 SampleEmission(vec2 texCoord);
vec3 SampleNormal(vec2 texCoord);
vec3 SampleOrm(vec2 texCoord); // (Occlusion, Roughness, Metalness)
```

### Quick Auto-Fill

To automatically fill all built-in variables with material values:

```glsl
void FetchMaterial(vec2 texCoord);
```

### Example

```glsl
#define R3D_NO_AUTO_FETCH

void fragment() {
    // Sample material at distorted UV
    vec2 distorted_uv = TEXCOORD + vec2(sin(TEXCOORD.y * 10.0) * 0.1, 0.0);
    FetchMaterial(distorted_uv);
    
    // Or sample individual textures
    // ALBEDO = SampleAlbedo(distorted_uv).rgb;
    // vec3 orm = SampleOrm(distorted_uv);
    // ROUGHNESS = orm.g;
}
```

---

## Usage Hints

R3D compiles multiple shader variants for different render passes (opaque, transparent, shadows, etc.). By default, only the opaque variant is pre-compiled; others compile on-demand when needed.

### The Problem

On-demand compilation can cause stuttering when a new variant is first used. For example, if your shader is used on a transparent object, the transparent variant compiles when the object first becomes visible.

### The Solution

Use `#pragma usage` to specify which variants should be pre-compiled:

```glsl
#pragma usage transparent shadow
```

### Available Usage Hints

| Hint | Description |
|------|-------------|
| `opaque` | Opaque rendering for **lit objects** (default if no pragma specified) |
| `prepass` | Transparent pre-pass rendering for **lit objects** |
| `transparent` | Transparent rendering (color/alpha blending) for **lit objects** |
| `unlit` | Unlit rendering (handles both opaque and transparent **unlit objects**) |
| `shadow` | Shadow map rendering |
| `decal` | Decal rendering |
| `probe` | Reflection probe rendering |

### Examples

**Opaque object with shadows:**
```glsl
#pragma usage opaque shadow

void fragment() {
    ALBEDO = vec3(1.0, 0.0, 0.0);
    ALPHA = 0.5; // Alpha cutoff
}
```

**Transparent object:**
```glsl
#pragma usage transparent

void fragment() {
    ALBEDO = vec3(0.0, 0.5, 1.0);
    ALPHA = 0.5; // Alpha blending
}
```

**Unlit object:**
```glsl
#pragma usage unlit

void fragment() {
    ALBEDO = vec3(1.0, 1.0, 0.0);
    ALPHA = 0.5; // Alpha cutoff or blending
}
```

**Decal shader:**
```glsl
#pragma usage decal

void fragment() {
    ALBEDO = vec3(1.0, 1.0, 0.0);
    ALPHA = 0.5; // Alpha fading
}
```

### Important Notes

- Usage hints are **optional**; missing variants will still compile on-demand
- Multiple hints can be specified: `#pragma usage opaque transparent shadow`
- Rendering mode separation: `opaque`, `prepass`, and `transparent` apply to **lit objects** only, while `unlit` applies to **unlit objects** regardless of opacity
- If no pragma is specified, only `opaque` is pre-compiled
- Variants not in the pragma can still be used; they just compile lazily

---

## Best Practices

### Performance

1. **Minimize varyings:** Only pass what's needed
2. **Leverage vertex math:** Compute values per-vertex when they can be interpolated
3. **Avoid dynamic loops & heavy branches:** Keep loops bounded and branches predictable/simple
4. **Reuse calculations:** Don't repeat work in shader

### Debugging

1. **Test incrementally:** Start simple, add complexity gradually
2. **Use constants first:** Replace textures or calculations with constants to confirm logic
3. **Isolate effects:** Disable parts of the shader to identify issues
4. **Use emissive for debugging:** `EMISSION = vec3(some_value)` to visualize values

### Organization

1. **One shader per effect:** Don't create mega-shaders with branches for different effects
2. **Use meaningful names:** You can prefix uniforms with `u_`, varyings with `v_`
3. **Use usage hints:** Pre-compile variants you know you'll need

---

## Quick Reference

### Loading/Unloading
```c
R3D_SurfaceShader* R3D_LoadSurfaceShader(const char* filePath);
R3D_SurfaceShader* R3D_LoadSurfaceShaderFromMemory(const char* code);
void R3D_UnloadSurfaceShader(R3D_SurfaceShader* shader);
```

### Setting Uniforms
```c
void R3D_SetSurfaceShaderUniform(R3D_SurfaceShader* shader, const char* name, const void* value);
void R3D_SetSurfaceShaderSampler(R3D_SurfaceShader* shader, const char* name, Texture texture);
```

### Shader Structure
```glsl
#pragma usage <hints>           // Optional: opaque, transparent, shadow, etc.
#define R3D_NO_AUTO_FETCH       // Optional: disable automatic material sampling

uniform <type> <name>;          // Uniforms
varying <type> <name>;          // Varyings (communication between stages)

void vertex() {                 // Optional: vertex stage
    // Modify POSITION, NORMAL, etc.
}

void fragment() {               // Optional: fragment stage
    // Modify ALBEDO, ROUGHNESS, etc.
}
```
