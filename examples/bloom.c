#include "./common.h"

// TODO: Adding bloom prefilter settings here

/* === Resources === */

static R3D_Mesh cube = { 0 };
static R3D_Material material = { 0 };
static Camera3D camera = { 0 };
static float hueCube = 0.0f;

/* === Local Functions === */

static const char* getBloomModeName(R3D_Bloom mode)
{
    switch (R3D_GetBloomMode()) {
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

    R3D_SetTonemapMode(R3D_TONEMAP_ACES);
    R3D_SetBloomMode(R3D_BLOOM_MIX);
    R3D_SetBackgroundColor(BLACK);

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

    int hueDir = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) - IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (hueDir != 0) {
        hueCube = Wrap(hueCube + hueDir * 90.0f * delta, 0, 360);
        material.emission.color = ColorFromHSV(hueCube, 1.0f, 1.0f);
    }

    int intensityDir = (IsKeyPressedRepeat(KEY_RIGHT) || IsKeyPressed(KEY_RIGHT)) -
                       (IsKeyPressedRepeat(KEY_LEFT) || IsKeyPressed(KEY_LEFT));

    if (intensityDir != 0) {
        R3D_SetBloomIntensity(R3D_GetBloomIntensity() + intensityDir * 0.01f);
    }

    int radiusDir = (IsKeyPressedRepeat(KEY_UP) || IsKeyPressed(KEY_UP)) -
                    (IsKeyPressedRepeat(KEY_DOWN) || IsKeyPressed(KEY_DOWN));

    if (radiusDir != 0) {
        R3D_SetBloomFilterRadius(R3D_GetBloomFilterRadius() + radiusDir);
    }

    if (IsKeyPressed(KEY_SPACE)) {
        R3D_SetBloomMode((R3D_GetBloomMode() + 1) % (R3D_BLOOM_SCREEN + 1));
    }
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawMesh(&cube, &material, MatrixIdentity());
    R3D_End();

    R3D_DrawBufferEmission(10, 10, 100, 100);
    R3D_DrawBufferBloom(120, 10, 100, 100);

    const char* infoStr;
    int infoLen;

    infoStr = TextFormat("Mode: %s", getBloomModeName(R3D_GetBloomMode()));
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 10, 20, LIME);

    infoStr = TextFormat("Intensity: %.2f", R3D_GetBloomIntensity());
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 40, 20, LIME);

    infoStr = TextFormat("Filter Radius: %i", R3D_GetBloomFilterRadius());
    infoLen = MeasureText(infoStr, 20);
    DrawText(infoStr, GetScreenWidth() - infoLen - 10, 70, 20, LIME);
}

void Close(void)
{
    R3D_UnloadMesh(&cube);
    R3D_Close();
}
