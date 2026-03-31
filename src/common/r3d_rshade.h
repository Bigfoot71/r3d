/* r3d_rshade.h -- Common Ray-Shade functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
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

/* Returns the std140 alignment requirement for a given type size */
static inline int r3d_rshade_get_std140_alignment(int size)
{
    size--;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    return size + 1;
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

/* Count comma-separated arguments at depth 1 inside a constructor.
 * ptr must point to the character just after the opening '('. */
static inline int r3d_rshade_count_constructor_args(const char* ptr)
{
    int depth = 1;
    int count = 0;
    bool hasContent = false;

    while (*ptr && depth > 0)
    {
        if (*ptr == '(') {
            depth++;
            hasContent = true;
        }
        else if (*ptr == ')') {
            if (depth == 1 && hasContent) count++;
            depth--;
        }
        else if (*ptr == ',' && depth == 1) {
            count++;
            hasContent = false;
        }
        else if (!isspace((unsigned char)*ptr)) {
            hasContent = true;
        }
        ptr++;
    }

    return count;
}

/* Parse a single scalar token (float or int) from *ptr without advancing it. */
static inline void r3d_rshade_read_scalar(const char* ptr, bool isFloat, float* fOut, int32_t* iOut)
{
    if (isFloat) {
        float  v = 0.0f;
        sscanf(ptr, "%f", &v);
        *fOut = v;
    }
    else {
        int32_t v = 0;
        sscanf(ptr, "%d", &v);
        *iOut = v;
    }
}

/* Parse a GLSL literal value and write it into the uniform buffer at given offset.
 * Returns true if a default value was found and parsed. */
static inline bool r3d_rshade_parse_default_value(const char** ptr,
    const char* type, uint8_t* buffer, int offset)
{
    r3d_rshade_skip_whitespace(ptr);
    if (**ptr != '=') return false;

    (*ptr)++;
    r3d_rshade_skip_whitespace(ptr);

    uint8_t* dst = buffer + offset;

    // Scalars
    if (strcmp(type, "float") == 0) {
        float v = 0.0f; sscanf(*ptr, "%f", &v);
        memcpy(dst, &v, 4);
        return true;
    }
    if (strcmp(type, "int") == 0) {
        int v = 0; sscanf(*ptr, "%d", &v);
        memcpy(dst, &v, 4);
        return true;
    }
    if (strcmp(type, "bool") == 0) {
        // Accept both true/false keywords and 1/0 literals
        int v = (strncmp(*ptr, "true", 4) == 0) ? 1
              : (strncmp(*ptr, "false", 5) == 0) ? 0
              : ((*ptr)[0] != '0');
        memcpy(dst, &v, 4);
        return true;
    }

    // Vectors and matrices
    // std140: each column occupies a 16-byte slot (vec4 stride),
    // so the byte offset of component [col][row] is: col * 16 + row * 4.
    // For vectors (cols=1) this degenerates to simple contiguous layout.

    int cols = 0, rows = 0;
    bool isFloat = true;

    if      (strcmp(type, "vec2")  == 0) { cols = 1; rows = 2; }
    else if (strcmp(type, "vec3")  == 0) { cols = 1; rows = 3; }
    else if (strcmp(type, "vec4")  == 0) { cols = 1; rows = 4; }
    else if (strcmp(type, "ivec2") == 0) { cols = 1; rows = 2; isFloat = false; }
    else if (strcmp(type, "ivec3") == 0) { cols = 1; rows = 3; isFloat = false; }
    else if (strcmp(type, "ivec4") == 0) { cols = 1; rows = 4; isFloat = false; }
    else if (strcmp(type, "mat2")  == 0) { cols = 2; rows = 2; }
    else if (strcmp(type, "mat3")  == 0) { cols = 3; rows = 3; }
    else if (strcmp(type, "mat4")  == 0) { cols = 4; rows = 4; }

    if (cols == 0) return false;

    // Advance past the constructor name to '('
    while (**ptr && **ptr != '(') (*ptr)++;
    if (**ptr != '(') return false;
    (*ptr)++;

    int argCount = r3d_rshade_count_constructor_args(*ptr);

    if (argCount == 1) {
        // Single-scalar constructor
        r3d_rshade_skip_whitespace(ptr);

        float   fScalar = 0.0f;
        int32_t iScalar = 0;
        r3d_rshade_read_scalar(*ptr, isFloat, &fScalar, &iScalar);

        for (int c = 0; c < cols; c++) {
            for (int r = 0; r < rows; r++) {
                int byteOffset = c * 16 + r * 4;

                /* For matrices, only the diagonal gets the scalar value.
                 * For vectors (cols == 1), every component gets it. */
                bool active = (cols == 1) || (r == c);

                if (isFloat) {
                    float v = active ? fScalar : 0.0f;
                    memcpy(dst + byteOffset, &v, 4);
                }
                else {
                    int32_t v = active ? iScalar : 0;
                    memcpy(dst + byteOffset, &v, 4);
                }
            }
        }

        // Skip to closing ')'
        while (**ptr && **ptr != ')') (*ptr)++;
        if (**ptr == ')') (*ptr)++;
    }
    else {
        // Full component list
        for (int c = 0; c < cols; c++) {
            for (int r = 0; r < rows; r++) {
                r3d_rshade_skip_whitespace(ptr);

                int byteOffset = c * 16 + r * 4;
                if (isFloat) {
                    float v = 0.0f; sscanf(*ptr, "%f", &v);
                    memcpy(dst + byteOffset, &v, 4);
                }
                else {
                    int32_t v = 0; sscanf(*ptr, "%d", &v);
                    memcpy(dst + byteOffset, &v, 4);
                }

                while (**ptr && **ptr != ',' && **ptr != ')') (*ptr)++;
                if (**ptr == ',') (*ptr)++;
            }
        }
    }

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

/* Parse a 'uniform' declaration and register it as a sampler or UBO entry.
 * The pointer must be positioned just after the 'uniform' keyword. */
static inline bool r3d_rshade_parse_uniform(const char** ptr,
    r3d_rshade_sampler_t* samplers, r3d_rshade_uniform_buffer_t* uniforms,
    int* samplerCount, int* uniformCount, int* currentOffset,
    int maxSamplers, int maxUniforms)
{
    char type[R3D_RSHADE_MAX_VAR_TYPE_LENGTH];
    char name[R3D_RSHADE_MAX_VAR_NAME_LENGTH];

    // Stop before '=' or ';' so default value parsing can inspect what follows
    if (!r3d_rshade_parse_identifier(ptr, type, R3D_RSHADE_MAX_VAR_TYPE_LENGTH) ||
        !r3d_rshade_parse_identifier(ptr, name, R3D_RSHADE_MAX_VAR_NAME_LENGTH)) {
        r3d_rshade_skip_to_semicolon(ptr);
        return false;
    }

    // Samplers are bound by texture unit, not stored in the UBO
    GLenum samplerTarget = r3d_rshade_get_sampler_target(type);
    if (samplerTarget != 0) {
        if (*samplerCount < maxSamplers) {
            r3d_rshade_sampler_t* s = &samplers[(*samplerCount)++];
            strncpy(s->name, name, R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1);
            s->name[R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1] = '\0';
            s->target = samplerTarget;
            s->texture = 0;
        }
        r3d_rshade_skip_to_semicolon(ptr);
        return true;
    }

    int size = r3d_rshade_get_type_size(type);
    if (size > 0 && *uniformCount < maxUniforms) {
        // Align offset per std140 before committing the slot
        *currentOffset = r3d_align_offset(*currentOffset, r3d_rshade_get_std140_alignment(size));

        r3d_rshade_uniform_t* u = &uniforms->entries[(*uniformCount)++];
        strncpy(u->name, name, R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1);
        u->name[R3D_RSHADE_MAX_VAR_NAME_LENGTH - 1] = '\0';
        strncpy(u->type, type, R3D_RSHADE_MAX_VAR_TYPE_LENGTH - 1);
        u->type[R3D_RSHADE_MAX_VAR_TYPE_LENGTH - 1] = '\0';
        u->offset = *currentOffset;
        u->size = size;
        *currentOffset += size;

        if (r3d_rshade_parse_default_value(ptr, type, uniforms->buffer, u->offset)) {
            uniforms->dirty = true; // Just for security, but the data is necessarily uploaded when the buffer is created afterwards
        }
    }

    r3d_rshade_skip_to_semicolon(ptr);
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
        const char* lineStart = ptr;
        r3d_rshade_skip_whitespace_and_comments(&ptr);

        if (!*ptr) break;

        // Check if line should be skipped
        if (r3d_rshade_should_skip_line(ptr, hasVaryings, vertexFunc, fragmentFunc)) {
            // Special handling for entry points, skip entire function body
            if (r3d_rshade_check_shader_entry(ptr, vertexFunc, fragmentFunc)) {
                r3d_rshade_skip_to_matching_brace(&ptr);
            }
            // For pragmas, skip to end of line (not semicolon)
            else if (strncmp(ptr, "#pragma", 7) == 0) {
                r3d_rshade_skip_to_end_of_line(&ptr);
            }
            // For uniforms/varyings, skip to semicolon
            else {
                r3d_rshade_skip_to_semicolon(&ptr);
            }
            continue;
        }

        // Copy user code content
        while (lineStart < ptr) *outPtr++ = *lineStart++;
        if (*ptr) *outPtr++ = *ptr++;
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

#endif // R3D_RSHADE_H
