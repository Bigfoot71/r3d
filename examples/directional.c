#include <r3d/r3d.h>
#include <raymath.h>
#include <stdlib.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Directional light example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Create meshes and material
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.35f, 24, 16);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Create transforms for instanced spheres
    int count = 100;
    Matrix* transforms = RL_MALLOC(count * count * sizeof(Matrix));
    if (!transforms) exit(-1);

    for (int x = -50; x < 50; x++) {
        for (int z = -50; z < 50; z++) {
            transforms[(z+50)*count + (x+50)] = MatrixTranslate((float)x*2, 0, (float)z*2);
        }
    }

    // Setup environment
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});

    // Create directional light with shadows
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){0, -1, -1});
    R3D_SetLightActive(light, true);
    R3D_SetLightRange(light, 16.0f);
    R3D_EnableShadow(light, 4096);
    R3D_SetShadowDepthBias(light, 0.01f);
    R3D_SetShadowSoftness(light, 2.0f);

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
                R3D_DrawMesh(&plane, &material, MatrixTranslate(0, -0.5f, 0));
                R3D_DrawMeshInstanced(&sphere, &material, transforms, count*count);
            R3D_End();

            DrawFPS(10, 10);

        EndDrawing();
    }

    // Cleanup
    RL_FREE(transforms);
    R3D_UnloadMesh(&plane);
    R3D_UnloadMesh(&sphere);
    R3D_UnloadMaterial(&material);
    R3D_Close();

    CloseWindow();

    return 0;
}
