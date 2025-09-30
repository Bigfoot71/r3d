/* billboard.glsl -- Contains everything you need to manage billboards
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Constants === */

#define BILLBOARD_NONE   0
#define BILLBOARD_FRONT  1
#define BILLBOARD_Y_AXIS 2

/* === Functions === */

void BillboardFront(inout mat4 model, mat4 invView)
{
    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));

    model[0] = vec4(invView[0].xyz * scaleX, 0.0);
    model[1] = vec4(invView[1].xyz * scaleY, 0.0);
    model[2] = vec4(invView[2].xyz * scaleZ, 0.0);
}

void BillboardFront(inout mat4 model, inout mat3 normal, mat4 invView)
{
    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));

    model[0] = vec4(invView[0].xyz * scaleX, 0.0);
    model[1] = vec4(invView[1].xyz * scaleY, 0.0);
    model[2] = vec4(invView[2].xyz * scaleZ, 0.0);

    normal[0] = invView[0].xyz;
    normal[1] = invView[1].xyz;
    normal[2] = invView[2].xyz;
}

void BillboardYAxis(inout mat4 model, mat4 invView)
{
    vec3 position = vec3(model[3]);

    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));

    vec3 upVector = normalize(vec3(model[1]));
    vec3 lookDirection = normalize(vec3(invView[3]) - position);
    vec3 rightVector = normalize(cross(upVector, lookDirection));
    vec3 frontVector = normalize(cross(rightVector, upVector));

    model[0] = vec4(rightVector * scaleX, 0.0);
    model[1] = vec4(upVector * scaleY, 0.0);
    model[2] = vec4(frontVector * scaleZ, 0.0);
}

void BillboardYAxis(inout mat4 model, inout mat3 normal, mat4 invView)
{
    vec3 position = vec3(model[3]);

    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));

    vec3 upVector = normalize(vec3(model[1]));
    vec3 lookDirection = normalize(vec3(invView[3]) - position);
    vec3 rightVector = normalize(cross(upVector, lookDirection));
    vec3 frontVector = normalize(cross(rightVector, upVector));

    model[0] = vec4(rightVector * scaleX, 0.0);
    model[1] = vec4(upVector * scaleY, 0.0);
    model[2] = vec4(frontVector * scaleZ, 0.0);

    normal[0] = rightVector;
    normal[1] = upVector;
    normal[2] = frontVector;
}
