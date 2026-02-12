#include "r3d/r3d_instance.h"
#include <r3d/r3d.h>
#include <raymath.h>

#define INSTANCE_COUNT 1000

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Instanced rendering example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Set ambient light
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    // Create cube mesh and default material
    R3D_Mesh mesh = R3D_GenMeshCube(1, 1, 1);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Generate random transforms and colors for instances
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(INSTANCE_COUNT, R3D_INSTANCE_POSITION | R3D_INSTANCE_ROTATION | R3D_INSTANCE_SCALE | R3D_INSTANCE_COLOR);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    Quaternion* rotations = R3D_MapInstances(instances, R3D_INSTANCE_ROTATION);
    Vector3* scales = R3D_MapInstances(instances, R3D_INSTANCE_SCALE);
    Color* colors = R3D_MapInstances(instances, R3D_INSTANCE_COLOR);

    for (int i = 0; i < INSTANCE_COUNT; i++)
    {
        positions[i] = (Vector3) {
            (float)GetRandomValue(-50000, 50000) / 1000,
            (float)GetRandomValue(-50000, 50000) / 1000,
            (float)GetRandomValue(-50000, 50000) / 1000
        };
        rotations[i] = QuaternionFromEuler(
            (float)GetRandomValue(-314000, 314000) / 100000,
            (float)GetRandomValue(-314000, 314000) / 100000,
            (float)GetRandomValue(-314000, 314000) / 100000
        );
        scales[i] = (Vector3) {
            (float)GetRandomValue(100, 2000) / 1000,
            (float)GetRandomValue(100, 2000) / 1000,
            (float)GetRandomValue(100, 2000) / 1000
        };
        colors[i] = ColorFromHSV(
            (float)GetRandomValue(0, 360000) / 1000, 1.0f, 1.0f
        );
    }

    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION | R3D_INSTANCE_ROTATION | R3D_INSTANCE_SCALE | R3D_INSTANCE_COLOR);

    // Setup directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){0, -1, 0});
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 2, 2},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Capture mouse
    DisableCursor();

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FREE);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawMeshInstanced(mesh, material, instances, INSTANCE_COUNT);
            R3D_End();

            DrawFPS(10, 10);
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadInstanceBuffer(instances);
    R3D_UnloadMaterial(material);
    R3D_UnloadMesh(mesh);
    R3D_Close();

    CloseWindow();

    return 0;
}
