#include <r3d/r3d.h>
#include <raymath.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Transparency example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Create cube model
    R3D_Mesh cube = R3D_GenMeshCube(1, 1, 1);
    R3D_Material matCube = R3D_MATERIAL_BASE;
    matCube.transparencyMode = R3D_TRANSPARENCY_ALPHA;
    matCube.albedo.color = (Color){150, 150, 255, 100};
    matCube.orm.occlusion = 1.0f;
    matCube.orm.roughness = 0.2f;
    matCube.orm.metalness = 0.2f;

    // Create plane model
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Material matPlane = R3D_MATERIAL_BASE;
    matPlane.orm.occlusion = 1.0f;
    matPlane.orm.roughness = 1.0f;
    matPlane.orm.metalness = 0.0f;

    // Create sphere model
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Material matSphere = R3D_MATERIAL_BASE;
    matSphere.orm.occlusion = 1.0f;
    matSphere.orm.roughness = 0.25f;
    matSphere.orm.metalness = 0.75f;

    // Setup camera
    Camera3D camera = {
        .position = {0, 2, 2},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Setup lighting
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0, 10, 5}, (Vector3){0});
    R3D_SetLightActive(light, true);
    R3D_EnableShadow(light);

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawMesh(plane, matPlane, (Vector3){0, -0.5f, 0}, 1.0f);
                R3D_DrawMesh(sphere, matSphere, Vector3Zero(), 1.0f);
                R3D_DrawMesh(cube, matCube, Vector3Zero(), 1.0f);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(plane);
    R3D_UnloadMesh(cube);
    R3D_Close();

    CloseWindow();

    return 0;
}
