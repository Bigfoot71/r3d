#include "r3d/r3d_environment.h"
#include "r3d/r3d_lighting.h"
#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - PBR car example");
    SetTargetFPS(60);

    // Initialize R3D with flags
    R3D_Flags flags = R3D_FLAG_TRANSPARENT_SORTING | R3D_FLAG_FXAA;
    R3D_Init(GetScreenWidth(), GetScreenHeight(), flags);

    // Set environment
    R3D_ENVIRONMENT_SET(background.color, BLACK);
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    // Post-processing
    R3D_ENVIRONMENT_SET(ssr.enabled, true);
    R3D_ENVIRONMENT_SET(ssao.enabled, true);
    R3D_ENVIRONMENT_SET(ssao.radius, 2.0f);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.1f);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);

    // Load model
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "pbr/car.glb");

    // Ground mesh
    R3D_Mesh ground = R3D_GenMeshPlane(10.0f, 10.0f, 1, 1);
    R3D_Material groundMat = R3D_GetDefaultMaterial();
    groundMat.albedo.color = (Color){31, 31, 31, 255};
    groundMat.orm.roughness = 0.0f;
    groundMat.orm.metalness = 0.5f;

    // Load skybox (disabled by default)
    R3D_Skybox skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox3.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    bool showSkybox = false;

    // Setup directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1, -1, -1});
    R3D_SetShadowDepthBias(light, 0.003f);
    R3D_EnableShadow(light, 4096);
    R3D_SetLightActive(light, true);
    R3D_SetLightEnergy(light, 2.0f);
    R3D_SetLightRange(light, 10);

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

        // Toggle SSAO
        if (IsKeyPressed(KEY_O)) {
            R3D_ENVIRONMENT_SET(ssao.enabled, !R3D_ENVIRONMENT_GET(ssao.enabled));
        }

        // Toggle skybox
        if (IsKeyPressed(KEY_T)) {
            showSkybox = !showSkybox;
            if (showSkybox) R3D_ENVIRONMENT_SET(background.sky, skybox);
            else R3D_ENVIRONMENT_SET(background.sky, (R3D_Skybox){0});
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw scene
            R3D_Begin(camera);
                R3D_DrawMesh(&ground, &groundMat, MatrixIdentity());
                R3D_DrawModel(&model, (Vector3){0,0,0}, 1.0f);
            R3D_End();

            DrawText("Model made by MaximePages", 10, GetScreenHeight()-26, 16, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(&model, true);
    R3D_UnloadSkybox(skybox);
    R3D_Close();

    CloseWindow();

    return 0;
}
