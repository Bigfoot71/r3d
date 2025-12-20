#include "./common.h"
#include "r3d/r3d_lighting.h"
#include "r3d/r3d_material.h"
#include "raymath.h"

/* === Resources === */

static Camera3D camera = { 0 };

static R3D_Mesh meshGround = { 0 };
static R3D_Material matGround = { 0 };

static R3D_Mesh meshBillboard = { 0 };
static R3D_Material matBillboard = { 0 };
static Matrix transformsBillboards[64] = { 0 };

/* === Examples === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Set the background color --- */

    R3D_SetBackgroundColor(SKYBLUE);

    /* --- Setup the ground mesh / material --- */

    meshGround = R3D_GenMeshPlane(200, 200, 1, 1);
    matGround = R3D_GetDefaultMaterial();
    matGround.albedo.color = GREEN;

    /* --- Setup the billboard mesh / material --- */

    meshBillboard = R3D_GenMeshQuad(1.0f, 1.0f, 1, 1, (Vector3) { 0.0f, 0.0f, 1.0f });
    meshBillboard.shadowCastMode = R3D_SHADOW_CAST_ON_DOUBLE_SIDED;

    matBillboard = R3D_GetDefaultMaterial();
    matBillboard.billboardMode = R3D_BILLBOARD_Y_AXIS;
    matBillboard.albedo.texture = LoadTexture(RESOURCES_PATH "tree.png");

    /* --- Create multiple transforms for instanced billboards --- */

    for (int i = 0; i < sizeof(transformsBillboards) / sizeof(*transformsBillboards); i++) {
        float scaleFactor = GetRandomValue(25, 50) / 10.0f;
        Matrix scale = MatrixScale(scaleFactor, scaleFactor, 1.0f);
        Matrix translate = MatrixTranslate((float)GetRandomValue(-100, 100), scaleFactor * 0.5f, (float)GetRandomValue(-100, 100));
        transformsBillboards[i] = MatrixMultiply(scale, translate);
    }

    /* --- Setup the scene lighting --- */

    R3D_SetSceneBounds(
        (BoundingBox) {
            .min = { -100, -10, -100 },
            .max = { +100, +10, +100 }
        }
    );

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    {
        R3D_SetLightDirection(light, (Vector3) { -1, -1, -1 });
        R3D_EnableShadow(light, 4096);
        R3D_SetLightActive(light, true);
    }

    /* --- Setup the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 5, 0 },
        .target = (Vector3) { 0, 5, -1 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    /* --- Capture the mouse and let's go! --- */

    DisableCursor();

    return "[r3d] - Billboards example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);
}

void Draw(void)
{
    R3D_Begin(camera);
    R3D_DrawMesh(&meshGround, &matGround, MatrixIdentity());
    R3D_DrawMeshInstanced(&meshBillboard, &matBillboard, transformsBillboards, sizeof(transformsBillboards) / sizeof(*transformsBillboards));
    R3D_End();
}

void Close(void)
{
    R3D_UnloadMaterial(&matBillboard);
    R3D_UnloadMesh(&meshBillboard);
    R3D_UnloadMesh(&meshGround);
    R3D_Close();
}
