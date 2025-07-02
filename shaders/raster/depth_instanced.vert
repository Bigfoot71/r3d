/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#version 330 core

/* === Defines === */

#define BILLBOARD_FRONT 0
#define BILLBOARD_Y_AXIS 1

/* === Attributes === */

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 3) in vec4 aColor;

/* === Instanced attributes === */

layout(location = 10) in mat4 aInstanceModel;

/* === Uniforms === */

uniform mat4 uMatInvView;       ///< Only for billboard modes
uniform mat4 uMatModel;
uniform mat4 uMatVP;

uniform float uAlpha;

uniform lowp int uBillboardMode;

/* === Varyings === */

out vec2 vTexCoord;
out float vAlpha;

/* === Helper functions === */

void BillboardFront(inout mat4 model)
{
    // Extract the original scales of the model
    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));

    // Copy the inverted view vectors for the X, Y, and Z axes
    // while applying the original scales
    model[0] = vec4(normalize(uMatInvView[0].xyz) * scaleX, 0.0);
    model[1] = vec4(normalize(uMatInvView[1].xyz) * scaleY, 0.0);
    model[2] = vec4(normalize(uMatInvView[2].xyz) * scaleZ, 0.0);
}

void BillboardY(inout mat4 model)
{
    // Extract the model position
    vec3 position = vec3(model[3]);
    
    // Extract the original scales of the model
    float scaleX = length(vec3(model[0]));
    float scaleY = length(vec3(model[1]));
    float scaleZ = length(vec3(model[2]));
    
    // Preserve the original Y-axis of the model (vertical direction)
    vec3 upVector = normalize(vec3(model[1]));
    
    // Direction from the camera to the object
    vec3 lookDirection = normalize(position - vec3(uMatInvView[3]));
    
    // Compute the right vector using the cross product
    vec3 rightVector = normalize(cross(upVector, lookDirection));
    
    // Recalculate the front vector to ensure orthogonality
    vec3 frontVector = normalize(cross(rightVector, upVector));
    
    // Construct the new model matrix while preserving the scales
    model[0] = vec4(rightVector * scaleX, 0.0);
    model[1] = vec4(upVector * scaleY, 0.0);
    model[2] = vec4(frontVector * scaleZ, 0.0);
}

/* === Main function === */

void main()
{
    mat4 matModel = uMatModel * transpose(aInstanceModel);

    if (uBillboardMode == BILLBOARD_FRONT) BillboardFront(matModel);
    else if (uBillboardMode == BILLBOARD_Y_AXIS) BillboardY(matModel);

    vTexCoord = aTexCoord;
    vAlpha = uAlpha * aColor.a;

    gl_Position = uMatVP * (matModel * vec4(aPosition, 1.0));
}
