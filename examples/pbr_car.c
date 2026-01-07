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
    R3D_ENVIRONMENT_SET(background.color, DARKGRAY);
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    // Post-processing
    R3D_ENVIRONMENT_SET(ssr.enabled, true);
    R3D_ENVIRONMENT_SET(ssao.enabled, true);
    R3D_ENVIRONMENT_SET(ssao.radius, 1.0f);
    R3D_ENVIRONMENT_SET(bloom.levels, 0.5f);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.025f);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);

    // Load model
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "pbr/car.glb");

    // Ground mesh
    R3D_Mesh ground = R3D_GenMeshPlane(10.0f, 10.0f, 1, 1);
    R3D_Material groundMat = R3D_GetDefaultMaterial();
    groundMat.albedo.color = DARKGRAY;
    groundMat.orm.roughness = 0.0f;
    groundMat.orm.metalness = 0.5f;

    // Load skybox and ambient map (disabled by default)
    R3D_Cubemap skybox = R3D_LoadCubemap(RESOURCES_PATH "sky/skybox3.png", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_AmbientMap ambient = R3D_GenAmbientMap(skybox, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    bool showSkybox = false;

    // Setup directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1, -1, -1});
    R3D_SetShadowDepthBias(light, 0.003f);
    R3D_EnableShadow(light);
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
            if (showSkybox) {
                R3D_ENVIRONMENT_SET(background.sky, skybox);
                R3D_ENVIRONMENT_SET(ambient.map, ambient);
            }
            else {
                R3D_ENVIRONMENT_SET(background.sky, (R3D_Cubemap){0});
                R3D_ENVIRONMENT_SET(ambient.map, (R3D_AmbientMap){0});
            }
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw scene
            R3D_Begin(camera);
                R3D_DrawMesh(ground, groundMat, Vector3Zero(), 1.0f);
                R3D_DrawModel(model, Vector3Zero(), 1.0f);
            R3D_End();

            DrawText("Model made by MaximePages", 10, GetScreenHeight()-26, 16, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(model, true);
    R3D_UnloadAmbientMap(ambient);
    R3D_UnloadCubemap(skybox);
    R3D_Close();

    CloseWindow();

    return 0;
}
