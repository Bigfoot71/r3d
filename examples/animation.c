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

    // Initialize R3D with FXAA and no frustum culling
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set background and ambient colors
    R3D_ENVIRONMENT_SET(background.color, RAYWHITE);
    R3D_ENVIRONMENT_SET(ambient.color, GRAY);

    // Generate a ground plane and load the animated model
    R3D_Mesh plane = R3D_GenMeshPlane(10, 10, 1, 1);
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "models/CesiumMan.glb");

    // Load animations
    R3D_AnimationLib modelAnims = R3D_LoadAnimationLib(RESOURCES_PATH "models/CesiumMan.glb");
    R3D_AnimationPlayer modelPlayer = R3D_LoadAnimationPlayer(model.skeleton, modelAnims);
    modelPlayer.states[0].weight = 1.0f;
    modelPlayer.states[0].loop = true;

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
        .position = {0, 2.0f, 3.5f},
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
                R3D_DrawAnimatedModel(model, modelPlayer, (Vector3){0, 0, 0.0f}, 1.0f);
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
