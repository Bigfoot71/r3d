#include "./common.h"

/* === Resources === */

static R3D_Mesh plane = { 0 };
static R3D_Material planeMat = { 0 };

static R3D_Model dancer = { 0 };
static Matrix dancerInstances[2*2] = { 0 };
static R3D_AnimationLib dancerAnims = { 0 };
static R3D_AnimationPlayer* dancerPlayer = NULL;

static Camera3D camera = { 0 };
static R3D_Light lights[2] = { 0 };

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with FXAA and disable frustum culling --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA | R3D_FLAG_NO_FRUSTUM_CULLING);

    /* --- Set the application frame rate --- */

    SetTargetFPS(60);

    /* --- Enable post-processing effects --- */

    R3D_ENVIRONMENT_SET(ssao.enabled, true);
    R3D_ENVIRONMENT_SET(bloom.intensity, 0.03f);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);

    /* --- Set background and ambient lighting colors --- */

    R3D_ENVIRONMENT_SET(background.color, BLACK);
    R3D_ENVIRONMENT_SET(ambient.color, (Color) { 7, 7, 7, 255 });

    /* --- Generate a plane to serve as the ground and setup its material --- */

    plane = R3D_GenMeshPlane(32, 32, 1, 1);

    planeMat = R3D_GetDefaultMaterial();
    planeMat.orm.roughness = 0.5f;
    planeMat.orm.metalness = 0.5f;
    planeMat.uvScale.x = 64.0f;
    planeMat.uvScale.y = 64.0f;

    Image checked = GenImageChecked(2, 2, 1, 1, (Color) { 20, 20, 20, 255 }, WHITE);
    planeMat.albedo.texture = LoadTextureFromImage(checked);
    UnloadImage(checked);

    SetTextureWrap(planeMat.albedo.texture, TEXTURE_WRAP_REPEAT);

    /* --- Load the 3D model and its default material --- */

    dancer = R3D_LoadModel(RESOURCES_PATH "dancer.glb");

    /* --- Create instance matrices for multiple model copies --- */

    for (int z = 0; z < 2; z++) {
        for (int x = 0; x < 2; x++) {
            dancerInstances[z * 2 + x] = MatrixTranslate((float)x - 0.5f, 0, (float)z - 0.5f);
        }
    }

    /* --- Load model animations --- */

    dancerAnims = R3D_LoadAnimationLib(RESOURCES_PATH "dancer.glb");
    dancer.player = R3D_LoadAnimationPlayer(&dancer.skeleton, &dancerAnims);
    dancer.player->states[0].weight = 1.0f;
    dancer.player->states[0].loop = true;

    /* --- Setup scene lights with shadows --- */

    lights[0] = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(lights[0], (Vector3) { -10.0f, 25.0f, 0.0f });
    R3D_EnableShadow(lights[0], 4096);
    R3D_SetLightActive(lights[0], true);

    lights[1] = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(lights[1], (Vector3) { +10.0f, 25.0f, 0.0f });
    R3D_EnableShadow(lights[1], 4096);
    R3D_SetLightActive(lights[1], true);

    /* --- Setup the camera --- */

    camera = (Camera3D) {
        .position = (Vector3) { 0, 2.0f, 3.5f },
        .target = (Vector3) { 0, 1.0f, 1.5f },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    /* --- Capture the mouse and let's go! --- */

    DisableCursor();

    return "[r3d] - Animation example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);
    R3D_UpdateAnimationPlayer(dancer.player, delta);

    R3D_SetLightColor(lights[0], ColorFromHSV(90.0f * (float)GetTime() + 90.0f, 1.0f, 1.0f));
    R3D_SetLightColor(lights[1], ColorFromHSV(90.0f * (float)GetTime() - 90.0f, 1.0f, 1.0f));
}

void Draw(void)
{
    static int frame = 0;

    R3D_Begin(camera);
        R3D_DrawMesh(&plane, &planeMat, MatrixIdentity());
        R3D_DrawModel(&dancer, (Vector3) { 0, 0, 1.5f }, 1.0f);
        R3D_DrawModelInstanced(&dancer, dancerInstances, 2*2);
    R3D_End();

	DrawCredits("Model made by zhuoyi0904");
}

void Close(void)
{
    R3D_UnloadMesh(&plane);
    R3D_UnloadModel(&dancer, true);
    R3D_UnloadMaterial(&planeMat);
    R3D_Close();
}
