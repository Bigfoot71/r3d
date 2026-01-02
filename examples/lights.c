#include <r3d/r3d.h>
#include <raymath.h>

#define GRID_SIZE 100
#define LIGHT_GRID 10

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Many lights example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set ambient light
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});

    // Create plane and sphere meshes
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.35f, 16, 16);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Allocate transforms for all spheres
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(GRID_SIZE * GRID_SIZE, R3D_INSTANCE_POSITION);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    for (int x = -50; x < 50; x++) {
        for (int z = -50; z < 50; z++) {
            positions[(z+50)*GRID_SIZE + (x+50)] = (Vector3) {x, 0, z};
        }
    }
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION);

    // Create lights
    R3D_Light lights[LIGHT_GRID * LIGHT_GRID];
    for (int x = -5; x < 5; x++) {
        for (int z = -5; z < 5; z++) {
            int index = (z+5)*LIGHT_GRID + (x+5);
            lights[index] = R3D_CreateLight(R3D_LIGHT_OMNI);
            R3D_SetLightPosition(lights[index], (Vector3){(float)x*10, 10, (float)z*10});
            R3D_SetLightColor(lights[index], ColorFromHSV((float)index/100*360, 1.0f, 1.0f));
            R3D_SetLightRange(lights[index], 20.0f);
            R3D_SetLightActive(lights[index], true);
        }
    }

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

        // Draw scene
        R3D_Begin(camera);
            R3D_DrawMesh(plane, material, (Vector3) {0, -0.5f, 0}, 1.0f);
            R3D_DrawMeshInstanced(sphere, material, instances, GRID_SIZE*GRID_SIZE);
        R3D_End();

        // Optionally show lights shapes
        if (IsKeyDown(KEY_SPACE)) {
            BeginMode3D(camera);
            for (int i = 0; i < LIGHT_GRID*LIGHT_GRID; i++)
                R3D_DrawLightShape(lights[i]);
            EndMode3D();
        }

        DrawFPS(10, 10);
        DrawText("Press SPACE to show the lights", 10, GetScreenHeight()-34, 24, BLACK);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadInstanceBuffer(instances);
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
