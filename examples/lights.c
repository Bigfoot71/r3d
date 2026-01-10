#include <r3d/r3d.h>
#include <raymath.h>
#include <stdlib.h>

#define NUM_LIGHTS 128
#define GRID_SIZE 100

static inline float randf(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Many lights example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set ambient light
    R3D_ENVIRONMENT_SET(background.color, BLACK);
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});

    // Create plane and cube meshes
    R3D_Mesh plane = R3D_GenMeshPlane(100, 100, 1, 1);
    R3D_Mesh cube = R3D_GenMeshCube(0.5f, 0.5f, 0.5f);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Allocate transforms for all spheres
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(GRID_SIZE * GRID_SIZE, R3D_INSTANCE_POSITION);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    for (int x = -50; x < 50; x++) {
        for (int z = -50; z < 50; z++) {
            positions[(z+50)*GRID_SIZE + (x+50)] = (Vector3) {x + 0.5f, 0, z + 0.5f};
        }
    }
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION);

    // Create lights
    R3D_Light lights[NUM_LIGHTS];
    for (int i = 0; i < NUM_LIGHTS; i++) {
        lights[i] = R3D_CreateLight(R3D_LIGHT_OMNI);
        R3D_SetLightPosition(lights[i], (Vector3){randf(-50.0f, 50.0f), randf(1.0f, 5.0f), randf(-50.0f, 50.0f)});
        R3D_SetLightColor(lights[i], ColorFromHSV(randf(0.0f, 360.0f), 1.0f, 1.0f));
        R3D_SetLightRange(lights[i], randf(8.0f, 16.0f));
        R3D_SetLightActive(lights[i], true);
    }

    // Setup camera
    Camera3D camera = {
        .position = {0, 10, 10},
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
            R3D_DrawMesh(plane, material, (Vector3) {0, -0.25f, 0}, 1.0f);
            R3D_DrawMeshInstanced(cube, material, instances, GRID_SIZE*GRID_SIZE);
        R3D_End();

        // Optionally show lights shapes
        if (IsKeyDown(KEY_F)) {
            BeginMode3D(camera);
            for (int i = 0; i < NUM_LIGHTS; i++) {
                R3D_DrawLightShape(lights[i]);
            }
            EndMode3D();
        }

        DrawFPS(10, 10);
        DrawText("Press 'F' to show the lights", 10, GetScreenHeight()-34, 24, BLACK);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadInstanceBuffer(instances);
    R3D_UnloadMesh(cube);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
