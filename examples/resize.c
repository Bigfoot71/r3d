#include <r3d/r3d.h>
#include <raymath.h>

static const char* GetAspectModeName(R3D_AspectMode mode);
static const char* GetUpscaleModeName(R3D_UpscaleMode mode);

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

    // Current blit state
    R3D_AspectMode aspect = R3D_ASPECT_EXPAND;
    R3D_UpscaleMode upscale = R3D_UPSCALE_NEAREST;

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Toggle aspect keep
        if (IsKeyPressed(KEY_R)) {
            aspect = (aspect + 1) % 2;
            R3D_SetAspectMode(aspect);
        }

        // Toggle linear filtering
        if (IsKeyPressed(KEY_F)) {
            upscale = (upscale + 1) % 4;
            R3D_SetUpscaleMode(upscale);
        }

        BeginDrawing();
            ClearBackground(BLACK);

            // Draw spheres
            R3D_Begin(camera);
                for (int i = 0; i < 5; i++) {
                    R3D_DrawMesh(sphere, materials[i], (Vector3) {(float)i - 2, 0, 0}, 1.0f);
                }
            R3D_End();

            // Draw info
            DrawText(TextFormat("Resize mode: %s", GetAspectModeName(aspect)), 10, 10, 20, RAYWHITE);
            DrawText(TextFormat("Filter mode: %s", GetUpscaleModeName(upscale)), 10, 40, 20, RAYWHITE);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(sphere);
    R3D_Close();

    CloseWindow();

    return 0;
}

const char* GetAspectModeName(R3D_AspectMode mode)
{
    switch (mode) {
    case R3D_ASPECT_EXPAND: return "EXPAND";
    case R3D_ASPECT_KEEP: return "KEEP";
    }
    return "UNKNOWN";
}

const char* GetUpscaleModeName(R3D_UpscaleMode mode)
{
    switch (mode) {
    case R3D_UPSCALE_NEAREST: return "NEAREST";
    case R3D_UPSCALE_LINEAR: return "LINEAR";
    case R3D_UPSCALE_BICUBIC: return "BICUBIC";
    case R3D_UPSCALE_LANCZOS: return "LANCZOS";
    }
    return "UNKNOWN";
}
