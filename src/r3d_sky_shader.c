/* r3d_sky_shader.c -- R3D Sky Shader Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_sky_shader.h>
#include <r3d_config.h>
#include <raylib.h>
#include <string.h>
#include <stdlib.h>
#include <glad.h>

#include "./modules/r3d_shader.h"
#include "./common/r3d_rshade.h"

// ========================================
// OPAQUE STRUCTS
// ========================================

struct R3D_SkyShader {
    r3d_shader_custom_t program;
    char userCode[R3D_MAX_SHADER_CODE_LENGTH];
};

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool compile_shader(R3D_SkyShader* shader);

// ========================================
// PUBLIC API
// ========================================

R3D_SkyShader* R3D_LoadSkyShader(const char* filePath)
{
    char* code = LoadFileText(filePath);
    if (code == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load sky shader; Unable to load shader file");
        return NULL;
    }

    R3D_SkyShader* shader = R3D_LoadSkyShaderFromMemory(code);
    UnloadFileText(code);

    return shader;
}

R3D_SkyShader* R3D_LoadSkyShaderFromMemory(const char* code)
{
    size_t userCodeLen = strlen(code);
    if (userCodeLen > R3D_MAX_SHADER_CODE_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load sky shader; User code too long");
        return NULL;
    }

    if (!strstr(code, "void fragment()")) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load sky shader; Missing fragment() entry point");
        return NULL;
    }

    R3D_SkyShader* shader = RL_CALLOC(1, sizeof(R3D_SkyShader));
    if (!shader) {
        R3D_TRACELOG(LOG_ERROR, "Bad alloc during sky shader loading");
        return NULL;
    }

    char* output = RL_MALLOC(userCodeLen * 3);
    if (!output) {
        R3D_TRACELOG(LOG_ERROR, "Bad alloc during sky shader loading");
        RL_FREE(shader);
        return NULL;
    }

    char* outPtr = output;
    const char* ptr = code;

    int uniformCount = 0;
    int samplerCount = 0;
    int currentOffset = 0;

    r3d_rshade_parsed_function_t fragmentFunc = {0};

    /* --- PHASE 1: Parse user code and collect metadata --- */

    while (*ptr)
    {
        r3d_rshade_skip_whitespace_and_comments(&ptr);
        if (!*ptr) break;

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

        // Parse fragment() function
        r3d_rshade_parsed_function_t* func = r3d_rshade_check_shader_entry(ptr, NULL, &fragmentFunc);
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

    // Copy global code (excluding comments, uniforms, fragment()) then write fragment stage section
    outPtr = r3d_rshade_copy_global_code(outPtr, code, false, NULL, &fragmentFunc);
    outPtr = r3d_rshade_write_shader_function(outPtr, "fragment", &fragmentFunc);

    *outPtr = '\0';

    // Copy transformed code to shader structure
    size_t finalLen = strlen(output);
    if (finalLen > R3D_MAX_SHADER_CODE_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load sky shader; Transformed code too long");
        RL_FREE(output);
        RL_FREE(shader);
        return NULL;
    }

    memcpy(shader->userCode, output, finalLen + 1);
    RL_FREE(output);

    /* --- PHASE 3: Compile shader --- */

    if (!compile_shader(shader)) {
        R3D_UnloadSkyShader(shader);
        return NULL;
    }

    /* --- PHASE 4: Initialize uniform buffer --- */

    r3d_rshade_init_ubo(&shader->program.uniforms, currentOffset);

    R3D_TRACELOG(LOG_INFO, "Sky shader loaded successfully");
    R3D_TRACELOG(LOG_INFO, "    > Sampler count: %i", samplerCount);
    R3D_TRACELOG(LOG_INFO, "    > Uniform count: %i", uniformCount);

    return shader;
}

void R3D_UnloadSkyShader(R3D_SkyShader* shader)
{
    if (!shader) return;

    if (shader->program.uniforms.bufferId != 0) {
        glDeleteBuffers(1, &shader->program.uniforms.bufferId);
    }

    if (shader->program.prepare.cubemapCustomSky.id != 0) {
        glDeleteProgram(shader->program.prepare.cubemapCustomSky.id);
    }

    RL_FREE(shader);
}

void R3D_SetSkyShaderUniform(R3D_SkyShader* shader, const char* name, const void* value)
{
    if (!shader) {
        R3D_TRACELOG(LOG_WARNING, "Cannot set uniform '%s' on NULL sky shader", name);
        return;
    }

    if (!r3d_shader_set_custom_uniform(&shader->program, name, value)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to set custom uniform '%s'", name);
    }
}

void R3D_SetSkyShaderSampler(R3D_SkyShader* shader, const char* name, Texture texture)
{
    if (!shader) {
        R3D_TRACELOG(LOG_WARNING, "Cannot set sampler '%s' on NULL sky shader", name);
        return;
    }

    if (!r3d_shader_set_custom_sampler(&shader->program, name, texture)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to set custom sampler '%s'", name);
    }
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

bool compile_shader(R3D_SkyShader* shader)
{
    // Store reference to the user code in custom shader struct
    shader->program.userCode = shader->userCode;

    bool ok = R3D_MOD_SHADER_LOADER.prepare.cubemapCustomSky(&shader->program);
    if (!ok) {
        R3D_TRACELOG(LOG_ERROR, "Failed to compile sky shader");
    }
    return ok;
}
