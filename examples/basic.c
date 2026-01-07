#include "r3d/r3d_material.h"
#include <r3d/r3d.h>
#include <raymath.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Basic example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Create meshes
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Material material = R3D_GetDefaultMaterial();
    material.transparencyMode = R3D_TRANSPARENCY_PREPASS;

    // Setup environment
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});

    // Create light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0, 10, 5}, (Vector3){0});
    R3D_EnableShadow(light);
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 2, 2},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawMesh(plane, material, (Vector3) {0, -0.5f, 0}, 1.0f);
                R3D_DrawMesh(sphere, material, Vector3Zero(), 1.0f);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
