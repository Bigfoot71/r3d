#include <r3d/r3d.h>
#include <raymath.h>

#define INSTANCE_COUNT 1000

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Instanced rendering example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set ambient light
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    // Create cube mesh and default material
    R3D_Mesh mesh = R3D_GenMeshCube(1, 1, 1);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Generate random transforms and colors for instances
    Matrix transforms[INSTANCE_COUNT];
    Color colors[INSTANCE_COUNT];

    for (int i = 0; i < INSTANCE_COUNT; i++)
    {
        Matrix translate = MatrixTranslate(
            (float)GetRandomValue(-50000, 50000) / 1000,
            (float)GetRandomValue(-50000, 50000) / 1000,
            (float)GetRandomValue(-50000, 50000) / 1000
        );
        Matrix rotate = MatrixRotateXYZ((Vector3){
            (float)GetRandomValue(-314000, 314000) / 100000,
            (float)GetRandomValue(-314000, 314000) / 100000,
            (float)GetRandomValue(-314000, 314000) / 100000
        });
        Matrix scale = MatrixScale(
            (float)GetRandomValue(100, 2000) / 1000,
            (float)GetRandomValue(100, 2000) / 1000,
            (float)GetRandomValue(100, 2000) / 1000
        );

        transforms[i] = MatrixMultiply(MatrixMultiply(scale, rotate), translate);
        colors[i] = ColorFromHSV((float)GetRandomValue(0, 360000) / 1000, 1.0f, 1.0f);
    }

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
                R3D_DrawMeshInstancedEx(&mesh, &material, transforms, colors, INSTANCE_COUNT);
            R3D_End();

            DrawFPS(10, 10);
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(&mesh);
    R3D_UnloadMaterial(&material);
    R3D_Close();

    CloseWindow();

    return 0;
}
