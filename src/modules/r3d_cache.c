/* r3d_draw.c -- Internal R3D cache module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_cache.h"

#include <r3d/r3d_environment.h>
#include <stdalign.h>
#include <raymath.h>
#include <stddef.h>
#include <glad.h>

#include "../details/r3d_math.h"

// ========================================
// MODULE STATE
// ========================================

struct r3d_cache R3D_MOD_CACHE;

// ========================================
// INTERNAL UNIFORM BUFFER STRUCTS
// ========================================

typedef struct {
    alignas(16) Vector3 viewPosition;
    alignas(16) Matrix view;
    alignas(16) Matrix invView;
    alignas(16) Matrix proj;
    alignas(16) Matrix invProj;
    alignas(16) Matrix viewProj;
    alignas(4) float aspect;
    alignas(4) float near;
    alignas(4) float far;
} uniform_view_state_t;

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_cache_init(R3D_Flags flags)
{
    R3D_MOD_CACHE.matCubeViews[0] = MatrixLookAt((Vector3) {0}, (Vector3) { 1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D_MOD_CACHE.matCubeViews[1] = MatrixLookAt((Vector3) {0}, (Vector3) {-1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D_MOD_CACHE.matCubeViews[2] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  1.0f,  0.0f}, (Vector3) {0.0f,  0.0f,  1.0f});
    R3D_MOD_CACHE.matCubeViews[3] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f, -1.0f,  0.0f}, (Vector3) {0.0f,  0.0f, -1.0f});
    R3D_MOD_CACHE.matCubeViews[4] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  0.0f,  1.0f}, (Vector3) {0.0f, -1.0f,  0.0f});
    R3D_MOD_CACHE.matCubeViews[5] = MatrixLookAt((Vector3) {0}, (Vector3) { 0.0f,  0.0f, -1.0f}, (Vector3) {0.0f, -1.0f,  0.0f});

    R3D_MOD_CACHE.environment = R3D_ENVIRONMENT_BASE;

    R3D_MOD_CACHE.textureFilter = TEXTURE_FILTER_TRILINEAR;
    R3D_MOD_CACHE.layers = R3D_LAYER_ALL;
    R3D_MOD_CACHE.state = flags;

    glGenBuffers(R3D_CACHE_UNIFORM_COUNT, R3D_MOD_CACHE.uniformBuffers);

    glBindBuffer(GL_UNIFORM_BUFFER, R3D_MOD_CACHE.uniformBuffers[R3D_CACHE_UNIFORM_VIEW_STATE]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(uniform_view_state_t), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    return true;
}

void r3d_cache_quit(void)
{
    glDeleteBuffers(R3D_CACHE_UNIFORM_COUNT, R3D_MOD_CACHE.uniformBuffers);
}

void r3d_cache_update_view_state(Camera3D camera, double aspect, double near, double far)
{
    Matrix view = MatrixLookAt(camera.position, camera.target, camera.up);
    Matrix proj = R3D_MATRIX_IDENTITY;

    if (camera.projection == CAMERA_PERSPECTIVE) {
        proj = MatrixPerspective(camera.fovy * DEG2RAD, aspect, near, far);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double top = camera.fovy / 2.0, right = top * aspect;
        proj = MatrixOrtho(-right, right, -top, top, near, far);
    }

    Matrix viewProj = r3d_matrix_multiply(&view, &proj);

    R3D_MOD_CACHE.viewState.frustum = r3d_frustum_create(viewProj);
    R3D_MOD_CACHE.viewState.viewPosition = camera.position;

    R3D_MOD_CACHE.viewState.view = view;
    R3D_MOD_CACHE.viewState.proj = proj;
    R3D_MOD_CACHE.viewState.invView = MatrixInvert(view);
    R3D_MOD_CACHE.viewState.invProj = MatrixInvert(proj);
    R3D_MOD_CACHE.viewState.viewProj = viewProj;

    R3D_MOD_CACHE.viewState.aspect = aspect;
    R3D_MOD_CACHE.viewState.near = near;
    R3D_MOD_CACHE.viewState.far = far;
}

void r3d_cache_bind_view_state(int slot)
{
    GLuint ubo = R3D_MOD_CACHE.uniformBuffers[R3D_CACHE_UNIFORM_VIEW_STATE];

    uniform_view_state_t uViewState = {0};
    uViewState.viewPosition = R3D_MOD_CACHE.viewState.viewPosition;
    uViewState.view = r3d_matrix_transpose(&R3D_MOD_CACHE.viewState.view);
    uViewState.invView = r3d_matrix_transpose(&R3D_MOD_CACHE.viewState.invView);
    uViewState.proj = r3d_matrix_transpose(&R3D_MOD_CACHE.viewState.proj);
    uViewState.invProj = r3d_matrix_transpose(&R3D_MOD_CACHE.viewState.invProj);
    uViewState.viewProj = r3d_matrix_transpose(&R3D_MOD_CACHE.viewState.viewProj);
    uViewState.aspect = R3D_MOD_CACHE.viewState.aspect;
    uViewState.near = R3D_MOD_CACHE.viewState.near;
    uViewState.far = R3D_MOD_CACHE.viewState.far;

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uniform_view_state_t), &uViewState);
    glBindBufferBase(GL_UNIFORM_BUFFER, slot, ubo);
}
