#include "./common.h"

/* === Resources === */

static R3D_Mesh plane = { 0 };
static R3D_Mesh sphere = { 0 };
static R3D_Material material = { 0 };
static Camera3D camera = { 0 };

static Matrix* transforms = NULL;

/* === Examples === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Generates a plane and sphere meshes and a default material to render them --- */

    plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    sphere = R3D_GenMeshSphere(0.35f, 16, 16);
    material = R3D_GetDefaultMaterial();

    /* --- Calculation of transformation matrices for all spheres instances --- */

    transforms = RL_MALLOC(100 * 100 * sizeof(Matrix));
    if (transforms == NULL) {
        TraceLog(LOG_FATAL, "EXAMPLE: Failed to allocate transforms buffer");
        exit(-1);
    }

    for (int x = -50; x < 50; x++) {
        for (int z = -50; z < 50; z++) {
            int index = (z + 50) * 100 + (x + 50);
            transforms[index] = MatrixTranslate((float)x * 2, 0, (float)z * 2);
        }
    }

    /* --- Setup the scene lighting --- */

    R3D_SetAmbientColor((Color) { 10, 10, 10, 255 });

    R3D_SetSceneBounds(
        (BoundingBox) {
            .min = { -100, -1, -100 },
            .max = { +100, +1, +100 }
        }
    );

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    {
        R3D_SetLightDirection(light, (Vector3) { 0, -1, -1 });
        R3D_SetShadowUpdateMode(light, R3D_SHADOW_UPDATE_MANUAL);
        R3D_SetLightActive(light, true);

        R3D_EnableShadow(light, 4096);
        R3D_SetShadowDepthBias(light, 0.01f);
        R3D_SetShadowSoftness(light, 2.0f);
    }

    /* --- Setup the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 2, 2 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    /* --- Capture the mouse and play! --- */

    DisableCursor();

    return "[r3d] - Directional light example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawMesh(&plane, &material, MatrixTranslate(0, -0.5f, 0));
        R3D_DrawMeshInstanced(&sphere, &material, transforms, 100 * 100);
    R3D_End();

    DrawFPS(10, 10);
}

void Close(void)
{
    R3D_UnloadMesh(&plane);
    R3D_UnloadMesh(&sphere);
    R3D_UnloadMaterial(&material);
    R3D_Close();
}
