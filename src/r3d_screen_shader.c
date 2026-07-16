/* r3d_screen_shader.c -- R3D Screen Shader Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_screen_shader.h>
#include <r3d_config.h>
#include <raylib.h>
#include <string.h>
#include <glad.h>

#include "./modules/r3d_shader.h"
#include "./common/r3d_rshade.h"
#include "./common/r3d_helper.h"
#include "./r3d_core_state.h"

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool compile_shader(R3D_ScreenShader* shader);

// ========================================
// PUBLIC API
// ========================================

R3D_ScreenShader* R3D_LoadScreenShader(const char* filePath)
{
    char* code = LoadFileText(filePath);
    if (code == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load screen shader; Unable to load shader file");
        return NULL;
    }

    R3D_ScreenShader* shader = R3D_LoadScreenShaderFromMemory(code);
    UnloadFileText(code);

    return shader;
}

R3D_ScreenShader* R3D_LoadScreenShaderFromMemory(const char* code)
{
    size_t userCodeLen = strlen(code);
    if (userCodeLen > R3D_MAX_SHADER_CODE_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load screen shader; User code too long");
        return NULL;
    }

    if (!strstr(code, "void fragment()")) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load screen shader; Missing fragment() entry point");
        return NULL;
    }

    R3D_ScreenShader* shader = r3d_shader_custom_alloc();
    if (!shader) {
        R3D_TRACELOG(LOG_ERROR, "Bad alloc during screen shader loading");
        return NULL;
    }

    int uniformCount = 0;
    int samplerCount = 0;
    int currentOffset = 0;

    r3d_rshade_parsed_function_t fragmentFunc = {0};

    /* --- PHASE 1: Parse user code and collect metadata --- */

    const char* ptr = code;

    while (*ptr)
    {
        r3d_rshade_skip_whitespace_and_comments(&ptr);
        if (!*ptr) break;

        // Parse uniform declarations
        if (r3d_rshade_match_keyword(ptr, "uniform", 7)) {
            ptr += 7;
            r3d_rshade_parse_uniform(&ptr,
                shader->data.samplers,
                &shader->data.uniforms,
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

    r3d_rshade_writer_t writer = {
        .ptr       = shader->program->userCode,
        .remaining = R3D_MAX_SHADER_CODE_LENGTH,
        .overflow  = false,
    };

    // Write uniform block and samplers
    r3d_rshade_write_uniform_block(&writer, shader->data.uniforms.entries, uniformCount);
    r3d_rshade_write_samplers(&writer, shader->data.samplers, samplerCount);

    // Copy global code (excluding comments, uniforms, fragment()) then write fragment stage section
    r3d_rshade_copy_global_code(&writer, code, false, NULL, &fragmentFunc);
    r3d_rshade_write_shader_function(&writer, "fragment", &fragmentFunc);

    if (writer.overflow) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load screen shader; Transformed code too long");
        MemFree(shader);
        return NULL;
    }

    *writer.ptr = '\0';

    /* --- PHASE 3: Compile shader --- */

    if (!compile_shader(shader)) {
        R3D_UnloadScreenShader(shader);
        return NULL;
    }

    /* --- PHASE 4: Initialize uniform buffer --- */

    r3d_shader_custom_init_uniforms(shader, currentOffset);

    R3D_TRACELOG(LOG_INFO, "Screen shader loaded successfully");
    R3D_TRACELOG(LOG_INFO, "    > Sampler count: %i", samplerCount);
    R3D_TRACELOG(LOG_INFO, "    > Uniform count: %i", uniformCount);

    return shader;
}

R3D_ScreenShader* R3D_LoadScreenShaderAlias(R3D_ScreenShader* shader)
{
    R3D_ScreenShader* alias = r3d_shader_custom_clone(shader);
    if (!alias) {
        R3D_TRACELOG(LOG_ERROR, "Bad alloc during screen shader alias loading");
        return NULL;
    }
    return alias;
}

void R3D_UnloadScreenShader(R3D_ScreenShader* shader)
{
    r3d_shader_custom_free(shader);
}

void R3D_SetScreenShaderUniform(R3D_ScreenShader* shader, const char* name, const void* value)
{
    if (!shader) {
        R3D_TRACELOG(LOG_WARNING, "Cannot set uniform '%s' on NULL screen shader", name);
        return;
    }

    if (!r3d_shader_custom_set_uniform(shader, name, value)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to set custom uniform '%s'", name);
    }
}

void R3D_SetScreenShaderSampler(R3D_ScreenShader* shader, const char* name, Texture texture)
{
    if (!shader) {
        R3D_TRACELOG(LOG_WARNING, "Cannot set sampler '%s' on NULL screen shader", name);
        return;
    }

    if (!r3d_shader_custom_set_sampler(shader, name, texture)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to set custom sampler '%s'", name);
    }
}

void R3D_SetScreenShaderChain(R3D_ScreenShaderStage stage, R3D_ScreenShader** shaders, int count)
{
    if (stage < 0 || stage >= R3D_SCREEN_SHADER_STAGE_COUNT) {
        R3D_TRACELOG(LOG_WARNING, "Invalid screen shader stage: %d", stage);
    }

    size_t maxChain = R3D_ARRAY_SIZE(R3D.screenShaders[stage]);

    if (count > (int)maxChain) {
        R3D_TRACELOG(
            LOG_WARNING,
            "Too many screen shaders provided for chain: %d / %zu",
            count,
            maxChain
        );

        count = (int)maxChain;
    }

    memset(R3D.screenShaders[stage], 0, sizeof(R3D.screenShaders[stage]));
    if (shaders == NULL) return;

    for (int i = 0; i < count; i++) {
        R3D.screenShaders[stage][i] = shaders[i];
    }
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

bool compile_shader(R3D_ScreenShader* shader)
{
    bool ok = R3D_MOD_SHADER_LOADER.post.screen(shader);
    if (!ok) {
        R3D_TRACELOG(LOG_ERROR, "Failed to compile screen shader");
    }
    return ok;
}
