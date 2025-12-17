#include "./r3d_light.h"

#include <raylib.h>
#include <raymath.h>
#include <stddef.h>
#include <string.h>
#include <float.h>
#include <rlgl.h>
#include <glad.h>

/* === Internal functions === */

static r3d_shadow_map_t r3d_light_create_shadow_map_dir(int resolution)
{
    r3d_shadow_map_t shadowMap = { 0 };

    glGenFramebuffers(1, &shadowMap.id);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.id);

    glGenTextures(1, &shadowMap.depth);
    glBindTexture(GL_TEXTURE_2D, shadowMap.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "Framebuffer creation error for the directional shadow map");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    shadowMap.texelSize = 1.0f / resolution;
    shadowMap.resolution = resolution;

    return shadowMap;
}

static r3d_shadow_map_t r3d_light_create_shadow_map_spot(int resolution)
{
    r3d_shadow_map_t shadowMap = { 0 };

    shadowMap.resolution = resolution;

    glGenFramebuffers(1, &shadowMap.id);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.id);

    glGenTextures(1, &shadowMap.depth);
    glBindTexture(GL_TEXTURE_2D, shadowMap.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "Framebuffer creation error for the Shadow Map Spot");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    shadowMap.texelSize = 1.0f / resolution;
    shadowMap.resolution = resolution;

    return shadowMap;
}

static r3d_shadow_map_t r3d_light_create_shadow_map_omni(int resolution)
{
    r3d_shadow_map_t shadowMap = { 0 };

    glGenFramebuffers(1, &shadowMap.id);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.id);

    glGenTextures(1, &shadowMap.depth);
    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMap.depth);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT16,
            resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL
        );
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, shadowMap.depth, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "Framebuffer creation error for the omni shadow map");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    shadowMap.texelSize = 1.0f / resolution;
    shadowMap.resolution = resolution;

    return shadowMap;
}

/* === Public functions === */

void r3d_light_init(r3d_light_t* light, R3D_LightType type)
{
    memset(light, 0, sizeof(r3d_light_t));

    /* --- Set base light parameters --- */

    light->color = (Vector3){ 1, 1, 1 };
    light->position = (Vector3){ 0 };
    light->direction = (Vector3){ 0, 0, -1 };

    light->specular = 0.5f;
    light->energy = 1.0f;
    light->range = 50.0f;

    light->attenuation = 1.0f;
    light->innerCutOff = cosf(22.5f * DEG2RAD);
    light->outerCutOff = cosf(45.0f * DEG2RAD);

    light->type = type;
    light->enabled = false;

    /* --- Set common shadow config --- */

    light->shadow.updateConf.mode = R3D_SHADOW_UPDATE_INTERVAL;
    light->shadow.updateConf.frequencySec = 0.016f;
    light->shadow.updateConf.timerSec = 0.0f;
    light->shadow.updateConf.shouldUpdate = true;

    /* --- Set specific shadow config --- */

    switch (type) {
    case R3D_LIGHT_DIR:
        light->shadow.depthBias = 0.0002f;
        light->shadow.slopeBias = 0.002f;
        break;
    case R3D_LIGHT_SPOT:
        light->shadow.depthBias = 0.00002f;
        light->shadow.slopeBias = 0.0002f;
        break;
    case R3D_LIGHT_OMNI:
        light->shadow.depthBias = 0.01f;
        light->shadow.slopeBias = 0.02f;
        break;
    }
}

void r3d_light_create_shadow_map(r3d_light_t* light, int resolution)
{
    switch (light->type) {
    case R3D_LIGHT_DIR:
        light->shadow.map = r3d_light_create_shadow_map_dir(resolution);
        break;
    case R3D_LIGHT_SPOT:
        light->shadow.map = r3d_light_create_shadow_map_spot(resolution);
        break;
    case R3D_LIGHT_OMNI:
        light->shadow.map = r3d_light_create_shadow_map_omni(resolution);
        break;
    }

    light->shadow.softness = 4.0f * light->shadow.map.texelSize;
    light->shadow.updateConf.shouldUpdate = true;
}

void r3d_light_destroy_shadow_map(r3d_light_t* light)
{
    if (light->shadow.map.id != 0) {
        rlUnloadTexture(light->shadow.map.depth);
        rlUnloadFramebuffer(light->shadow.map.id);
    }
}

void r3d_light_process_shadow_update(r3d_light_t* light)
{
    switch (light->shadow.updateConf.mode) {
    case R3D_SHADOW_UPDATE_MANUAL:
        break;
    case R3D_SHADOW_UPDATE_INTERVAL:
        if (!light->shadow.updateConf.shouldUpdate) {
            light->shadow.updateConf.timerSec += GetFrameTime();
            if (light->shadow.updateConf.timerSec >= light->shadow.updateConf.frequencySec) {
                light->shadow.updateConf.shouldUpdate = true;
                light->shadow.updateConf.timerSec = 0.0f;
            }
        }
        break;
    case R3D_SHADOW_UPDATE_CONTINUOUS:
        light->shadow.updateConf.shouldUpdate = true;
        break;
    }
}

void r3d_light_indicate_shadow_update(r3d_light_t* light)
{
    switch (light->shadow.updateConf.mode) {
    case R3D_SHADOW_UPDATE_MANUAL:
        light->shadow.updateConf.shouldUpdate = false;
        break;
    case R3D_SHADOW_UPDATE_INTERVAL:
        light->shadow.updateConf.shouldUpdate = false;
        light->shadow.updateConf.timerSec = 0.0f;
        break;
    case R3D_SHADOW_UPDATE_CONTINUOUS:
        break;
    }
}

BoundingBox r3d_light_get_bounding_box(const r3d_light_t* light)
{
    BoundingBox aabb = {
        {-FLT_MAX, -FLT_MAX, -FLT_MAX},
        {+FLT_MAX, +FLT_MAX, +FLT_MAX},
    };

    switch (light->type)
    {
    case R3D_LIGHT_DIR:
        break;

    case R3D_LIGHT_OMNI:
        aabb.min = Vector3AddValue(light->position, -light->range);
        aabb.max = Vector3AddValue(light->position, +light->range);
        break;

    case R3D_LIGHT_SPOT:
        {
            // REVIEW: This method seems a bit extreme to me
            //         A simpler method allowing a slight margin would be acceptable...

            const float h = light->range;
            const float cosTheta = light->outerCutOff;

            // Handle extreme wide angles (> ~84°) to avoid numerical instability
            if (cosTheta < 0.1f) {
                // Treat as omnidirectional light with range limit
                const Vector3* p = &light->position;
                for (int i = 0; i < 3; ++i) {
                    ((float*)&aabb.min)[i] = ((float*)p)[i] - h;
                    ((float*)&aabb.max)[i] = ((float*)p)[i] + h;
                }
                break;
            }

            // Stable radius calculation: tan(acos(cosTheta)) = sqrt(1 - cosTheta²) / cosTheta
            const float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);
            const float r = h * sinTheta / cosTheta;

            // Additional safety: clamp radius to reasonable bounds
            const float maxR = h * 5.0f;
            const float clampedR = fminf(r, maxR);

            // Cone tip at light position
            Vector3 tip = light->position;

            // Center of cone base
            Vector3 base = {
                light->position.x + light->direction.x * h,
                light->position.y + light->direction.y * h,
                light->position.z + light->direction.z * h
            };

            // Initialize bounding box with the cone tip
            aabb.min = aabb.max = tip;

            // Extend to include base center
            for (int i = 0; i < 3; ++i) {
                float baseCoord = ((float*)&base)[i];
                ((float*)&aabb.min)[i] = fminf(((float*)&aabb.min)[i], baseCoord);
                ((float*)&aabb.max)[i] = fmaxf(((float*)&aabb.max)[i], baseCoord);
            }

            // Calculate orthogonal vectors to light direction
            Vector3 dir = light->direction;
            Vector3 up, right;

            // Choose reference vector that's not parallel to direction
            if (fabsf(dir.x) < 0.9f) {
                up = (Vector3){1.0f, 0.0f, 0.0f};
            } else {
                up = (Vector3){0.0f, 1.0f, 0.0f};
            }

            // Calculate right vector (cross product)
            right = (Vector3){
                dir.y * up.z - dir.z * up.y,
                dir.z * up.x - dir.x * up.z,
                dir.x * up.y - dir.y * up.x
            };

            // Normalize right vector
            float rightLen = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
            if (rightLen > 1e-6f) {
                right.x /= rightLen;
                right.y /= rightLen;
                right.z /= rightLen;
            } else {
                // Direction was parallel to chosen up vector, try another
                up = (Vector3){0.0f, 0.0f, 1.0f};
                right = (Vector3){
                    dir.y * up.z - dir.z * up.y,
                    dir.z * up.x - dir.x * up.z,
                    dir.x * up.y - dir.y * up.x
                };
                rightLen = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
                if (rightLen > 1e-6f) {
                    right.x /= rightLen;
                    right.y /= rightLen;
                    right.z /= rightLen;
                }
            }

            // Calculate up vector (cross product of dir and right)
            up = (Vector3){
                dir.y * right.z - dir.z * right.y,
                dir.z * right.x - dir.x * right.z,
                dir.x * right.y - dir.y * right.x
            };

            // Sample points on circular base to get accurate bounding box
            const int numSamples = 12;
            for (int i = 0; i < numSamples; ++i) {
                float angle = (float)(i * 2.0 * PI / numSamples);
                float cosAngle = cosf(angle);
                float sinAngle = sinf(angle);

                // Point on the circular base
                Vector3 point = {
                    base.x + clampedR * (cosAngle * right.x + sinAngle * up.x),
                    base.y + clampedR * (cosAngle * right.y + sinAngle * up.y),
                    base.z + clampedR * (cosAngle * right.z + sinAngle * up.z)
                };

                // Extend bounding box to include this point
                for (int j = 0; j < 3; ++j) {
                    float coord = ((float*)&point)[j];
                    ((float*)&aabb.min)[j] = fminf(((float*)&aabb.min)[j], coord);
                    ((float*)&aabb.max)[j] = fmaxf(((float*)&aabb.max)[j], coord);
                }
            }
        }
        break;
    }

    return aabb;
}

void r3d_light_get_matrix_vp_dir(r3d_light_t* light, BoundingBox sceneBounds, Matrix* view, Matrix* proj)
{
    // Calculating the center of the scene
    Vector3 sceneCenter = {
        (sceneBounds.min.x + sceneBounds.max.x) * 0.5f,
        (sceneBounds.min.y + sceneBounds.max.y) * 0.5f,
        (sceneBounds.min.z + sceneBounds.max.z) * 0.5f
    };

    // Calculating the half-extents of the scene with a safety margin
    const float SCENE_MARGIN = 1.1f; // 10% margin
    Vector3 sceneExtents = {
        (sceneBounds.max.x - sceneBounds.min.x) * 0.5f * SCENE_MARGIN,
        (sceneBounds.max.y - sceneBounds.min.y) * 0.5f * SCENE_MARGIN,
        (sceneBounds.max.z - sceneBounds.min.z) * 0.5f * SCENE_MARGIN
    };

    // Normalizing the light direction
    Vector3 lightDir = Vector3Normalize(light->direction);

    // Calculating the light position (placed at a distance from the center of the scene)
    float maxSceneExtent = fmaxf(sceneExtents.x, fmaxf(sceneExtents.y, sceneExtents.z));
    float lightDistance = maxSceneExtent * 2.0f;
    Vector3 lightPos = Vector3Add(sceneCenter, Vector3Scale(Vector3Negate(lightDir), lightDistance));

    // Keeps the calculated position of the light
    light->position = lightPos;

    // Calculating the view matrix with a stable up vector
    Vector3 upVector;
    if (fabsf(lightDir.y) > 0.99f) {
        // If the direction is nearly vertical, use Z as the "up" vector
        upVector = (Vector3){ 0.0f, 0.0f, 1.0f };
    }
    else {
        upVector = (Vector3){ 0.0f, 1.0f, 0.0f };
    }
    *view = MatrixLookAt(lightPos, sceneCenter, upVector);

    // Calculating the bounding volume of the scene in light space
    Matrix viewMatrix = *view;
    Vector3 corners[8] = {
        {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.min.z},
        {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.min.z},
        {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.min.z},
        {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.min.z},
        {sceneBounds.min.x, sceneBounds.min.y, sceneBounds.max.z},
        {sceneBounds.max.x, sceneBounds.min.y, sceneBounds.max.z},
        {sceneBounds.min.x, sceneBounds.max.y, sceneBounds.max.z},
        {sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z}
    };

    float minX = INFINITY, maxX = -INFINITY;
    float minY = INFINITY, maxY = -INFINITY;
    float minZ = INFINITY, maxZ = -INFINITY;

    for (int i = 0; i < 8; i++) {
        Vector3 transformed = Vector3Transform(corners[i], viewMatrix);
        minX = fminf(minX, transformed.x);
        maxX = fmaxf(maxX, transformed.x);
        minY = fminf(minY, transformed.y);
        maxY = fmaxf(maxY, transformed.y);
        minZ = fminf(minZ, transformed.z);
        maxZ = fmaxf(maxZ, transformed.z);
    }

    // Creating the orthographic projection matrix
    // WARNING: In camera space, objects in front of the camera have negative Z values.
    // Here, maxZ corresponds to the closest plane (less negative) and minZ to the farthest plane.
    // To obtain positive distances for the projection, we reverse the signs:
    // near = -maxZ and far = -minZ (which guarantees near < far).

    light->near = -maxZ;    // Save near plane (can be used in shaders)
    light->far = -minZ;     // Save far plane (can be used in shaders)

    *proj = MatrixOrtho(minX, maxX, minY, maxY, light->near, light->far);
}

Matrix r3d_light_get_matrix_view_spot(r3d_light_t* light)
{
    return MatrixLookAt(light->position,
        Vector3Add(light->position, light->direction), (Vector3) { 0, 1, 0 }
    );
}

Matrix r3d_light_get_matrix_proj_spot(r3d_light_t* light)
{
    light->near = 0.05f;        // Save near plane (can be used in shaders)
    light->far = light->range;  // Save far plane (can be used in shaders)
    return MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);
}

Matrix r3d_light_get_matrix_view_omni(r3d_light_t* light, int face)
{
    static const Vector3 dirs[6] = {
        {  1.0,  0.0,  0.0 }, // +X
        { -1.0,  0.0,  0.0 }, // -X
        {  0.0,  1.0,  0.0 }, // +Y
        {  0.0, -1.0,  0.0 }, // -Y
        {  0.0,  0.0,  1.0 }, // +Z
        {  0.0,  0.0, -1.0 }  // -Z
    };

    static const Vector3 ups[6] = {
        {  0.0, -1.0,  0.0 }, // +X
        {  0.0, -1.0,  0.0 }, // -X
        {  0.0,  0.0,  1.0 }, // +Y
        {  0.0,  0.0, -1.0 }, // -Y
        {  0.0, -1.0,  0.0 }, // +Z
        {  0.0, -1.0,  0.0 }  // -Z
    };

    return MatrixLookAt(
        light->position, Vector3Add(light->position, dirs[face]), ups[face]
    );
}

Matrix r3d_light_get_matrix_proj_omni(r3d_light_t* light)
{
    light->near = 0.05f;        // Save near plane (can be used in shaders)
    light->far = light->range;  // Save far plane (can be used in shaders)
    return MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);
}
