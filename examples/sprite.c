#include "./common.h"

/* === Resources === */

static Camera3D camera = { 0 };

static R3D_Mesh meshGround = { 0 };
static R3D_Material matGround = { 0 };

static R3D_Mesh meshSprite = { 0 };
static R3D_Material matSprite = { 0 };

/* === Bird Data === */

float birdDirX = 1.0f;
Vector3 birdPos = { 0.0f, 0.5f, 0.0f };

/* === Sprite Helper === */

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

/* === Examples === */

const char* Init(void)
{
    /* --- Initialize R3D with screen resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Setup the ground mesh / material --- */

    meshGround = R3D_GenMeshPlane(200, 200, 1, 1);
    matGround = R3D_GetDefaultMaterial();
    matGround.albedo.color = GREEN;

    /* --- Setup the sprite mesh / material --- */

    meshSprite = R3D_GenMeshQuad(1.0f, 1.0f, 1, 1, (Vector3) { 0.0f, 0.0f, 1.0f });
    meshSprite.shadowCastMode = R3D_SHADOW_CAST_ON_DOUBLE_SIDED;

    matSprite = R3D_GetDefaultMaterial();
    matSprite.billboardMode = R3D_BILLBOARD_Y_AXIS;
    matSprite.albedo.texture = LoadTexture(RESOURCES_PATH "spritesheet.png");

    /* --- Setup a spotlight in the scene --- */

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    {
        R3D_LightLookAt(light, (Vector3) { 0, 10, 10 }, (Vector3) { 0 });
        R3D_SetLightRange(light, 64.0f);

        R3D_EnableShadow(light, 1024);
        R3D_SetLightActive(light, true);
    }

    /* --- Setup the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 2, 5 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    return "[r3d] - Sprite example";
}

void Update(float delta)
{
    /* --- Update bird --- */

    Vector3 birdPosPrev = birdPos;

    birdPos.x = 2.0f * sinf((float)GetTime());
    birdPos.y = 1.0f + cosf((float)GetTime() * 4.0f) * 0.5f;
    birdDirX = (birdPos.x - birdPosPrev.x >= 0.0f) ? 1.0f : -1.0f;

    /* --- Update sprite --- */

    float currentFrame = 10.0f * GetTime();
    GetTexCoordScaleOffset(
        &matSprite.uvScale,
        &matSprite.uvOffset,
        4 * birdDirX,       // Multiply by the sign of the X direction to invert the uvScale.x
        1,
        currentFrame
    );
}

void Draw(void)
{
    R3D_Begin(camera);

    R3D_DrawMesh(&meshGround, &matGround, MatrixTranslate(0, -0.5f, 0));
    R3D_DrawMesh(&meshSprite, &matSprite, MatrixTranslate(birdPos.x, birdPos.y, 0.0f));

    R3D_End();
}

void Close(void)
{
    R3D_UnloadMaterial(&matSprite);
    R3D_UnloadMesh(&meshSprite);
    R3D_UnloadMesh(&meshGround);
    R3D_Close();
}
