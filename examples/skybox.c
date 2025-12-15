#include "./common.h"

/* === Resources === */

static R3D_Mesh sphere = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };

static R3D_Material materials[7 * 7] = { 0 };

/* === Examples === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Generate a sphere mesh --- */

    sphere = R3D_GenMeshSphere(0.5f, 64, 64);

    /* --- Create a grid of materials with varying metalness and roughness --- */

    for (int x = 0; x < 7; x++) {
        for (int y = 0; y < 7; y++) {
            int i = y * 7 + x;
            materials[i] = R3D_GetDefaultMaterial();
            materials[i].orm.metalness = (float)x / 7;
            materials[i].orm.roughness = (float)y / 7;
            materials[i].albedo.color = ColorFromHSV(((float)x / 7) * 360, 1, 1);
        }
    }

    /* --- Load and enable a skybox --- */

    skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox1.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_EnableSkybox(skybox);

    /* --- Setup the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 0, 5 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    /* --- Capture the mouse and, action! --- */

    DisableCursor();

    return "[r3d] - Skybox example";
}


void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);
}

void Draw(void)
{
    R3D_Begin(camera);
        for (int x = 0; x < 7; x++) {
            for (int y = 0; y < 7; y++) {
                R3D_DrawMesh(&sphere, &materials[y * 7 + x], MatrixTranslate(x - 3, y - 3, 0.0f));
            }
        }
    R3D_End();
}

void Close(void)
{
    R3D_UnloadMesh(&sphere);
    R3D_UnloadSkybox(skybox);
    R3D_Close();
}
