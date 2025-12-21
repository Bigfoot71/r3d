#include "./common.h"

/* === Resources === */

static R3D_Model sponza = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };
static R3D_Light lights[2] = { 0 };

static bool sky = false;

/* === Examples === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Configure default post process settings --- */

    R3D_SetSSAO(true);
    R3D_SetSSAORadius(4.0f);
    R3D_SetSSAOIntensity(1.25f);
    R3D_SetSSAOPower(1.5f);

    R3D_SetBloomMode(R3D_BLOOM_MIX);

    /* --- Set default background and ambient color (when no skybox is activated) --- */

    R3D_SetBackgroundColor(SKYBLUE);
    R3D_SetAmbientColor(DARKGRAY);

    /* --- Load Sponza scene --- */

    sponza = R3D_LoadModel(RESOURCES_PATH "sponza.glb");

    /* --- Load skybox (disabled by default) --- */

    skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox3.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    //R3D_EnableSkybox(skybox);

    /* --- Configure lights --- */

    for (int i = 0; i < 2; i++)
    {
        lights[i] = R3D_CreateLight(R3D_LIGHT_OMNI);

        R3D_SetLightPosition(lights[i], (Vector3) { i ? -10.0f : 10.0f, 20.0f, 0.0f });
        R3D_SetLightActive(lights[i], true);
        R3D_SetLightEnergy(lights[i], 4.0f);

        R3D_SetShadowUpdateMode(lights[i], R3D_SHADOW_UPDATE_MANUAL);
        R3D_EnableShadow(lights[i], 4096);
    }

    /* --- Configure camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 8.0f, 1.0f, 0.5f },
        .target = (Vector3) { 0.0f, 2.0f, -2.0f },
        .up = (Vector3) { 0.0f, 1.0f, 0.0f },
        .fovy = 60.0f
    };

    /* --- Ready to go! --- */

    DisableCursor();

    return "[r3d] - Sponza example";
}

void Update(float delta)
{
    /* --- Update the camera via raylib's functions --- */

    UpdateCamera(&camera, CAMERA_FREE);

    /* --- Skybox toggling --- */

    if (IsKeyPressed(KEY_ZERO)) {
        if (sky) R3D_DisableSkybox();
        else R3D_EnableSkybox(skybox);
        sky = !sky;
    }

    /* --- SSAO toggling --- */

    if (IsKeyPressed(KEY_ONE)) {
        R3D_SetSSAO(!R3D_GetSSAO());
    }

    /* --- Fog toggling --- */

    if (IsKeyPressed(KEY_TWO)) {
        R3D_SetFogMode(R3D_GetFogMode() == R3D_FOG_DISABLED ? R3D_FOG_EXP : R3D_FOG_DISABLED);
    }

    /* --- FXAA toggling --- */

    if (IsKeyPressed(KEY_THREE)) {
        bool fxaa = R3D_HasState(R3D_FLAG_FXAA);
        if (fxaa) R3D_ClearState(R3D_FLAG_FXAA);
        else R3D_SetState(R3D_FLAG_FXAA);
    }

    /* --- Tonemapping setter --- */

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        R3D_Tonemap tonemap = R3D_GetTonemapMode();
        R3D_SetTonemapMode((tonemap + R3D_TONEMAP_COUNT - 1) % R3D_TONEMAP_COUNT);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        R3D_Tonemap tonemap = R3D_GetTonemapMode();
        R3D_SetTonemapMode((tonemap + 1) % R3D_TONEMAP_COUNT);
    }
}

void Draw(void)
{
    /* --- Render R3D scene --- */

    R3D_Begin(camera);
        R3D_DrawModel(&sponza, (Vector3) { 0 }, 1.0f);
    R3D_End();

    /* --- 'Standard' raylib rendering to show where are the lights --- */

    BeginMode3D(camera);
        DrawSphere(R3D_GetLightPosition(lights[0]), 0.5f, WHITE);
        DrawSphere(R3D_GetLightPosition(lights[1]), 0.5f, WHITE);
    EndMode3D();

    /* --- Indicates which tonemapping is used --- */

    R3D_Tonemap tonemap = R3D_GetTonemapMode();

    switch (tonemap) {
    case R3D_TONEMAP_LINEAR: {
        const char* txt = "< TONEMAP LINEAR >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);
    }
    break;
    case R3D_TONEMAP_REINHARD: {
        const char* txt = "< TONEMAP REINHARD >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);
    }
    break;
    case R3D_TONEMAP_FILMIC: {
        const char* txt = "< TONEMAP FILMIC >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);
    }
    break;
    case R3D_TONEMAP_ACES: {
        const char* txt = "< TONEMAP ACES >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);

    } break;
    case R3D_TONEMAP_AGX: {
        const char* txt = "< TONEMAP AGX >";
        DrawText(txt, GetScreenWidth() - MeasureText(txt, 20) - 10, 10, 20, LIME);

    } break;
    default:
        break;
    }

    /* --- I think we understand what's going on here --- */

    DrawFPS(10, 10);
}

void Close(void)
{
    R3D_UnloadModel(&sponza, true);
    R3D_UnloadSkybox(skybox);
    R3D_Close();
}
