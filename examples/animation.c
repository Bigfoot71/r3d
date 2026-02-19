#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Animation example");
    SetTargetFPS(60);

    // Initialize R3D with FXAA
    R3D_Init(GetScreenWidth(), GetScreenHeight());
    R3D_SetAntiAliasingMode(R3D_ANTI_ALIASING_MODE_FXAA);

    // Setup environment sky
    R3D_Cubemap cubemap = R3D_LoadCubemap(RESOURCES_PATH "panorama/indoor.hdr", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.skyBlur, 0.3f);
    R3D_ENVIRONMENT_SET(background.energy, 0.6f);
    R3D_ENVIRONMENT_SET(background.sky, cubemap);

    // Setup environment ambient
    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(cubemap, R3D_AMBIENT_ILLUMINATION);
    R3D_ENVIRONMENT_SET(ambient.map, ambientMap);
    R3D_ENVIRONMENT_SET(ambient.energy, 0.25f);

    // Setup tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.75f);

    // Generate a ground plane and load the animated model
    R3D_Mesh plane = R3D_GenMeshPlane(10, 10, 1, 1);
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "models/CesiumMan.glb");

    // Load animations
    R3D_AnimationLib modelAnims = R3D_LoadAnimationLib(RESOURCES_PATH "models/CesiumMan.glb");
    R3D_AnimationPlayer modelPlayer = R3D_LoadAnimationPlayer(model.skeleton, modelAnims);

    // Setup animation playing
    R3D_SetAnimationWeight(&modelPlayer, 0, 1.0f);
    R3D_SetAnimationLoop(&modelPlayer, 0, true);
    R3D_PlayAnimation(&modelPlayer, 0);

    // Create model instances
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(4, R3D_INSTANCE_POSITION);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    for (int z = 0; z < 2; z++) {
        for (int x = 0; x < 2; x++) {
            positions[z*2 + x] = (Vector3) {(float)x - 0.5f, 0, (float)z - 0.5f};
        }
    }
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION);

    // Setup lights with shadows
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1.0f, -1.0f, -1.0f});
    R3D_SetLightActive(light, true);
    R3D_SetLightRange(light, 10.0f);
    R3D_EnableShadow(light);

    // Setup camera
    Camera3D camera = {
        .position = {0, 1.5f, 3.0f},
        .target = {0, 0.75f, 0.0f},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        UpdateCamera(&camera, CAMERA_ORBITAL);
        R3D_UpdateAnimationPlayer(&modelPlayer, delta);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            R3D_Begin(camera);
                R3D_DrawMesh(plane, R3D_MATERIAL_BASE, Vector3Zero(), 1.0f);
                R3D_DrawAnimatedModel(model, modelPlayer, Vector3Zero(), 1.25f);
                R3D_DrawAnimatedModelInstanced(model, modelPlayer, instances, 4);
            R3D_End();
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadAnimationPlayer(modelPlayer);
    R3D_UnloadAnimationLib(modelAnims);
    R3D_UnloadModel(model, true);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
