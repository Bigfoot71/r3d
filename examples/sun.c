#include <r3d/r3d.h>
#include <raymath.h>

#define X_INSTANCES 50
#define Y_INSTANCES 50
#define INSTANCE_COUNT (X_INSTANCES * Y_INSTANCES)

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Sun example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());
    R3D_SetAntiAliasing(R3D_ANTI_ALIASING_FXAA);

    // Create meshes and material
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.35f, 16, 32);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Create transforms for instanced spheres
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(INSTANCE_COUNT, R3D_INSTANCE_POSITION);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    float spacing = 1.5f;
    float offsetX = (X_INSTANCES * spacing) / 2.0f;
    float offsetZ = (Y_INSTANCES * spacing) / 2.0f;
    int idx = 0;
    for (int x = 0; x < X_INSTANCES; x++) {
        for (int y = 0; y < Y_INSTANCES; y++) {
            positions[idx] = (Vector3) {x * spacing - offsetX, 0, y * spacing - offsetZ};
            idx++;
        }
    }
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION);

    // Setup environment
    R3D_Cubemap skybox = R3D_GenCubemapSky(1024, R3D_CUBEMAP_SKY_BASE);
    R3D_ENVIRONMENT_SET(background.sky, skybox);

    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(skybox, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    R3D_ENVIRONMENT_SET(ambient.map, ambientMap);;

    // Create directional light with shadows
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1, -1, -1});
    R3D_SetLightActive(light, true);
    R3D_SetLightRange(light, 16.0f);
    R3D_SetShadowSoftness(light, 2.0f);
    R3D_EnableShadow(light);

    // Setup camera
    Camera3D camera = {
        .position = {0, 1, 0},
        .target = {1, 1.25f, 1},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Capture mouse
    DisableCursor();

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_FREE);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            R3D_Begin(camera);
                R3D_DrawMesh(plane, material, (Vector3) {0, -0.5f, 0}, 1.0f);
                R3D_DrawMeshInstanced(sphere, material, instances, INSTANCE_COUNT);
            R3D_End();
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadInstanceBuffer(instances);
    R3D_UnloadMaterial(material);
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
