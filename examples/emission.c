#include "r3d/r3d_environment.h"
#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

static void ToggleLight(R3D_Light light, R3D_Skybox sky);

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Emission example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Configure post-processing (Tonemap + Bloom)
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.5f);
    R3D_ENVIRONMENT_SET(tonemap.white, 4.0f);

    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);
    R3D_ENVIRONMENT_SET(bloom.softThreshold, 0.2f);
    R3D_ENVIRONMENT_SET(bloom.threshold, 0.6f);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.2f);
    R3D_ENVIRONMENT_SET(bloom.levels, 0.5f);

    R3D_ENVIRONMENT_SET(ssil.enabled, true);
    R3D_ENVIRONMENT_SET(ssil.energy, 4.0f);

    // Load and set skybox
    R3D_Skybox skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox3.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.sky, skybox);
    R3D_ENVIRONMENT_SET(ambient.reflect, 0.6f);

    // Load model
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "emission.glb");

    // Create ground plane
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Setup spotlight
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0, 10, 5}, (Vector3){0});
    R3D_SetLightOuterCutOff(light, 45.0f);
    R3D_SetLightInnerCutOff(light, 22.5f);
    R3D_EnableShadow(light, 4096);
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = {
        .position = {-1.0f, 1.75f, 1.75f},
        .target = {0, 0.5f, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    float rotModel = 0.0f;

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        // Input
        if (IsKeyPressed(KEY_SPACE)) {
            ToggleLight(light, skybox);
        }
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            camera.position.y = Clamp(camera.position.y + 0.01f * GetMouseDelta().y, 0.25f, 2.5f);
            rotModel += 0.01f * GetMouseDelta().x;
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Render scene
            R3D_Begin(camera);
                R3D_DrawMesh(&plane, &material, MatrixIdentity());
                R3D_DrawModelEx(&model, (Vector3){0}, (Vector3){0, 1, 0}, rotModel, (Vector3){1, 1, 1});
            R3D_End();

            // UI
            DrawText("Press SPACE to toggle the light", 10, 10, 20, LIME);
            DrawText("Model by har15204405", 10, GetScreenHeight() - 26, 16, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(&model, true);
    R3D_UnloadMesh(&plane);
    R3D_Close();

    CloseWindow();

    return 0;
}

void ToggleLight(R3D_Light light, R3D_Skybox sky)
{
    if (R3D_IsLightActive(light)) {
        R3D_SetLightActive(light, false);
        R3D_ENVIRONMENT_SET(background.sky, (R3D_Skybox) {0});
        R3D_ENVIRONMENT_SET(background.color, BLACK);
        R3D_ENVIRONMENT_SET(ambient.color, BLACK);
    } else {
        R3D_SetLightActive(light, true);
        R3D_ENVIRONMENT_SET(background.sky, sky);
    }
}
