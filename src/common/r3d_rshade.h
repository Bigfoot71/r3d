/* r3d_rshade.h -- Common Ray-Shade functions
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_RSHADE_H
#define R3D_RSHADE_H

#include <r3d_config.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <glad.h>

#include "./r3d_helper.h"

// ========================================
// CONSTANTS
// ========================================

#define R3D_RSHADE_MAX_VAR_TYPE_LENGTH 32
#define R3D_RSHADE_MAX_VAR_NAME_LENGTH 64

// ========================================
// STRUCT TYPES
// ========================================

typedef struct {
    char qualifier[16];
    char type[R3D_RSHADE_MAX_VAR_TYPE_LENGTH];
    char name[R3D_RSHADE_MAX_VAR_NAME_LENGTH];
} r3d_rshade_varying_t;

typedef struct {
    char name[R3D_RSHADE_MAX_VAR_NAME_LENGTH];
    GLenum target;
    GLuint texture;
} r3d_rshade_sampler_t;

typedef struct {
    char type[R3D_RSHADE_MAX_VAR_TYPE_LENGTH];
    char name[R3D_RSHADE_MAX_VAR_NAME_LENGTH];
    int offset;
    int size;
} r3d_rshade_uniform_t;

typedef struct {
    r3d_rshade_uniform_t entries[R3D_MAX_SHADER_UNIFORMS];
    uint8_t buffer[R3D_MAX_SHADER_UNIFORMS * 64];
    GLuint bufferId;
    int bufferSize;
    bool dirty;
} r3d_rshade_uniform_buffer_t;

typedef struct {
    const char* bodyStart;
    const char* bodyEnd;
} r3d_rshade_parsed_function_t;

// ========================================
// INLINE FUNCTIONS
// ========================================

/* Returns the size in bytes of a GLSL type (std140 layout) */
static inline int r3d_rshade_get_type_size(const char* type)
{
    // Scalar types
    if (strcmp(type, "bool") == 0)  return 4;
    if (strcmp(type, "int") == 0)   return 4;
    if (strcmp(type, "float") == 0) return 4;

    // Integer vectors
    if (strcmp(type, "ivec2") == 0) return 8;
    if (strcmp(type, "ivec3") == 0) return 12;
    if (strcmp(type, "ivec4") == 0) return 16;

    // Float vectors
    if (strcmp(type, "vec2") == 0) return 8;
    if (strcmp(type, "vec3") == 0) return 12;
    if (strcmp(type, "vec4") == 0) return 16;

    // Matrices
    if (strcmp(type, "mat2") == 0) return 32;
    if (strcmp(type, "mat3") == 0) return 48;
    if (strcmp(type, "mat4") == 0) return 64;

    return 0;
}

/* Returns the OpenGL texture target for a GLSL sampler type */
static inline GLenum r3d_rshade_get_sampler_target(const char* type)
{
    if (strcmp(type, "sampler1D") == 0)   return GL_TEXTURE_1D;
    if (strcmp(type, "sampler2D") == 0)   return GL_TEXTURE_2D;
    if (strcmp(type, "sampler3D") == 0)   return GL_TEXTURE_3D;
    if (strcmp(type, "samplerCube") == 0) return GL_TEXTURE_CUBE_MAP;

    return 0;
}

/* Skip to next semicolon and advance past it */
static inline void r3d_rshade_skip_to_semicolon(const char** ptr)
{
    while (**ptr && **ptr != ';') (*ptr)++;
    if (**ptr == ';') (*ptr)++;
}

/* Skip to end of line */
static inline void r3d_rshade_skip_to_end_of_line(const char** ptr)
{
    while (**ptr && **ptr != '\n') (*ptr)++;
    if (**ptr == '\n') (*ptr)++;
}

/* Skip to opening brace */
static inline void r3d_rshade_skip_to_brace(const char** ptr)
{
    while (**ptr && **ptr != '{') (*ptr)++;
}

/* Skip to the matching closing brace */
static inline void r3d_rshade_skip_to_matching_brace(const char** ptr)
{
    int depth = 0;
    while (**ptr) {
        if (**ptr == '{') depth++;
        else if (**ptr == '}') {
            if (--depth == 0) {
                (*ptr)++;
                return;
            }
        }
        (*ptr)++;
    }
}

/* Skip whitespace characters */
static inline void r3d_rshade_skip_whitespace(const char** ptr)
{
    while (**ptr && isspace(**ptr)) (*ptr)++;
}

/* Skip whitespace and all comment types (single-line and multi-line) */
static inline void r3d_rshade_skip_whitespace_and_comments(const char** ptr)
{
    while (**ptr)
    {
        if (isspace(**ptr)) {
            (*ptr)++;
            continue;
        }

        if (strncmp(*ptr, "//", 2) == 0) {
            r3d_rshade_skip_to_end_of_line(ptr);
            continue;
        }

        if (strncmp(*ptr, "/*", 2) == 0) {
            *ptr += 2;
            while (**ptr && strncmp(*ptr, "*/", 2) != 0) (*ptr)++;
            if (**ptr) *ptr += 2;
            continue;
        }

        break;
    }
}

/* Check if current position matches a keyword followed by whitespace */
static inline bool r3d_rshade_match_keyword(const char* ptr, const char* keyword, size_t len)
{
    return strncmp(ptr, keyword, len) == 0 && isspace(ptr[len]);
}

/* Check if current position is a varying keyword (varying, flat, smooth, noperspective) */
static inline bool r3d_rshade_match_varying_keyword(const char* ptr)
{
    return r3d_rshade_match_keyword(ptr, "varying", 7)        ||
           r3d_rshade_match_keyword(ptr, "flat", 4)           ||
           r3d_rshade_match_keyword(ptr, "smooth", 6)         ||
           r3d_rshade_match_keyword(ptr, "noperspective", 13);
}

/* Parse an identifier (stops at whitespace, semicolon, or bracket) */
static inline bool r3d_rshade_parse_identifier(const char** ptr, char* output, size_t maxLen)
{
    r3d_rshade_skip_whitespace(ptr);

    size_t i = 0;
    while (**ptr && !isspace(**ptr) && **ptr != ';' && **ptr != '[' && i < maxLen - 1) {
        output[i++] = *(*ptr)++;
    }
    output[i] = '\0';

    return i > 0;
}

/* Parse a GLSL declaration (type + name) and advance past semicolon */
static inline bool r3d_rshade_parse_declaration(const char** ptr, char* type, char* name)
{
    if (!r3d_rshade_parse_identifier(ptr, type, R3D_RSHADE_MAX_VAR_TYPE_LENGTH)) {
        r3d_rshade_skip_to_semicolon(ptr);
        return false;
    }

    if (!r3d_rshade_parse_identifier(ptr, name, R3D_RSHADE_MAX_VAR_NAME_LENGTH)) {
        r3d_rshade_skip_to_semicolon(ptr);
        return false;
    }

    r3d_rshade_skip_to_semicolon(ptr);
    return true;
}

/* Parse varying declaration with optional interpolation qualifier */
static inline bool r3d_rshade_parse_varying(const char** ptr, r3d_rshade_varying_t* varying)
{
    varying->qualifier[0] = '\0';

    // Check for interpolation qualifier before "varying"
    size_t qualLen = 0;
    if      (strncmp(*ptr, "flat", 4) == 0 && isspace((*ptr)[4]))            qualLen = 4;
    else if (strncmp(*ptr, "noperspective", 13) == 0 && isspace((*ptr)[13])) qualLen = 13;
    else if (strncmp(*ptr, "smooth", 6) == 0 && isspace((*ptr)[6]))          qualLen = 6;

    if (qualLen > 0) {
        memcpy(varying->qualifier, *ptr, qualLen);
        varying->qualifier[qualLen] = '\0';
        *ptr += qualLen;
        r3d_rshade_skip_whitespace(ptr);

        // Must be followed by "varying"
        if (!r3d_rshade_match_keyword(*ptr, "varying", 7)) {
            r3d_rshade_skip_to_semicolon(ptr);
            return false;
        }
    }

    // Skip "varying" keyword
    *ptr += 7;

    return r3d_rshade_parse_declaration(ptr, varying->type, varying->name);
}

/* Parse uniform declaration and add to samplers or uniform buffer */
static inline bool r3d_rshade_parse_uniform(const char** ptr,
    r3d_rshade_sampler_t* samplers, r3d_rshade_uniform_buffer_t* uniforms,
    int* samplerCount, int* uniformCount, int* currentOffset,
    int maxSamplers, int maxUniforms)
{
    char type[R3D_RSHADE_MAX_VAR_TYPE_LENGTH];
    char name[R3D_RSHADE_MAX_VAR_NAME_LENGTH];
    
    if (!r3d_rshade_parse_declaration(ptr, type, name)) {
        return false;
    }

    // Check if it's a sampler
    GLenum samplerTarget = r3d_rshade_get_sampler_target(type);
    if (samplerTarget != 0) {
        if (*samplerCount < maxSamplers) {
            r3d_rshade_sampler_t* s = &samplers[*samplerCount];
            strncpy(s->name, name, R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1);
            s->name[R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1] = '\0';
            s->target = samplerTarget;
            s->texture = 0;
            (*samplerCount)++;
        }
        return true;
    }

    // It's a uniform value, add to UBO with std140 alignment
    int size = r3d_rshade_get_type_size(type);
    if (size > 0 && *uniformCount < maxUniforms) {
        int alignment = (size >= 16) ? 16 : size;
        *currentOffset = r3d_align_offset(*currentOffset, alignment);
        
        r3d_rshade_uniform_t* u = &uniforms->entries[*uniformCount];
        strncpy(u->name, name, R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1);
        u->name[R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1] = '\0';
        strncpy(u->type, type, R3D_RSHADE_MAX_VAR_TYPE_LENGTH - 1);
        u->type[R3D_RSHADE_MAX_VAR_TYPE_LENGTH - 1] = '\0';
        u->offset = *currentOffset;
        u->size = size;
        
        (*uniformCount)++;
        *currentOffset += size;
    }
    
    return true;
}

/* Check if current position is vertex() or fragment() function entry point */
static inline r3d_rshade_parsed_function_t* r3d_rshade_check_shader_entry(const char* ptr,
    r3d_rshade_parsed_function_t* vertexFunc, r3d_rshade_parsed_function_t* fragmentFunc)
{
    if (!r3d_rshade_match_keyword(ptr, "void", 4)) {
        return NULL;
    }

    const char* ahead = ptr + 4;
    while (*ahead && isspace(*ahead)) ahead++;

    if (strncmp(ahead, "vertex", 6) == 0)   return vertexFunc;
    if (strncmp(ahead, "fragment", 8) == 0) return fragmentFunc;

    return NULL;
}

/* Check if line should be skipped during global code copy */
static inline bool r3d_rshade_should_skip_line(const char* ptr, bool hasVaryings,
    r3d_rshade_parsed_function_t* vertexFunc, r3d_rshade_parsed_function_t* fragmentFunc)
{
    return strncmp(ptr, "#pragma", 7) == 0 ||
           r3d_rshade_match_keyword(ptr, "uniform", 7) ||
           (hasVaryings && r3d_rshade_match_varying_keyword(ptr)) ||
           (r3d_rshade_check_shader_entry(ptr, vertexFunc, fragmentFunc) != NULL);
}

/* Copy global code while skipping uniforms, varyings, and entry points */
static inline char* r3d_rshade_copy_global_code(char* outPtr, const char* code, bool hasVaryings,
    r3d_rshade_parsed_function_t* vertexFunc, r3d_rshade_parsed_function_t* fragmentFunc)
{
    const char* ptr = code;

    while (*ptr)
    {
        // Skip comments
        if (strncmp(ptr, "//", 2) == 0) {
            r3d_rshade_skip_to_end_of_line(&ptr);
            continue;
        }

        if (strncmp(ptr, "/*", 2) == 0) {
            ptr += 2;
            while (*ptr && strncmp(ptr, "*/", 2) != 0) ptr++;
            if (*ptr) ptr += 2;
            continue;
        }

        const char* lineStart = ptr;
        r3d_rshade_skip_whitespace(&ptr);

        // Check if line should be skipped
        if (r3d_rshade_should_skip_line(ptr, hasVaryings, vertexFunc, fragmentFunc)) {
            if (r3d_rshade_check_shader_entry(ptr, vertexFunc, fragmentFunc)) {
                r3d_rshade_skip_to_matching_brace(&ptr);
            }
            else {
                r3d_rshade_skip_to_semicolon(&ptr);
            }
        }
        else {
            // Copy whitespace and content
            while (lineStart < ptr) *outPtr++ = *lineStart++;
            if (*ptr) *outPtr++ = *ptr++;
        }
    }

    return outPtr;
}

/* Write varyings with in/out qualifier */
static inline char* r3d_rshade_write_varyings(char* outPtr, const char* inout, r3d_rshade_varying_t* varyings, int count)
{
    for (int i = 0; i < count; i++) {
        if (varyings[i].qualifier[0] != '\0') {
            outPtr += sprintf(outPtr, "%s %s %s %s;\n",
                varyings[i].qualifier, inout, varyings[i].type, varyings[i].name);
        }
        else {
            outPtr += sprintf(outPtr, "%s %s %s;\n",
                inout, varyings[i].type, varyings[i].name);
        }
    }
    if (count > 0) *outPtr++ = '\n';
    return outPtr;
}

/* Write sampler uniform declarations */
static inline char* r3d_rshade_write_samplers(char* outPtr, r3d_rshade_sampler_t* samplers, int count)
{
    for (int i = 0; i < count; i++) {
        const char* typeStr;
        switch (samplers[i].target) {
            case GL_TEXTURE_1D:       typeStr = "sampler1D"; break;
            case GL_TEXTURE_2D:       typeStr = "sampler2D"; break;
            case GL_TEXTURE_3D:       typeStr = "sampler3D"; break;
            case GL_TEXTURE_CUBE_MAP: typeStr = "samplerCube"; break;
            default:                  typeStr = "sampler2D"; break;
        }
        outPtr += sprintf(outPtr, "uniform %s %s;\n", typeStr, samplers[i].name);
    }
    if (count > 0) *outPtr++ = '\n';
    return outPtr;
}

/* Write uniform block (UBO) declaration */
static inline char* r3d_rshade_write_uniform_block(char* outPtr, r3d_rshade_uniform_t* entries, int count)
{
    if (count <= 0) return outPtr;

    outPtr += sprintf(outPtr, "layout(std140) uniform UserBlock {\n");
    for (int i = 0; i < count; i++) {
        outPtr += sprintf(outPtr, "    %s %s;\n", entries[i].type, entries[i].name);
    }
    outPtr += sprintf(outPtr, "};\n\n");

    return outPtr;
}

/* Write shader function body (vertex or fragment) */
static inline char* r3d_rshade_write_shader_function(char* outPtr, const char* name, r3d_rshade_parsed_function_t* func)
{
    if (!func->bodyStart) return outPtr;

    outPtr += sprintf(outPtr, "void %s() ", name);
    while (func->bodyStart < func->bodyEnd) {
        *outPtr++ = *func->bodyStart++;
    }
    *outPtr++ = '\n';

    return outPtr;
}

/* Initialize OpenGL uniform buffer object (UBO) */
static inline void r3d_rshade_init_ubo(r3d_rshade_uniform_buffer_t* uniforms, int currentOffset)
{
    if (uniforms->entries[0].name[0] == '\0') return;

    // Ensure minimum UBO size of 16 bytes (OpenGL requirement)
    int uboSize = (currentOffset < 16) ? 16 : currentOffset;
    uniforms->bufferSize = uboSize;
    uniforms->dirty = true;

    glGenBuffers(1, &uniforms->bufferId);
    glBindBuffer(GL_UNIFORM_BUFFER, uniforms->bufferId);
    glBufferData(GL_UNIFORM_BUFFER, uboSize, uniforms->buffer, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

#endif // R3D_RSHADE_H
