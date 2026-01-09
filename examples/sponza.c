#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Sponza example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Post-processing setup
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(ssao.enabled, true);

    // Background and ambient
    R3D_ENVIRONMENT_SET(background.color, SKYBLUE);
    R3D_ENVIRONMENT_SET(ambient.color, GRAY);

    // Load Sponza model
    R3D_SetTextureFilter(TEXTURE_FILTER_ANISOTROPIC_8X);
    R3D_Model sponza = R3D_LoadModel(RESOURCES_PATH "sponza.glb");

    // Load skybox (disabled by default)
    R3D_Cubemap skybox = R3D_LoadCubemap(RESOURCES_PATH "sky/skybox3.png", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_AmbientMap ambient = R3D_GenAmbientMap(skybox, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    bool skyEnabled = false;

    // Setup lights
    R3D_Light lights[2];
    for (int i = 0; i < 2; i++) {
        lights[i] = R3D_CreateLight(R3D_LIGHT_OMNI);
        R3D_SetLightPosition(lights[i], (Vector3){ i ? -10.0f : 10.0f, 20.0f, 0.0f });
        R3D_SetLightActive(lights[i], true);
        R3D_SetLightEnergy(lights[i], 4.0f);
        R3D_SetShadowUpdateMode(lights[i], R3D_SHADOW_UPDATE_MANUAL);
        R3D_EnableShadow(lights[i]);
    }

    // Setup camera
    Camera3D camera = {
        .position = {8.0f, 1.0f, 0.5f},
        .target = {0.0f, 2.0f, -2.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 60.0f
    };

    // Capture mouse
    DisableCursor();

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FREE);

        // Toggle skybox
        if (IsKeyPressed(KEY_ZERO)) {
            if (skyEnabled) {
                R3D_ENVIRONMENT_SET(background.sky, (R3D_Cubemap){0});
                R3D_ENVIRONMENT_SET(ambient.map, (R3D_AmbientMap){0});
            }
            else {
                R3D_ENVIRONMENT_SET(background.sky, skybox);
                R3D_ENVIRONMENT_SET(ambient.map, ambient);
            }
            skyEnabled = !skyEnabled;
        }

        // Toggle SSAO
        if (IsKeyPressed(KEY_ONE)) {
            R3D_ENVIRONMENT_SET(ssao.enabled, !R3D_ENVIRONMENT_GET(ssao.enabled));
        }

        // Toggle SSIL
        if (IsKeyPressed(KEY_TWO)) {
            R3D_ENVIRONMENT_SET(ssil.enabled, !R3D_ENVIRONMENT_GET(ssil.enabled));
        }

        // Toggle SSR
        if (IsKeyPressed(KEY_THREE)) {
            R3D_ENVIRONMENT_SET(ssr.enabled, !R3D_ENVIRONMENT_GET(ssr.enabled));
        }

        // Toggle fog
        if (IsKeyPressed(KEY_FOUR)) {
            R3D_ENVIRONMENT_SET(fog.mode, R3D_ENVIRONMENT_GET(fog.mode) == R3D_FOG_DISABLED ? R3D_FOG_EXP : R3D_FOG_DISABLED);
        }

        // Toggle FXAA
        if (IsKeyPressed(KEY_FIVE)) {
            R3D_SetAntiAliasing(!R3D_GetAntiAliasing());
        }

        // Cycle tonemapping
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            R3D_Tonemap mode = R3D_ENVIRONMENT_GET(tonemap.mode);
            R3D_ENVIRONMENT_SET(tonemap.mode, (mode + R3D_TONEMAP_COUNT - 1) % R3D_TONEMAP_COUNT);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            R3D_Tonemap mode = R3D_ENVIRONMENT_GET(tonemap.mode);
            R3D_ENVIRONMENT_SET(tonemap.mode, (mode + 1) % R3D_TONEMAP_COUNT);
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw Sponza model
            R3D_Begin(camera);
                R3D_DrawModel(sponza, Vector3Zero(), 1.0f);
            R3D_End();

            // Draw lights
            BeginMode3D(camera);
                DrawSphere(R3D_GetLightPosition(lights[0]), 0.5f, WHITE);
                DrawSphere(R3D_GetLightPosition(lights[1]), 0.5f, WHITE);
            EndMode3D();

            // Display tonemapping
            R3D_Tonemap tonemap = R3D_ENVIRONMENT_GET(tonemap.mode);
            const char* tonemapText = "";
            switch (tonemap)
            {
                case R3D_TONEMAP_LINEAR:    tonemapText = "< TONEMAP LINEAR >"; break;
                case R3D_TONEMAP_REINHARD:  tonemapText = "< TONEMAP REINHARD >"; break;
                case R3D_TONEMAP_FILMIC:    tonemapText = "< TONEMAP FILMIC >"; break;
                case R3D_TONEMAP_ACES:      tonemapText = "< TONEMAP ACES >"; break;
                case R3D_TONEMAP_AGX:       tonemapText = "< TONEMAP AGX >"; break;
                default: break;
            }
            DrawText(tonemapText, GetScreenWidth() - MeasureText(tonemapText, 20) - 10, 10, 20, LIME);

            DrawFPS(10, 10);
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(sponza, true);
    R3D_UnloadAmbientMap(ambient);
    R3D_UnloadCubemap(skybox);
    R3D_Close();

    CloseWindow();

    return 0;
}
