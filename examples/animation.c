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
    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);

    // Enable post-processing effects
    R3D_ENVIRONMENT_SET(ssao.enabled, true);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.03f);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_AGX);

    // Set background and ambient colors
    R3D_ENVIRONMENT_SET(background.color, (Color){12, 10, 15, 255});
    R3D_ENVIRONMENT_SET(ambient.color, (Color){12, 10, 15, 255});

    // Create ground plane
    R3D_Mesh plane = R3D_GenMeshPlane(32, 32, 1, 1);
    R3D_Material planeMat = R3D_GetDefaultMaterial();
    planeMat.orm.roughness = 0.5f;
    planeMat.orm.metalness = 0.5f;
    planeMat.uvScale = (Vector2){8.0f, 8.0f};

    Image checked = GenImageChecked(512, 512, 32, 32, (Color){20,20,20,255}, WHITE);
    planeMat.albedo.texture = LoadTextureFromImage(checked);
    UnloadImage(checked);

    GenTextureMipmaps(&planeMat.albedo.texture);
    SetTextureFilter(planeMat.albedo.texture, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(planeMat.albedo.texture, TEXTURE_WRAP_REPEAT);

    // Load animated model
    R3D_Model dancer = R3D_LoadModel(RESOURCES_PATH "dancer.glb");

    // Create instance matrices
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(4, R3D_INSTANCE_POSITION);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    for (int z = 0; z < 2; z++) {
        for (int x = 0; x < 2; x++) {
            positions[z*2 + x] = (Vector3) {(float)x - 0.5f, 0, (float)z - 0.5f};
        }
    }
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION);

    // Load animations
    R3D_AnimationLib dancerAnims = R3D_LoadAnimationLib(RESOURCES_PATH "dancer.glb");
    R3D_AnimationPlayer dancerPlayer = R3D_LoadAnimationPlayer(dancer.skeleton, dancerAnims);
    dancerPlayer.states[0].weight = 1.0f;
    dancerPlayer.states[0].loop = true;

    // Setup lights with shadows
    R3D_Light lights[2];
    lights[0] = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(lights[0], (Vector3){-10.0f, 25.0f, 0.0f});
    R3D_EnableShadow(lights[0]);
    R3D_SetLightEnergy(lights[0], 1.25f);
    R3D_SetLightActive(lights[0], true);

    lights[1] = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(lights[1], (Vector3){10.0f, 25.0f, 0.0f});
    R3D_EnableShadow(lights[1]);
    R3D_SetLightEnergy(lights[1], 1.25f);
    R3D_SetLightActive(lights[1], true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 2.0f, 3.5f},
        .target = {0, 1.0f, 1.5f},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Capture mouse
    DisableCursor();

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        UpdateCamera(&camera, CAMERA_FREE);
        R3D_UpdateAnimationPlayer(&dancerPlayer, delta);

        // Animate lights
        R3D_SetLightColor(lights[0], ColorFromHSV(90.0f * GetTime() + 90.0f, 1.0f, 1.0f));
        R3D_SetLightColor(lights[1], ColorFromHSV(90.0f * GetTime() - 90.0f, 1.0f, 1.0f));

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawMesh(plane, planeMat, Vector3Zero(), 1.0f);
                R3D_DrawAnimatedModel(dancer, dancerPlayer, (Vector3){0, 0, 1.5f}, 1.0f);
                R3D_DrawAnimatedModelInstanced(dancer, dancerPlayer, instances, 4);
            R3D_End();

            DrawText("Model made by zhuoyi0904", 10, GetScreenHeight() - 26, 16, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadAnimationPlayer(dancerPlayer);
    R3D_UnloadAnimationLib(dancerAnims);
    R3D_UnloadModel(dancer, true);
    R3D_UnloadMaterial(planeMat);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
