#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Skybox example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Create sphere mesh
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);

    // Create grid of materials (metalness and roughness)
    R3D_Material materials[7 * 7];
    for (int x = 0; x < 7; x++) {
        for (int y = 0; y < 7; y++) {
            int i = y * 7 + x;
            materials[i] = R3D_GetDefaultMaterial();
            materials[i].orm.metalness = (float)x / 7;
            materials[i].orm.roughness = (float)y / 7;
            materials[i].albedo.color = ColorFromHSV(((float)x / 7) * 360, 1, 1);
        }
    }

    // Load and enable skybox
    R3D_Skybox skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox1.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.sky, skybox);

    // Setup camera
    Camera3D camera = {
        .position = {0, 0, 5},
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

        // Draw sphere grid
        R3D_Begin(camera);
            for (int x = 0; x < 7; x++) {
                for (int y = 0; y < 7; y++) {
                    R3D_DrawMesh(&sphere, &materials[y * 7 + x], MatrixTranslate((float)x - 3, (float)y - 3, 0.0f));
                }
            }
        R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(&sphere);
    R3D_UnloadSkybox(skybox);
    R3D_Close();

    CloseWindow();

    return 0;
}
