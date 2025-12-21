#include "./common.h"

/* === Resources === */

static R3D_Model model = { 0 };
static R3D_Mesh plane = { 0 };
static R3D_Material material = { 0 };
static Camera3D camera = { 0 };
static R3D_Light light = { 0 };
static float rotModel = 0.0f;

/* === Toggle Light === */

void ToggleLight(void)
{
    if (R3D_IsLightActive(light)) {
        R3D_SetLightActive(light, false);
        R3D_ENVIRONMENT_SET(ambient.color, BLACK);
    }
    else {
        R3D_SetLightActive(light, true);
        R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);
    }
}

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Configure the background color and ambient lighting --- */

    R3D_ENVIRONMENT_SET(background.color, BLACK);
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    /* --- Configure the post process parameters --- */

    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.8f);
    R3D_ENVIRONMENT_SET(tonemap.white, 2.5f);

    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);
    R3D_ENVIRONMENT_SET(bloom.softThreshold, 0.2f);
    R3D_ENVIRONMENT_SET(bloom.threshold, 0.6f);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.2f);

    /* --- Loads the main model of the scene --- */

    model = R3D_LoadModel(RESOURCES_PATH "emission.glb");

    /* --- Generates a mesh for the ground and load a material for it --- */
    
    plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    material = R3D_GetDefaultMaterial();

    /* --- Setup the scene lighting --- */

    light = R3D_CreateLight(R3D_LIGHT_SPOT);
    {
        R3D_LightLookAt(light, (Vector3) { 0, 10, 5 }, (Vector3) { 0 });
        R3D_SetLightOuterCutOff(light, 45.0f);
        R3D_SetLightInnerCutOff(light, 22.5f);
        R3D_EnableShadow(light, 4096);
        R3D_SetLightActive(light, true);
    }

    /* --- Setup the camera --- */

    camera = (Camera3D) {
        .position = (Vector3) { -1.0f, 1.75f, 1.75f },
        .target = (Vector3) { 0, 0.5f, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    return "[r3d] - Emission example";
}

void Update(float delta)
{
    if (IsKeyPressed(KEY_SPACE)) {
        ToggleLight();
    }

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        camera.position.y = Clamp(camera.position.y + 0.01f * GetMouseDelta().y, 0.25f, 2.5f);
        rotModel += 0.01f * GetMouseDelta().x;
    }
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawMesh(&plane, &material, MatrixIdentity());
        R3D_DrawModelEx(&model, (Vector3) { 0 }, (Vector3) { 0, 1, 0 }, rotModel, (Vector3) { 1.0f, 1.0f, 1.0f });
    R3D_End();

    DrawText("Press SPACE to toggle the light", 10, 10, 20, LIME);
    DrawCredits("Model by har15204405");
}

void Close(void)
{
    R3D_UnloadModel(&model, true);
    R3D_UnloadMesh(&plane);
    R3D_Close();
}
