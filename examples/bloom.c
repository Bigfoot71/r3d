#include "./common.h"
#include "raylib.h"

// TODO: Adding bloom prefilter settings here

/* === Resources === */

static R3D_Mesh cube = { 0 };
static R3D_Material material = { 0 };
static Camera3D camera = { 0 };
static float hueCube = 0.0f;

/* === Local Functions === */

static const char* GetBloomModeName(void)
{
    switch (R3D_ENVIRONMENT_GET(bloom.mode)) {
        case R3D_BLOOM_DISABLED:
            return "Disabled";
        case R3D_BLOOM_MIX:
            return "Mix";
        case R3D_BLOOM_ADDITIVE:
            return "Additive";
        case R3D_BLOOM_SCREEN:
            return "Screen";
        }

    return "Unknown";
}

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Setup the default bloom parameters --- */

    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(bloom.levels, 1.0f);

    R3D_ENVIRONMENT_SET(background.color, BLACK);

    /* --- Load a cube mesh and setup its material --- */

    cube = R3D_GenMeshCube(1.0f, 1.0f, 1.0f);
    material = R3D_GetDefaultMaterial();

    material.emission.color = ColorFromHSV(hueCube, 1.0f, 1.0f);
    material.emission.energy = 1.0f;
    material.albedo.color = BLACK;

    /* --- Setup the camera --- */    

    camera = (Camera3D){
        .position = (Vector3) { 0, 3.5, 5 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    return "[r3d] - Bloom example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_ORBITAL);

    if (IsKeyDown(KEY_C)) {
        hueCube = Wrap(hueCube + 45.0f * delta, 0, 360);
        material.emission.color = ColorFromHSV(hueCube, 1.0f, 1.0f);
    }

    int intensityDir = (IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_RIGHT)) -
                       (IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_LEFT));

    if (intensityDir != 0) {
        R3D_ENVIRONMENT_SET(bloom.intensity, R3D_ENVIRONMENT_GET(bloom.intensity) + intensityDir * 0.01f);
    }

    int radiusDir = (IsKeyPressedRepeat(KEY_UP) || IsKeyPressed(KEY_UP)) -
                    (IsKeyPressedRepeat(KEY_DOWN) || IsKeyPressed(KEY_DOWN));

    if (radiusDir != 0) {
        R3D_ENVIRONMENT_SET(bloom.filterRadius, R3D_ENVIRONMENT_GET(bloom.filterRadius) + radiusDir * 0.1f);
    }

    int levelDir = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) - IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (levelDir != 0) {
        R3D_ENVIRONMENT_SET(bloom.levels, Clamp(R3D_ENVIRONMENT_GET(bloom.levels) + levelDir * 0.01f, 0.0f, 1.0f));
    }

    if (IsKeyPressed(KEY_SPACE)) {
        R3D_ENVIRONMENT_SET(bloom.mode, (R3D_ENVIRONMENT_GET(bloom.mode) + 1) % (R3D_BLOOM_SCREEN + 1));
    }
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawMesh(&cube, &material, MatrixIdentity());
    R3D_End();

    R3D_DrawBufferEmission(10, 10, 100, 100);

    const char* infoStr;
    int infoLen;

    infoStr = TextFormat("Mode: %s", GetBloomModeName());
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 10, 20, LIME);

    infoStr = TextFormat("Intensity: %.2f", R3D_ENVIRONMENT_GET(bloom.intensity));
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 40, 20, LIME);

    infoStr = TextFormat("Filter Radius: %.2f", R3D_ENVIRONMENT_GET(bloom.filterRadius));
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 70, 20, LIME);

    infoStr = TextFormat("Levels: %.2f", R3D_ENVIRONMENT_GET(bloom.levels));
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 100, 20, LIME);
}

void Close(void)
{
    R3D_UnloadMesh(&cube);
    R3D_Close();
}
