#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Billboards example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set background/ambient color
    R3D_ENVIRONMENT_SET(background.color, (Color){102, 191, 255, 255});
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 19, 25, 255});

    // Create ground mesh and material
    R3D_Mesh meshGround = R3D_GenMeshPlane(200, 200, 1, 1);
    R3D_Material matGround = R3D_GetDefaultMaterial();
    matGround.albedo.color = GREEN;

    // Create billboard mesh and material
    R3D_Mesh meshBillboard = R3D_GenMeshQuad(1.0f, 1.0f, 1, 1, (Vector3){0.0f, 0.0f, 1.0f});
    meshBillboard.shadowCastMode = R3D_SHADOW_CAST_ON_DOUBLE_SIDED;

    R3D_Material matBillboard = R3D_GetDefaultMaterial();
    matBillboard.billboardMode = R3D_BILLBOARD_Y_AXIS;
    matBillboard.albedo.texture = LoadTexture(RESOURCES_PATH "tree.png");

    // Create transforms for instanced billboards
    Matrix transformsBillboards[64];
    for (int i = 0; i < 64; i++)
    {
        float scaleFactor = GetRandomValue(25, 50) / 10.0f;
        Matrix scale = MatrixScale(scaleFactor, scaleFactor, 1.0f);
        Matrix translate = MatrixTranslate(
            (float)GetRandomValue(-100, 100),
            scaleFactor * 0.5f,
            (float)GetRandomValue(-100, 100)
        );
        transformsBillboards[i] = MatrixMultiply(scale, translate);
    }

    // Setup directional light with shadows
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1, -1, -1});
    R3D_SetShadowDepthBias(light, 0.01f);
    R3D_EnableShadow(light, 4096);
    R3D_SetLightActive(light, true);
    R3D_SetLightRange(light, 32.0f);

    // Setup camera
    Camera3D camera = {
        .position = {0, 5, 0},
        .target = {0, 5, -1},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Capture mouse
    DisableCursor();

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FREE);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawMesh(&meshGround, &matGround, MatrixIdentity());
                R3D_DrawMeshInstanced(&meshBillboard, &matBillboard, transformsBillboards, 64);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMaterial(&matBillboard);
    R3D_UnloadMesh(&meshBillboard);
    R3D_UnloadMesh(&meshGround);
    R3D_Close();
    CloseWindow();

    return 0;
}
