/* r3d_stack.c -- Temporary bump/stack allocator with push/pop scopes.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_stack.h"
#include <raylib.h>
#include <string.h>
#include <assert.h>

// ========================================
// INTERNAL HELPERS
// ========================================

static inline size_t stack_total_size(size_t capacity)
{
    return sizeof(r3d_stack_t) + capacity;
}

static inline void stack_set_pointer(r3d_stack_t* stack)
{
    stack->memory = (uint8_t*)stack + sizeof(r3d_stack_t);
}

/* Reallocate to at least 'minCapacity' bytes (geometric x2 growth).
 * Only bytes below 'cursor' are copied; the rest is unused scratch. */
static r3d_stack_t* stack_grow(r3d_stack_t* stack, size_t minCapacity)
{
    size_t newCap = (stack->capacity > 0) ? stack->capacity : 64;

    while (newCap < minCapacity) {
        size_t doubled = newCap * 2;
        if (doubled <= newCap) { // Overflow
            newCap = minCapacity;
            break;
        }
        newCap = doubled;
    }

    uint8_t* newMem = MemAlloc(stack_total_size(newCap));
    if (!newMem) return NULL;

    r3d_stack_t* newStack = (r3d_stack_t*)newMem;
    *newStack = *stack;
    newStack->capacity = newCap;

    stack_set_pointer(newStack);

    if (stack->cursor > 0) {
        memcpy(newStack->memory, stack->memory, stack->cursor);
    }

    MemFree(stack);
    return newStack;
}

// ========================================
// STACK FUNCTIONS
// ========================================

r3d_stack_t* r3d_stack_create(size_t capacity)
{
    if (capacity == 0) {
        return NULL;
    }

    uint8_t* mem = MemAlloc(stack_total_size(capacity));
    if (!mem) return NULL;

    r3d_stack_t* stack = (r3d_stack_t*)mem;
    stack->capacity = capacity;
    stack->cursor = 0;
    stack->depth = 0;

    stack_set_pointer(stack);

    return stack;
}

void r3d_stack_destroy(r3d_stack_t* stack)
{
    MemFree(stack);
}

bool r3d_stack_push(r3d_stack_t** stackPtr, size_t reserve)
{
    r3d_stack_t* stack = *stackPtr;

    assert(stack->depth < R3D_STACK_MAX_DEPTH && "r3d_stack: max push() depth exceeded");
    if (stack->depth >= R3D_STACK_MAX_DEPTH) {
        return false;
    }

    if (reserve > 0 && stack->cursor + reserve > stack->capacity) {
        stack = stack_grow(stack, stack->cursor + reserve);
        if (!stack) return false;
        *stackPtr = stack;
    }

    stack->marks[stack->depth++] = stack->cursor;

    return true;
}

void r3d_stack_pop(r3d_stack_t* stack)
{
    assert(stack->depth > 0 && "r3d_stack: pop() without matching push()");
    if (stack->depth == 0) return;

    stack->cursor = stack->marks[--stack->depth];
}

void* r3d_stack_alloc(r3d_stack_t** stackPtr, size_t size)
{
    return r3d_stack_alloc_aligned(stackPtr, size, R3D_STACK_DEFAULT_ALIGN);
}

void* r3d_stack_alloc_aligned(r3d_stack_t** stackPtr, size_t size, size_t align)
{
    if (size == 0) return NULL;
    if (align == 0) align = 1;

    assert((align & (align - 1)) == 0 && "r3d_stack: align must be a power of two");

    r3d_stack_t* stack = *stackPtr;

    // Worst-case space needed to satisfy alignment before knowing
    // the final base address (it may change if we have to grow)
    size_t worst = size + (align - 1);

    if (stack->cursor + worst > stack->capacity) {
        stack = stack_grow(stack, stack->cursor + worst);
        if (!stack) return NULL;
        *stackPtr = stack;
    }

    uintptr_t base = (uintptr_t)stack->memory + stack->cursor;
    uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t padding = (size_t)(aligned - base);

    stack->cursor += padding + size;

    return (void*)aligned;
}

void r3d_stack_reset(r3d_stack_t* stack)
{
    stack->cursor = 0;
    stack->depth = 0;
}
