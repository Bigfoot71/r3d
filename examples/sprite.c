#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

static void GetTexCoordScaleOffset(Vector2* uvScale, Vector2* uvOffset, int xFrameCount, int yFrameCount, float currentFrame);

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Sprite example");
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

    // Create sprite mesh and material
    R3D_Mesh meshSprite = R3D_GenMeshQuad(1.0f, 1.0f, 1, 1, (Vector3){0,0,1});
    meshSprite.shadowCastMode = R3D_SHADOW_CAST_ON_DOUBLE_SIDED;

    R3D_Material matSprite = R3D_GetDefaultMaterial();
    matSprite.billboardMode = R3D_BILLBOARD_Y_AXIS;
    matSprite.albedo.texture = LoadTexture(RESOURCES_PATH "spritesheet.png");

    // Setup spotlight
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0,10,10}, (Vector3){0});
    R3D_SetLightRange(light, 64.0f);
    R3D_EnableShadow(light, 1024);
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 2, 5},
        .target = {0, 0.5f, 0},
        .up = {0, 1, 0},
        .fovy = 45
    };

    // Bird data
    Vector3 birdPos = {0, 0.5f, 0};
    float birdDirX = 1.0f;

    // Main loop
    while (!WindowShouldClose())
    {
        // Update bird position
        Vector3 birdPrev = birdPos;
        birdPos.x = 2.0f * sinf((float)GetTime());
        birdPos.y = 1.0f + cosf((float)GetTime() * 4.0f) * 0.5f;
        birdDirX = (birdPos.x - birdPrev.x >= 0.0f) ? 1.0f : -1.0f;

        // Update sprite UVs
        // We multiply by the sign of the X direction to invert the uvScale.x
        float currentFrame = 10.0f * GetTime();
        GetTexCoordScaleOffset(&matSprite.uvScale, &matSprite.uvOffset, 4 * birdDirX, 1, currentFrame);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            // Draw scene
            R3D_Begin(camera);
                R3D_DrawMesh(meshGround, matGround, (Vector3) {0, -0.5f, 0}, 1.0f);
                R3D_DrawMesh(meshSprite, matSprite, (Vector3) {birdPos.x, birdPos.y, 0}, 1.0f);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMaterial(matSprite);
    R3D_UnloadMesh(meshSprite);
    R3D_UnloadMesh(meshGround);
    R3D_Close();

    CloseWindow();

    return 0;
}

void GetTexCoordScaleOffset(Vector2* uvScale, Vector2* uvOffset, int xFrameCount, int yFrameCount, float currentFrame)
{
    uvScale->x = 1.0f / xFrameCount;
    uvScale->y = 1.0f / yFrameCount;

    int frameIndex = (int)(currentFrame + 0.5f) % (xFrameCount * yFrameCount);
    int frameX = frameIndex % xFrameCount;
    int frameY = frameIndex / xFrameCount;

    uvOffset->x = frameX * uvScale->x;
    uvOffset->y = frameY * uvScale->y;
}
