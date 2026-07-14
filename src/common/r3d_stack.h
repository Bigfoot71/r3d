/* r3d_stack.h -- Temporary stack allocator with push/pop scopes.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_STACK_H
#define R3D_STACK_H

#include <raylib.h>
#include <stddef.h>
#include <stdint.h>

#include "./r3d_helper.h"

// ========================================
// CONFIG
// ========================================

/* Max nested push()/pop() depth (size of the internal marks[] array) */
#ifndef R3D_STACK_MAX_DEPTH
#   define R3D_STACK_MAX_DEPTH 16
#endif

/* Default alignment for r3d_stack_alloc() */
#define R3D_STACK_DEFAULT_ALIGN (sizeof(void*))

// ========================================
// HELPER MACROS
// ========================================

/* Push/alloc/pop in one block; pop() runs automatically on block exit.
 * Skips the body entirely if push() fails (max depth or OOM).
 *
 *   R3D_STACK_SCOPE(&stack, 1024) {
 *       void* tmp = r3d_stack_alloc(&stack, 64);
 *       ...
 *   } // pop() happens here
 *
 * A plain `break` inside the body exits early without popping.
 * Use R3D_STACK_SCOPE_EXIT for an early safe exit
 */
#define R3D_STACK_SCOPE(stackPtr, reserve)                      \
    for (bool R3D_CONCAT(r3d_stack_scope_ok_, __LINE__) =       \
             r3d_stack_push((stackPtr), (reserve));             \
         R3D_CONCAT(r3d_stack_scope_ok_, __LINE__);             \
         r3d_stack_pop(*(stackPtr)),                            \
         R3D_CONCAT(r3d_stack_scope_ok_, __LINE__) = false)

/* Early, safe exit from a R3D_STACK_SCOPE: pops then breaks out.
 *
 * CAUTION: this is a raw `break`, so it only exits the innermost
 * enclosing loop/switch. Only call it directly inside the
 * R3D_STACK_SCOPE body.
 */
#define R3D_STACK_SCOPE_EXIT(stack) do {    \
    r3d_stack_pop(stack);                   \
    break;                                  \
} while (0)

// ========================================
// STACK STRUCT
// ========================================

/* Single allocation: [ r3d_stack_t | memory[capacity] ].
 * push() saves the cursor, alloc() bumps it forward, pop() restores
 * it; a pure LIFO scratch allocator, nothing is freed individually.
 */
typedef struct {
    size_t capacity;                    // Buffer size in bytes
    size_t cursor;                      // Current bump position
    size_t marks[R3D_STACK_MAX_DEPTH];  // Saved cursor per push() level
    uint32_t depth;                     // Current nesting depth
    uint8_t* memory;                    // Buffer (right after the header)
} r3d_stack_t;

// ========================================
// STACK FUNCTIONS
// ========================================

/* Create a stack with the given initial capacity. NULL on failure or capacity == 0 */
r3d_stack_t* r3d_stack_create(size_t capacity);

/* Free the stack and all its memory */
void r3d_stack_destroy(r3d_stack_t* stack);

/* Open a scope, saving the current cursor. 'reserve' (may be 0) pre-grows
 * the buffer so at least that many bytes are available without further
 * reallocation during the scope. *stackPtr is updated if it grows.
 * Returns false on failure (max depth reached or OOM) */
bool r3d_stack_push(r3d_stack_t** stackPtr, size_t reserve);

/* Close the current scope: rewinds the cursor to the matching push() */
void r3d_stack_pop(r3d_stack_t* stack);

/* Bump-allocate 'size' bytes, aligned to R3D_STACK_DEFAULT_ALIGN.
 * *stackPtr is updated if it grows. NULL on failure */
void* r3d_stack_alloc(r3d_stack_t** stackPtr, size_t size);

/* Same as r3d_stack_alloc(), with an explicit power-of-two alignment */
void* r3d_stack_alloc_aligned(r3d_stack_t** stackPtr, size_t size, size_t align);

/* Rewind everything at once: cursor to 0, all scopes closed (O(1)) */
void r3d_stack_reset(r3d_stack_t* stack);

/* Bytes currently in use */
static inline size_t r3d_stack_used(const r3d_stack_t* stack)
{
    return stack->cursor;
}

/* Bytes free before the next reallocation */
static inline size_t r3d_stack_remaining(const r3d_stack_t* stack)
{
    return stack->capacity - stack->cursor;
}

#endif // R3D_STACK_H
