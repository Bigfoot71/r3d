#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - PBR musket example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);

    // Tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.75f);

    // Set texture filter for mipmaps
    R3D_SetTextureFilter(TEXTURE_FILTER_ANISOTROPIC_4X);

    // Load model
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "pbr/musket.glb");
    Matrix modelMatrix = MatrixIdentity();
    float modelScale = 1.0f;

    // Load skybox
    R3D_Skybox skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox2.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.sky, skybox);

    // Setup directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){0, -1, -1});
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 0, 0.5f},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        // Update model scale with mouse wheel
        modelScale = Clamp(modelScale + GetMouseWheelMove() * 0.1f, 0.25f, 2.5f);

        // Rotate model with left mouse button
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float pitch = (GetMouseDelta().y * 0.005f) / modelScale;
            float yaw   = (GetMouseDelta().x * 0.005f) / modelScale;
            Matrix rotate = MatrixRotateXYZ((Vector3){pitch, yaw, 0.0f});
            modelMatrix = MatrixMultiply(modelMatrix, rotate);
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw model
            R3D_Begin(camera);
                Matrix scale = MatrixScale(modelScale, modelScale, modelScale);
                Matrix transform = MatrixMultiply(modelMatrix, scale);
                R3D_DrawModelPro(model, transform);
            R3D_End();

            DrawText("Model made by TommyLingL", 10, GetScreenHeight()-26, 16, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(model, true);
    R3D_UnloadSkybox(skybox);
    R3D_Close();

    CloseWindow();

    return 0;
}
