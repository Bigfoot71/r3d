#include <r3d/r3d.h>
#include <raymath.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Transparency example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Create cube model
    R3D_Mesh mesh = R3D_GenMeshCube(1, 1, 1);
    R3D_Model cube = R3D_LoadModelFromMesh(mesh);
    cube.materials[0].transparencyMode = R3D_TRANSPARENCY_ALPHA;
    cube.materials[0].albedo.color = (Color){100, 100, 255, 100};
    cube.materials[0].orm.occlusion = 1.0f;
    cube.materials[0].orm.roughness = 0.2f;
    cube.materials[0].orm.metalness = 0.2f;

    // Create plane model
    mesh = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Model plane = R3D_LoadModelFromMesh(mesh);
    plane.materials[0].orm.occlusion = 1.0f;
    plane.materials[0].orm.roughness = 1.0f;
    plane.materials[0].orm.metalness = 0.0f;

    // Create sphere model
    mesh = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Model sphere = R3D_LoadModelFromMesh(mesh);
    sphere.materials[0].orm.occlusion = 1.0f;
    sphere.materials[0].orm.roughness = 0.25f;
    sphere.materials[0].orm.metalness = 0.75f;

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
    R3D_EnableShadow(light, 4096);

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawModel(plane, (Vector3){0, -0.5f, 0}, 1.0f);
                R3D_DrawModel(sphere, Vector3Zero(), 1.0f);
                R3D_DrawModel(cube, Vector3Zero(), 1.0f);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadModel(sphere, false);
    R3D_UnloadModel(plane, false);
    R3D_UnloadModel(cube, false);
    R3D_Close();

    CloseWindow();

    return 0;
}
