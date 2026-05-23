#include <r3d/r3d.h>
#include <raymath.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Multiview");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Create meshes
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Setup environment
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});

    // Create light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0, 10, 5}, (Vector3){0});
    R3D_EnableShadow(light);
    R3D_SetLightActive(light, true);

    // Setup cameras
    R3D_Camera cam0 = R3D_CAMERA_BASE;
    R3D_Camera cam1 = R3D_CAMERA_BASE;

    // Main loop
    while (!WindowShouldClose())
    {
        float time = (float)GetTime();

        cam0.position.x = 4.0f * cosf(time);
        cam0.position.y = 4.0f;
        cam0.position.z = 4.0f * sinf(time);

        cam1.position.x = 4.0f * cosf(-time);
        cam1.position.y = 4.0f;
        cam1.position.z = 4.0f * sinf(-time);

        R3D_CameraLookAt(&cam0, (Vector3){0}, (Vector3){0, 1, 0});
        R3D_CameraLookAt(&cam1, (Vector3){0}, (Vector3){0, 1, 0});

        float hw = (float)GetScreenWidth()/2;
        float h = (float)GetScreenHeight();

        R3D_View view0 = {.camera = cam0, .viewport = {0,  0, hw, h}};
        R3D_View view1 = {.camera = cam1, .viewport = {hw, 0, hw, h}};

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_BeginPro(view0);
                R3D_DrawMesh(plane, material, (Vector3) {0, -0.5f, 0}, 1.0f);
                R3D_DrawMesh(sphere, material, Vector3Zero(), 1.0f);
            R3D_End();

            R3D_BeginPro(view1);
                R3D_DrawMesh(plane, material, (Vector3) {0, -0.5f, 0}, 1.0f);
                R3D_DrawMesh(sphere, material, Vector3Zero(), 1.0f);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
