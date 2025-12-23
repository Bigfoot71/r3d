#include <r3d/r3d.h>
#include <raymath.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Resize example");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Create sphere mesh and materials
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Material materials[5];
    for (int i = 0; i < 5; i++)
    {
        materials[i] = R3D_GetDefaultMaterial();
        materials[i].albedo.color = ColorFromHSV((float)i / 5 * 330, 1.0f, 1.0f);
    }

    // Setup directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){0, 0, -1});
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

        // Toggle aspect keep
        if (IsKeyPressed(KEY_R)) {
            if (R3D_HasState(R3D_FLAG_ASPECT_KEEP)) R3D_ClearState(R3D_FLAG_ASPECT_KEEP);
            else R3D_SetState(R3D_FLAG_ASPECT_KEEP);
        }

        // Toggle linear filtering
        if (IsKeyPressed(KEY_F)) {
            if (R3D_HasState(R3D_FLAG_BLIT_LINEAR)) R3D_ClearState(R3D_FLAG_BLIT_LINEAR);
            else R3D_SetState(R3D_FLAG_BLIT_LINEAR);
        }

        BeginDrawing();
            ClearBackground(BLACK);

            // Draw spheres
            R3D_Begin(camera);
                for (int i = 0; i < 5; i++) {
                    R3D_DrawMesh(&sphere, &materials[i], MatrixTranslate((float)i - 2, 0, 0));
                }
            R3D_End();

            // Draw info
            bool keep = R3D_HasState(R3D_FLAG_ASPECT_KEEP);
            bool linear = R3D_HasState(R3D_FLAG_BLIT_LINEAR);
            DrawText(TextFormat("Resize mode: %s", keep ? "KEEP" : "EXPAND"), 10, 10, 20, RAYWHITE);
            DrawText(TextFormat("Filter mode: %s", linear ? "LINEAR" : "NEAREST"), 10, 40, 20, RAYWHITE);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(&sphere);
    R3D_Close();
    CloseWindow();

    return 0;
}
