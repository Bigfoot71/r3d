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
#include <ctype.h>
#include <glad.h>

#include "./modules/r3d_shader.h"

// ========================================
// INTERNAL STRUCTURES
// ========================================

typedef struct {
    char qualifier[16];
    char type[R3D_SHADER_MAX_VAR_TYPE_LENGTH];
    char name[R3D_SHADER_MAX_VAR_NAME_LENGTH];
} varying_t;

typedef struct {
    const char* bodyStart;
    const char* bodyEnd;
} parsed_function_t;

// ========================================
// OPAQUE STRUCTURES
// ========================================

struct R3D_SurfaceShader {
    r3d_shader_custom_t program;
    char userCode[R3D_CUSTOM_SHADER_USER_CODE_MAX_LENGTH];
};

// ========================================
// INTERNAL FUNCTIONS
// ========================================

/* Check if the code contains at least one `vertex()` or `fragment()` function */
static bool has_minimal_shader_entry(const char* userCode);

/* Returns the size of a GLSL type */
static int get_type_size(const char* type);

/* Returns the texture target according to a GLSL sampler type */
static GLenum get_sampler_target(const char* type);

/* Returns the alignment for an offset, used for UBOs */
static int align_offset(int offset, int align);

/* Skip to next semicolon and advance past it */
static void skip_to_semicolon(const char** ptr);

/* Skip to end of line */
static void skip_to_end_of_line(const char** ptr);

/* Skip to opening brace */
static void skip_to_brace(const char** ptr);

/* Skip to the closing brace of a function body */
static void skip_to_matching_brace(const char** ptr);

/* Skip whitespace */
static void skip_whitespace(const char** ptr);

/* Skip whitespace and all types of comments */
static void skip_whitespace_and_comments(const char** ptr);

/* Check if current position starts with keyword followed by whitespace */
static bool match_keyword(const char* ptr, const char* keyword, size_t len);

/* Parse an identifier (stopping at whitespace, semicolon, or bracket) */
static bool parse_identifier(const char** ptr, char* output, size_t maxLen);

/* Parse a GLSL declaration (type + name) and advance pointer past semicolon */
static bool parse_declaration(const char** ptr, char* type, char* name);

/* Parse varying declaration with optional interpolation qualifier */
static bool parse_varying_declaration(const char** ptr, varying_t* varying);

/* Check if current position is the start of a vertex() or fragment() function */
static parsed_function_t* check_shader_function(const char* ptr, parsed_function_t* vertexFunc, parsed_function_t* fragmentFunc);

/* Write varyings with specified qualifier (in/out) */
static char* write_varyings(char* outPtr, const char* qualifier, varying_t* varyings, int count);

/* Write a shader function body (vertex or fragment) */
static char* write_shader_function(char* outPtr, const char* name, parsed_function_t* func);

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
    if (userCodeLen > R3D_CUSTOM_SHADER_USER_CODE_MAX_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load surface shader; User code too long");
        return NULL;
    }

    if (!has_minimal_shader_entry(code)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load surface shader; Missing entry points");
        return NULL;
    }

    R3D_SurfaceShader* shader = RL_CALLOC(1, sizeof(R3D_SurfaceShader));
    if (shader == NULL) {
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

    varying_t varyings[32] = {0};
    parsed_function_t vertexFunc = {0};
    parsed_function_t fragmentFunc = {0};

    /* --- PHASE 1: Parse user code and collect metadata --- */

    while (*ptr)
    {
        skip_whitespace_and_comments(&ptr);
        if (!*ptr) break;  // End of code

        // Parse uniform declarations
        if (match_keyword(ptr, "uniform", 7))
        {
            ptr += 7;
            char type[R3D_SHADER_MAX_VAR_TYPE_LENGTH], name[R3D_SHADER_MAX_VAR_NAME_LENGTH];
            if (!parse_declaration(&ptr, type, name)) continue;

            // Check if it's a sampler
            GLenum samplerTarget = get_sampler_target(type);
            if (samplerTarget != 0) {
                if (samplerCount < R3D_CUSTOM_SHADER_MAX_SAMPLERS) {
                    strncpy(shader->program.samplers[samplerCount].name, name, R3D_SHADER_MAX_VAR_NAME_LENGTH - 1);
                    shader->program.samplers[samplerCount].name[R3D_SHADER_MAX_VAR_NAME_LENGTH - 1] = '\0';
                    shader->program.samplers[samplerCount].target = samplerTarget;
                    shader->program.samplers[samplerCount++].texture = 0;
                }
            }
            else {
                // It's a uniform value, add to UBO
                int size = get_type_size(type);
                if (size > 0 && uniformCount < R3D_CUSTOM_SHADER_MAX_UNIFORMS) {
                    int alignment = (size >= 16) ? 16 : size;
                    currentOffset = align_offset(currentOffset, alignment);
                    strncpy(shader->program.uniforms.entries[uniformCount].name, name, R3D_SHADER_MAX_VAR_NAME_LENGTH - 1);
                    shader->program.uniforms.entries[uniformCount].name[R3D_SHADER_MAX_VAR_NAME_LENGTH - 1] = '\0';
                    strncpy(shader->program.uniforms.entries[uniformCount].type, type, R3D_SHADER_MAX_VAR_TYPE_LENGTH - 1);
                    shader->program.uniforms.entries[uniformCount].type[R3D_SHADER_MAX_VAR_TYPE_LENGTH - 1] = '\0';
                    shader->program.uniforms.entries[uniformCount].offset = currentOffset;
                    shader->program.uniforms.entries[uniformCount++].size = size;
                    currentOffset += size;
                }
            }
            continue;
        }

        // Parse varying declarations
        if (match_keyword(ptr, "varying", 7)) {
            ptr += 7;
            if (varyingCount < 32) {
                if (parse_varying_declaration(&ptr, &varyings[varyingCount])) {
                    varyingCount++;
                }
            }
            else {
                skip_to_semicolon(&ptr);
            }
            continue;
        }

        // Parse vertex() and fragment() functions
        parsed_function_t* func = check_shader_function(ptr, &vertexFunc, &fragmentFunc);
        if (func) {
            skip_to_brace(&ptr);
            if (*ptr == '{') {
                func->bodyStart = ptr;
                skip_to_matching_brace(&ptr);
                func->bodyEnd = ptr;
            }
            continue;
        }

        ptr++;
    }

    /* --- PHASE 2: Generate transformed shader code --- */

    // Write uniform block (UBO)
    if (uniformCount > 0) {
        outPtr += sprintf(outPtr, "layout(std140) uniform UserBlock {\n");
        for (int i = 0; i < uniformCount; i++) {
            outPtr += sprintf(outPtr, "    %s %s;\n", 
                shader->program.uniforms.entries[i].type, 
                shader->program.uniforms.entries[i].name);
        }
        outPtr += sprintf(outPtr, "};\n\n");
    }

    // Write sampler declarations
    for (int i = 0; i < samplerCount; i++) {
        const char* typeStr;
        switch (shader->program.samplers[i].target) {
            case GL_TEXTURE_1D:        typeStr = "sampler1D"; break;
            case GL_TEXTURE_2D:        typeStr = "sampler2D"; break;
            case GL_TEXTURE_3D:        typeStr = "sampler3D"; break;
            case GL_TEXTURE_CUBE_MAP:  typeStr = "samplerCube"; break;
            default:                   typeStr = "sampler2D"; break;
        }
        outPtr += sprintf(outPtr, "uniform %s %s;\n", typeStr, shader->program.samplers[i].name);
    }
    if (samplerCount > 0) outPtr += sprintf(outPtr, "\n");

    // Copy global code (functions, etc.) excluding comments, uniforms, varyings, and entry points 
    ptr = code;
    while (*ptr)
    {
        // Check for comments before copying
        if (strncmp(ptr, "//", 2) == 0) {
            skip_to_end_of_line(&ptr);
            continue;
        }

        if (strncmp(ptr, "/*", 2) == 0) {
            ptr += 2;
            while (*ptr && strncmp(ptr, "*/", 2) != 0) ptr++;
            if (*ptr) ptr += 2;
            continue;
        }

        const char* lineStart = ptr;
        skip_whitespace(&ptr);

        bool skip = false;

        // Skip uniforms and varyings
        if (match_keyword(ptr, "uniform", 7) || match_keyword(ptr, "varying", 7)) {
            skip = true;
        }
        // Skip vertex() and fragment() functions
        else if (check_shader_function(ptr, &vertexFunc, &fragmentFunc)) {
            skip = true;
            skip_to_matching_brace(&ptr);
            continue;
        }

        if (skip) {
            skip_to_semicolon(&ptr);
        }
        else {
            // Copy whitespace and content
            while (lineStart < ptr) *outPtr++ = *lineStart++;
            if (*ptr) *outPtr++ = *ptr++;
        }
    }

    // Write vertex stage section
    outPtr += sprintf(outPtr, "\n#ifdef STAGE_VERT\n\n");
    outPtr = write_varyings(outPtr, "out", varyings, varyingCount);
    outPtr = write_shader_function(outPtr, "vertex", &vertexFunc);
    outPtr += sprintf(outPtr, "\n#endif\n\n");

    // Write fragment stage section
    outPtr += sprintf(outPtr, "#ifdef STAGE_FRAG\n\n");
    outPtr = write_varyings(outPtr, "in", varyings, varyingCount);
    outPtr = write_shader_function(outPtr, "fragment", &fragmentFunc);
    outPtr += sprintf(outPtr, "\n#endif\n");

    *outPtr = '\0';

    // Copy transformed code to shader structure
    int finalLen = strlen(output);
    if (finalLen > R3D_CUSTOM_SHADER_USER_CODE_MAX_LENGTH) {
        R3D_TRACELOG(LOG_ERROR, "Failed to load surface shader; Transformed code too long");
        RL_FREE(output);
        RL_FREE(shader);
        return NULL;
    }

    memcpy(shader->userCode, output, finalLen + 1);
    RL_FREE(output);

    // Store reference to the user code in custom shader struct
    shader->program.userCode = shader->userCode;

    /* --- PHASE 3: Pre-compile a shader to check user code --- */

    bool success = false;
    R3D_SHADER_CUSTOM_PRELOAD(&shader->program, scene.geometry, &success);
    if (!success) {
        R3D_TRACELOG(LOG_ERROR, "Failed to compile surface shader");
        RL_FREE(shader);
        return NULL;
    }

    /* --- PHASE 4: Initialize uniform buffer --- */

    memset(shader->program.uniforms.buffer, 0, sizeof(shader->program.uniforms.buffer));
    int uboSize = (currentOffset < 16) ? 16 : currentOffset;
    shader->program.uniforms.bufferSize = uboSize;
    shader->program.uniforms.dirty = true;

    glGenBuffers(1, &shader->program.uniforms.bufferId);
    glBindBuffer(GL_UNIFORM_BUFFER, shader->program.uniforms.bufferId);
    glBufferData(GL_UNIFORM_BUFFER, uboSize, shader->program.uniforms.buffer, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return shader;
}

void R3D_UnloadSurfaceShader(R3D_SurfaceShader* shader)
{
    glDeleteBuffers(1, &shader->program.uniforms.bufferId);
    glDeleteProgram(shader->program.scene.geometry.id);
    glDeleteProgram(shader->program.scene.forward.id);
    glDeleteProgram(shader->program.scene.depth.id);
    glDeleteProgram(shader->program.scene.depthCube.id);
    glDeleteProgram(shader->program.scene.probe.id);
    glDeleteProgram(shader->program.scene.decal.id);

    RL_FREE(shader);
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

bool has_minimal_shader_entry(const char* userCode)
{
    if (strstr(userCode, "void vertex()") != NULL) {
        return true;
    }

    if (strstr(userCode, "void fragment()") != NULL) {
        return true;
    }

    return false;
}

int get_type_size(const char* type)
{
    if (strcmp(type, "bool") == 0) return 4;
    if (strcmp(type, "int") == 0) return 4;
    if (strcmp(type, "float") == 0) return 4;

    if (strcmp(type, "ivec2") == 0) return 8;
    if (strcmp(type, "ivec3") == 0) return 12;
    if (strcmp(type, "ivec4") == 0) return 16;

    if (strcmp(type, "vec2") == 0) return 8;
    if (strcmp(type, "vec3") == 0) return 12;
    if (strcmp(type, "vec4") == 0) return 16;

    if (strcmp(type, "mat2") == 0) return 32;
    if (strcmp(type, "mat3") == 0) return 48;
    if (strcmp(type, "mat4") == 0) return 64;

    return 0;
}

GLenum get_sampler_target(const char* type)
{
    if (strcmp(type, "sampler1D") == 0) return GL_TEXTURE_1D;
    if (strcmp(type, "sampler2D") == 0) return GL_TEXTURE_2D;
    if (strcmp(type, "sampler3D") == 0) return GL_TEXTURE_3D;
    if (strcmp(type, "samplerCube") == 0) return GL_TEXTURE_CUBE_MAP;

    return 0;
}

int align_offset(int offset, int align)
{
    return (offset + align - 1) & ~(align - 1);
}

void skip_to_semicolon(const char** ptr)
{
    while (**ptr && **ptr != ';') (*ptr)++;
    if (**ptr == ';') (*ptr)++;
}

void skip_to_end_of_line(const char** ptr)
{
    while (**ptr && **ptr != '\n') (*ptr)++;
    if (**ptr == '\n') (*ptr)++;
}

void skip_to_brace(const char** ptr)
{
    while (**ptr && **ptr != '{') (*ptr)++;
}

void skip_to_matching_brace(const char** ptr)
{
    int braces = 0;
    while (**ptr) {
        if (**ptr == '{') braces++;
        if (**ptr == '}') {
            braces--;
            if (braces == 0) {
                (*ptr)++;
                return;
            }
        }
        (*ptr)++;
    }
}

void skip_whitespace(const char** ptr)
{
    while (**ptr && isspace(**ptr)) (*ptr)++;
}

void skip_whitespace_and_comments(const char** ptr)
{
    while (**ptr)
    {
        // Skip whitespace
        if (isspace(**ptr)) {
            (*ptr)++;
            continue;
        }

        // Skip single-line comments
        if (strncmp(*ptr, "//", 2) == 0) {
            skip_to_end_of_line(ptr);
            continue;
        }

        // Skip multi-line comments
        if (strncmp(*ptr, "/*", 2) == 0) {
            *ptr += 2;
            while (**ptr) {
                if (strncmp(*ptr, "*/", 2) == 0) {
                    *ptr += 2;
                    break;
                }
                (*ptr)++;
            }
            continue;
        }

        // No more whitespace or comments
        break;
    }
}

bool match_keyword(const char* ptr, const char* keyword, size_t len)
{
    return strncmp(ptr, keyword, len) == 0 && isspace(ptr[len]);
}

bool parse_identifier(const char** ptr, char* output, size_t maxLen)
{
    skip_whitespace(ptr);

    size_t i = 0;
    while (**ptr && !isspace(**ptr) && **ptr != ';' && **ptr != '[' && i < maxLen - 1) {
        output[i++] = *(*ptr)++;
    }
    output[i] = '\0';

    return i > 0;
}

bool parse_declaration(const char** ptr, char* type, char* name)
{
    // Parse type
    if (!parse_identifier(ptr, type, R3D_SHADER_MAX_VAR_TYPE_LENGTH)) {
        skip_to_semicolon(ptr);
        return false;
    }

    // Parse name
    if (!parse_identifier(ptr, name, R3D_SHADER_MAX_VAR_NAME_LENGTH)) {
        skip_to_semicolon(ptr);
        return false;
    }

    skip_to_semicolon(ptr);
    return true;
}

bool parse_varying_declaration(const char** ptr, varying_t* varying)
{
    skip_whitespace(ptr);

    varying->qualifier[0] = '\0';

    // Check for interpolation qualifiers
    static const struct {const char* name; size_t len;}
    qualifiers[] = {{"smooth", 6}, {"flat", 4}, {"noperspective", 14}};

    for (int i = 0; i < 3; i++) {
        if (strncmp(*ptr, qualifiers[i].name, qualifiers[i].len) == 0 && 
            isspace((*ptr)[qualifiers[i].len])) 
        {
            strncpy(varying->qualifier, qualifiers[i].name, sizeof(varying->qualifier) - 1);
            varying->qualifier[sizeof(varying->qualifier) - 1] = '\0';
            *ptr += qualifiers[i].len;
            skip_whitespace(ptr);
            break;
        }
    }

    return parse_declaration(ptr, varying->type, varying->name);
}

parsed_function_t* check_shader_function(const char* ptr, parsed_function_t* vertexFunc, parsed_function_t* fragmentFunc)
{
    if (strncmp(ptr, "void", 4) != 0 || !isspace(ptr[4])) {
        return NULL;
    }
    
    const char* ahead = ptr + 4;
    while (*ahead && isspace(*ahead)) ahead++;
    
    if (strncmp(ahead, "vertex", 6) == 0) return vertexFunc;
    if (strncmp(ahead, "fragment", 8) == 0) return fragmentFunc;
    
    return NULL;
}

char* write_varyings(char* outPtr, const char* qualifier, varying_t* varyings, int count)
{
    for (int i = 0; i < count; i++) {
        if (varyings[i].qualifier[0] != '\0') {
            outPtr += sprintf(outPtr, "%s %s %s %s;\n", 
                varyings[i].qualifier, qualifier, varyings[i].type, varyings[i].name);
        }
        else {
            outPtr += sprintf(outPtr, "%s %s %s;\n", 
                qualifier, varyings[i].type, varyings[i].name);
        }
    }
    if (count > 0) outPtr += sprintf(outPtr, "\n");
    return outPtr;
}

char* write_shader_function(char* outPtr, const char* name, parsed_function_t* func)
{
    if (func->bodyStart) {
        outPtr += sprintf(outPtr, "void %s() ", name);
        while (func->bodyStart < func->bodyEnd) {
            *outPtr++ = *func->bodyStart++;
        }
        outPtr += sprintf(outPtr, "\n");
    }
    return outPtr;
}
