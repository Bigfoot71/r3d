#include "./common.h"

/* === Resources === */

static Camera3D camera = { 0 };

static R3D_Mesh plane = { 0 };
static R3D_Material material = { 0 };

static Texture2D texture = { 0 };
static R3D_Sprite sprite = { 0 };

/* === Bird Data === */

float birdDirX = 1.0f;
Vector3 birdPos = { 0.0f, 0.5f, 0.0f };

/* === Examples === */

const char* Init(void)
{
    /* --- Initialize R3D with screen resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Generate a large plane to act as the ground --- */

    plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    material = R3D_GetDefaultMaterial();

    /* --- Load a sprite sheet texture and set its filter --- */

    texture = LoadTexture(RESOURCES_PATH "spritesheet.png");
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);

    /* --- Create a sprite from the loaded texture --- */

    sprite = R3D_LoadSprite(texture, 4, 1);

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
    R3D_UpdateSprite(&sprite, 10 * delta);

    Vector3 birdPosPrev = birdPos;

    birdPos.x = 2.0f * sinf((float)GetTime());
    birdPos.y = 1.0f + cosf((float)GetTime() * 4.0f) * 0.5f;
    birdDirX = (birdPos.x - birdPosPrev.x >= 0.0f) ? 1.0f : -1.0f;
}

void Draw(void)
{
    R3D_Begin(camera);

    R3D_DrawMesh(&plane, &material, MatrixTranslate(0, -0.5f, 0));
    R3D_DrawSpriteEx(&sprite, birdPos, (Vector2) { birdDirX, 1.0f }, 0.0f);

    R3D_End();
}

void Close(void)
{
    R3D_UnloadSprite(&sprite);
    R3D_UnloadMesh(&plane);
    UnloadTexture(texture);
    R3D_Close();
}
