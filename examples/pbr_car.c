#include "./common.h"
#include "r3d/r3d_skybox.h"

/* === Resources === */

static R3D_Model model = { 0 };
static R3D_Mesh ground = { 0 };
static R3D_Material groundMat = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };
static bool showSkybox = false;

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution and flags --- */

    R3D_Flags falgs = R3D_FLAG_TRANSPARENT_SORTING | R3D_FLAG_FXAA;

    R3D_Init(GetScreenWidth(), GetScreenHeight(), falgs);
    SetTargetFPS(60);

    /* --- Setup background color and ambient light --- */

    R3D_ENVIRONMENT_SET(background.color, BLACK);
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    /* --- Setup post processing parameters --- */

    R3D_ENVIRONMENT_SET(ssr.enabled, true);

    R3D_ENVIRONMENT_SET(ssao.enabled, true);
    R3D_ENVIRONMENT_SET(ssao.radius, 2.0f);

    R3D_ENVIRONMENT_SET(bloom.intensity, 0.1f);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);

    /* --- Load the car model and apply scaling on import --- */

    model = R3D_LoadModel(RESOURCES_PATH "pbr/car.glb");

    /* --- Generate ground mesh and setup its material --- */

    ground = R3D_GenMeshPlane(10.0f, 10.0f, 1, 1);

    groundMat = R3D_GetDefaultMaterial();
    groundMat.albedo.color = (Color) { 31, 31, 31, 255 };
    groundMat.orm.roughness = 0.0f;
    groundMat.orm.metalness = 0.5f;

    /* --- Load skybox (disabled by default) --- */

    skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox3.png", CUBEMAP_LAYOUT_AUTO_DETECT);

    /* --- Configure the scene lighting --- */

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    {
        R3D_SetLightDirection(light, (Vector3) { -1, -1, -1 });
        R3D_EnableShadow(light, 4096);
        R3D_SetLightActive(light, true);
        R3D_SetLightRange(light, 10);
    }

    /* --- Setup camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 0, 5 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    /* --- Capture the mouse and let's go! --- */

    DisableCursor();

    return "[r3d] - PBR car example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);

    if (IsKeyPressed(KEY_O)) {
        R3D_ENVIRONMENT_SET(ssao.enabled, !R3D_ENVIRONMENT_GET(ssao.enabled));
    }

    if (IsKeyPressed(KEY_T)) {
        showSkybox = !showSkybox;
        if (showSkybox) R3D_ENVIRONMENT_SET(background.sky, skybox);
        else R3D_ENVIRONMENT_SET(background.sky, (R3D_Skybox) {0});
    }
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawMesh(&ground, &groundMat, MatrixTranslate(0.0f, -0.4f, 0.0f));
        R3D_DrawModel(&model, (Vector3) { 0 }, 1.0f);
    R3D_End();

    DrawCredits("Model made by MaximePages");
}

void Close(void)
{
    R3D_UnloadModel(&model, true);
    R3D_UnloadSkybox(skybox);
    R3D_Close();
}
