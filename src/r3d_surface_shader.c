/* r3d_surface_shader.c -- R3D Material Shader Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_surface_shader.h>
#include <r3d_config.h>
#include <raylib.h>
#include <string.h>
#include <ctype.h>
#include <glad.h>

#include "./modules/r3d_shader.h"
#include "./common/r3d_rshade.h"
#include "./common/r3d_helper.h"

// ========================================
// INTERNAL ENUMS
// ========================================

typedef uint32_t usage_hint_t;

#define USAGE_HINT_OPAQUE        (1 << 0)   //< Build: geometry
#define USAGE_HINT_PREPASS       (1 << 1)   //< Build: geometry, forward, depth
#define USAGE_HINT_TRANSPARENT   (1 << 2)   //< Build: forward
#define USAGE_HINT_SHADOW        (1 << 3)   //< Build: depth, depthCube
#define USAGE_HINT_DECAL         (1 << 4)   //< Build: decal
#define USAGE_HINT_PROBE         (1 << 5)   //< Build: probe

// ========================================
// OPAQUE STRUCTS
// ========================================

struct R3D_SurfaceShader {
    r3d_shader_custom_t program;
    char userCode[R3D_MAX_SHADER_CODE_LENGTH];
};

// ========================================
// INTERNAL FUNCTIONS
// ========================================

/* Parse usage pragma to decide wich shader to pre-compile */
static usage_hint_t parse_pragma_usage(const char** ptr);

/* Compile all needed variants accordingly to the usage hints specified in pragma */
static bool compile_shader_variants(R3D_SurfaceShader* shader, usage_hint_t usage);

// ========================================
// PUBLIC API
// ========================================

R3D_SurfaceShader* R3D_LoadSurfaceShader(const char* filePath)
{
    char* code = LoadFileText(filePath);
    R3D_SurfaceShader* shader = R3D_LoadSurfaceShaderFromMemory(code);
    UnloadFileText(code);
    return shader;
}

R3D_SurfaceShader* R3D_LoadSurfaceShaderFromMemory(const char* code)
{
    int userCodeLen = strlen(code);
    if (userCodeLen > R3D_MAX_SHADER_CODE_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load surface shader; User code too long");
        return NULL;
    }

    if (!strstr(code, "void vertex()") && !strstr(code, "void fragment()")) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load surface shader; Missing entry points");
        return NULL;
    }

    R3D_SurfaceShader* shader = RL_CALLOC(1, sizeof(R3D_SurfaceShader));
    if (!shader) {
        R3D_TRACELOG(LOG_ERROR, "Bad alloc during surface shader loading");
        return NULL;
    }

    char* output = RL_MALLOC(userCodeLen * 3);
    if (!output) {
        R3D_TRACELOG(LOG_ERROR, "Bad alloc during surface shader loading");
        RL_FREE(shader);
        return NULL;
    }

    char* outPtr = output;
    const char* ptr = code;

    int uniformCount = 0;
    int samplerCount = 0;
    int varyingCount = 0;
    int currentOffset = 0;

    usage_hint_t usage = 0;
    r3d_rshade_varying_t varyings[32] = {0};
    r3d_rshade_parsed_function_t vertexFunc = {0};
    r3d_rshade_parsed_function_t fragmentFunc = {0};

    /* --- PHASE 1: Parse user code and collect metadata --- */

    while (*ptr)
    {
        r3d_rshade_skip_whitespace_and_comments(&ptr);
        if (!*ptr) break;

        // Parse #pragma usage directive
        if (strncmp(ptr, "#pragma", 7) == 0 && isspace(ptr[7])) {
            usage |= parse_pragma_usage(&ptr);
            continue;
        }

        // Parse uniform declarations
        if (r3d_rshade_match_keyword(ptr, "uniform", 7)) {
            ptr += 7;
            r3d_rshade_parse_uniform(&ptr,
                shader->program.samplers,
                &shader->program.uniforms,
                &samplerCount,
                &uniformCount,
                &currentOffset,
                R3D_MAX_SHADER_SAMPLERS,
                R3D_MAX_SHADER_UNIFORMS
            );
            continue;
        }

        // Parse varying declarations
        if (r3d_rshade_match_varying_keyword(ptr)) {
            if (varyingCount < 32) {
                if (r3d_rshade_parse_varying(&ptr, &varyings[varyingCount])) {
                    varyingCount++;
                }
            }
            else {
                r3d_rshade_skip_to_semicolon(&ptr);
            }
            continue;
        }

        // Parse vertex() and fragment() functions
        r3d_rshade_parsed_function_t* func = r3d_rshade_check_shader_entry(ptr, &vertexFunc, &fragmentFunc);
        if (func) {
            r3d_rshade_skip_to_brace(&ptr);
            if (*ptr == '{') {
                func->bodyStart = ptr;
                r3d_rshade_skip_to_matching_brace(&ptr);
                func->bodyEnd = ptr;
            }
            continue;
        }

        ptr++;
    }

    /* --- PHASE 2: Generate transformed shader code --- */

    // Write uniform block and samplers
    outPtr = r3d_rshade_write_uniform_block(outPtr, shader->program.uniforms.entries, uniformCount);
    outPtr = r3d_rshade_write_samplers(outPtr, shader->program.samplers, samplerCount);

    // Copy global code (excluding pragma, comments, uniforms, varyings, entry points)
    outPtr = r3d_rshade_copy_global_code(outPtr, code, true, &vertexFunc, &fragmentFunc);

    // Write vertex stage section
    outPtr += sprintf(outPtr, "\n#ifdef STAGE_VERT\n\n");
    outPtr = r3d_rshade_write_varyings(outPtr, "out", varyings, varyingCount);
    outPtr = r3d_rshade_write_shader_function(outPtr, "vertex", &vertexFunc);
    outPtr += sprintf(outPtr, "\n#endif\n\n");

    // Write fragment stage section
    outPtr += sprintf(outPtr, "#ifdef STAGE_FRAG\n\n");
    outPtr = r3d_rshade_write_varyings(outPtr, "in", varyings, varyingCount);
    outPtr = r3d_rshade_write_shader_function(outPtr, "fragment", &fragmentFunc);
    outPtr += sprintf(outPtr, "\n#endif\n");

    *outPtr = '\0';

    // Copy transformed code to shader structure
    int finalLen = strlen(output);
    if (finalLen > R3D_MAX_SHADER_CODE_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load surface shader; Transformed code too long");
        RL_FREE(output);
        RL_FREE(shader);
        return NULL;
    }

    memcpy(shader->userCode, output, finalLen + 1);
    RL_FREE(output);

    /* --- PHASE 3: Pre-compile needed shader variants --- */

    if (!compile_shader_variants(shader, usage)) {
        R3D_UnloadSurfaceShader(shader);
        return NULL;
    }

    /* --- PHASE 4: Initialize uniform buffer --- */

    r3d_rshade_init_ubo(&shader->program.uniforms, currentOffset);

    return shader;
}

void R3D_UnloadSurfaceShader(R3D_SurfaceShader* shader)
{
    #define DELETE_PROGRAM(id) \
        do { if ((id) != 0) glDeleteProgram((id)); } while(0)

    if (shader == NULL) return;

    if (shader->program.uniforms.bufferId != 0) {
        glDeleteBuffers(1, &shader->program.uniforms.bufferId);
    }

    DELETE_PROGRAM(shader->program.scene.geometry.id);
    DELETE_PROGRAM(shader->program.scene.forward.id);
    DELETE_PROGRAM(shader->program.scene.depth.id);
    DELETE_PROGRAM(shader->program.scene.depthCube.id);
    DELETE_PROGRAM(shader->program.scene.probe.id);
    DELETE_PROGRAM(shader->program.scene.decal.id);

    RL_FREE(shader);

#undef DELETE_PROGRAM
}

void R3D_SetSurfaceShaderUniform(R3D_SurfaceShader* shader, const char* name, const void* value)
{
    if (!r3d_shader_set_custom_uniform(&shader->program, name, value)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to set custom uniform '%s'", name);
    }
}

void R3D_SetSurfaceShaderSampler(R3D_SurfaceShader* shader, const char* name, Texture texture)
{
    if (!r3d_shader_set_custom_sampler(&shader->program, name, texture)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to set custom sampler '%s'", name);
    }
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

usage_hint_t parse_pragma_usage(const char** ptr)
{
    usage_hint_t result = 0;

    // Skip "#pragma"
    *ptr += 7;  // strlen("#pragma")
    r3d_rshade_skip_whitespace(ptr);

    // Check for "usage" keyword
    if (strncmp(*ptr, "usage", 5) != 0 || !isspace((*ptr)[5])) {
        r3d_rshade_skip_to_end_of_line(ptr);
        return 0;
    }

    *ptr += 5;  // strlen("usage")
    r3d_rshade_skip_whitespace(ptr);

    // Parse usage hints until end of line
    while (**ptr && **ptr != '\n')
    {
        r3d_rshade_skip_whitespace(ptr);
        if (!**ptr || **ptr == '\n') break;

        // Match usage hint keywords
        static const struct {
            const char* name;
            size_t len;
            usage_hint_t flag;
        } hints[] = {
            {"opaque",      6,  USAGE_HINT_OPAQUE},
            {"prepass",     7,  USAGE_HINT_PREPASS},
            {"transparent", 11, USAGE_HINT_TRANSPARENT},
            {"shadow",      6,  USAGE_HINT_SHADOW},
            {"decal",       5,  USAGE_HINT_DECAL},
            {"probe",       5,  USAGE_HINT_PROBE}
        };

        bool matched = false;
        for (int i = 0; i < 6; i++) {
            if (strncmp(*ptr, hints[i].name, hints[i].len) == 0 &&
                (isspace((*ptr)[hints[i].len]) || (*ptr)[hints[i].len] == '\n' || (*ptr)[hints[i].len] == '\0'))
            {
                result |= hints[i].flag;
                *ptr += hints[i].len;
                matched = true;
                break;
            }
        }

        if (!matched) {
            // Unknown hint, skip to next whitespace or end of line
            while (**ptr && !isspace(**ptr)) (*ptr)++;
        }
    }

    r3d_rshade_skip_to_end_of_line(ptr);
    return result;
}

bool compile_shader_variants(R3D_SurfaceShader* shader, usage_hint_t usage)
{
    // Store reference to the user code in custom shader struct
    shader->program.userCode = shader->userCode;

    if (usage == 0) {
        bool ok = R3D_MOD_SHADER_LOADER.scene.geometry(&shader->program);
        if (!ok) {
            R3D_TRACELOG(LOG_ERROR, "Failed to compile surface shader");
        }
        return ok;
    }

    const struct {
        const char* name;
        usage_hint_t condition;
        r3d_shader_loader_func func;
    } variants[] = {
        {"geometry",  USAGE_HINT_OPAQUE | USAGE_HINT_PREPASS,      R3D_MOD_SHADER_LOADER.scene.geometry},
        {"forward",   USAGE_HINT_PREPASS | USAGE_HINT_TRANSPARENT, R3D_MOD_SHADER_LOADER.scene.forward},
        {"depth",     USAGE_HINT_SHADOW | USAGE_HINT_PREPASS,      R3D_MOD_SHADER_LOADER.scene.depth},
        {"depthCube", USAGE_HINT_SHADOW,                           R3D_MOD_SHADER_LOADER.scene.depthCube},
        {"decal",     USAGE_HINT_DECAL,                            R3D_MOD_SHADER_LOADER.scene.decal},
        {"probe",     USAGE_HINT_PROBE,                            R3D_MOD_SHADER_LOADER.scene.probe}
    };

    for (int i = 0; i < 6; i++) {
        if (BIT_TEST_ANY(usage, variants[i].condition)) {
            if (!variants[i].func(&shader->program)) {
                R3D_TRACELOG(LOG_ERROR, "Failed to compile surface shader (variant: '%s')", variants[i].name);
                return false;
            }
        }
    }

    return true;
}
