#include "./common.h"
#include "r3d.h"

/* === Resources === */

static R3D_Model cube = { 0 };
static R3D_Model plane = { 0 };
static R3D_Model sphere = { 0 };
static Camera3D camera = { 0 };

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Load cube model --- */

    R3D_Mesh mesh = { 0 };

    mesh = R3D_GenMeshCube(1, 1, 1, true);
    cube = R3D_LoadModelFromMesh(&mesh);

    cube.meshes[0].shadowCastMode = R3D_SHADOW_CAST_DISABLED;

    cube.materials[0].albedo.color = (Color){ 100, 100, 255, 100 };
    cube.materials[0].orm.occlusion = 1.0f;
    cube.materials[0].orm.roughness = 0.2f;
    cube.materials[0].orm.metalness = 0.2f;

    cube.materials[0].blendMode = R3D_BLEND_ALPHA;

    /* --- Load plane model --- */

    mesh = R3D_GenMeshPlane(1000, 1000, 1, 1, true);
    plane = R3D_LoadModelFromMesh(&mesh);

    plane.materials[0].orm.occlusion = 1.0f;
    plane.materials[0].orm.roughness = 1.0f;
    plane.materials[0].orm.metalness = 0.0f;

    /* --- Load sphere model --- */

    mesh = R3D_GenMeshSphere(0.5f, 64, 64, true);
    sphere = R3D_LoadModelFromMesh(&mesh);

    sphere.materials[0].orm.occlusion = 1.0f;
    sphere.materials[0].orm.roughness = 0.25f;
    sphere.materials[0].orm.metalness = 0.75f;

    /* --- Configure the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0, 2, 2 },
        .target = (Vector3) { 0, 0, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60,
    };

    /* --- Configure lighting --- */

    R3D_SetAmbientColor((Color) { 10, 10, 10, 255 });

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    {
        R3D_LightLookAt(light, (Vector3) { 0, 10, 5 }, (Vector3) { 0 });
        R3D_SetLightActive(light, true);
        R3D_EnableShadow(light, 4096);
    }

    return "[r3d] - Transparency example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_ORBITAL);
}

void Draw(void)
{
    R3D_Begin(camera);
    {
        R3D_DrawModel(&plane, (Vector3) { 0, -0.5f, 0 }, 1.0f);
        R3D_DrawModel(&sphere, (Vector3) { 0 }, 1.0f);
        R3D_DrawModel(&cube, (Vector3) { 0 }, 1.0f);
    }
    R3D_End();
}

void Close(void)
{
    R3D_UnloadModel(&plane, false);
    R3D_UnloadModel(&sphere, false);

    R3D_Close();
}
