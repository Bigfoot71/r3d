#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - PBR example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());
    R3D_SetAntiAliasing(R3D_ANTI_ALIASING_FXAA);

    // Setup environment sky
    R3D_Cubemap cubemap = R3D_LoadCubemap(RESOURCES_PATH "panorama/indoor.hdr", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.skyBlur, 0.775f);
    R3D_ENVIRONMENT_SET(background.sky, cubemap);

    // Setup environment ambient
    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(cubemap, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    R3D_ENVIRONMENT_SET(ambient.map, ambientMap);

    // Setup bloom
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.02f);

    // Setup tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.5f);
    R3D_ENVIRONMENT_SET(tonemap.white, 4.0f);

    // Load model
    R3D_SetTextureFilter(TEXTURE_FILTER_ANISOTROPIC_4X);
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "models/DamagedHelmet.glb");
    Matrix modelMatrix = MatrixIdentity();
    float modelScale = 1.0f;

    // Setup camera
    Camera3D camera = {
        .position = {0, 0, 2.5f},
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
            R3D_Begin(camera);
                Matrix scale = MatrixScale(modelScale, modelScale, modelScale);
                Matrix transform = MatrixMultiply(modelMatrix, scale);
                R3D_DrawModelPro(model, transform);
            R3D_End();
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(model, true);
    R3D_UnloadAmbientMap(ambientMap);
    R3D_UnloadCubemap(cubemap);
    R3D_Close();

    CloseWindow();

    return 0;
}
