#include "./common.h"

/* === Resources === */

static R3D_Model model = { 0 };
static Matrix modelMatrix = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };

static float modelScale = 1.0f;

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution and flags --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);
    SetTargetFPS(60);

    /* --- Setup tonemapping --- */

    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.75f);
    R3D_ENVIRONMENT_SET(tonemap.white, 1.25f);

    /* --- Load model --- */

    R3D_SetTextureFilter(TEXTURE_FILTER_TRILINEAR);	//< Automatically generate mipmaps when set

    model = R3D_LoadModel(RESOURCES_PATH "pbr/musket.glb");

    /* --- Setup defaut model matrix --- */

    modelMatrix = MatrixIdentity();

    /* --- Load and enable the skybox --- */

    skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox2.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.sky, skybox);

    /* --- Setup the scene lighting --- */

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    {
        R3D_SetLightDirection(light, (Vector3) { 0, -1, -1 });
        R3D_SetLightActive(light, true);
    }

    /* --- Setup the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 0, 0.5f },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    return "[r3d] - PBR musket example";
}

void Update(float delta)
{
    modelScale = Clamp(modelScale + GetMouseWheelMove() * 0.1f, 0.25f, 2.5f);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        float pitch = (GetMouseDelta().y * 0.005f) / modelScale;
        float yaw = (GetMouseDelta().x * 0.005f) / modelScale;

        Matrix rotate = MatrixRotateXYZ((Vector3) { pitch, yaw, 0.0f });
        modelMatrix = MatrixMultiply(modelMatrix, rotate);
    }
}

void Draw(void)
{
    R3D_Begin(camera);
        Matrix scale = MatrixScale(modelScale, modelScale, modelScale);
        Matrix transform = MatrixMultiply(modelMatrix, scale);
        R3D_DrawModelPro(&model, transform);
    R3D_End();

    DrawCredits("Model made by TommyLingL");
}

void Close(void)
{
    R3D_UnloadModel(&model, true);
    R3D_UnloadSkybox(skybox);
    R3D_Close();
}
