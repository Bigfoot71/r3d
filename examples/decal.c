#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#   define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Decal example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Create meshes
    R3D_Mesh plane = R3D_GenMeshPlane(5.0f, 5.0f, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Mesh cylinder = R3D_GenMeshCylinder(0.5f, 0.5f, 1, 64);
    R3D_Material material = R3D_GetDefaultMaterial();
    material.albedo.color = GRAY;

    // Create decal
    R3D_Decal decal = R3D_DECAL_BASE;
    R3D_SetTextureFilter(TEXTURE_FILTER_BILINEAR);
    decal.albedo = R3D_LoadAlbedoMap(RESOURCES_PATH "images/decal.png", WHITE);
    decal.normal = R3D_LoadNormalMap(RESOURCES_PATH "images/decal_normal.png", 1.0f);
    decal.normalThreshold = 45.0f;
    decal.fadeWidth = 20.0f;

    // Create data for instanced drawing
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(3, R3D_INSTANCE_POSITION);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    positions[0] = (Vector3){ -1.25f, 0, 1 };
    positions[1] = (Vector3){ 0, 0, 1 };
    positions[2] = (Vector3){ 1.25f, 0, 1 };
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION);

    // Setup environment
    R3D_ENVIRONMENT_SET(ambient.color, (Color){ 10, 10, 10, 255 });

    // Create light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){ 0.5f, -1, -0.5f });
    R3D_SetShadowDepthBias(light, 0.005f);
    R3D_EnableShadow(light);
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = (Camera3D){
        .position = (Vector3){0, 3, 3},
        .target = (Vector3){0, 0, 0},
        .up = (Vector3){0, 1, 0},
        .fovy = 60,
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
                R3D_DrawMesh(plane, material, (Vector3){ 0, 0, 0 }, 1.0f);
                R3D_DrawMesh(sphere, material, (Vector3){ -1, 0.5f, -1 }, 1.0f);
                R3D_DrawMeshEx(cylinder, material, (Vector3){ 1, 0.5f, -1 }, QuaternionFromEuler(0, 0, PI/2), Vector3One());
             
                R3D_DrawDecal(decal, (Vector3){ -1, 1, -1 }, 1.0f);
                R3D_DrawDecalEx(decal, (Vector3){ 1, 0.5f, -0.5f }, QuaternionFromEuler(PI/2, 0, 0), (Vector3){1.25f, 1.25f, 1.25f});
                R3D_DrawDecalInstanced(decal, instances, 3);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(plane);
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(cylinder);
    R3D_UnloadMaterial(material);
    R3D_UnloadDecalMaps(decal);
    R3D_Close();

    CloseWindow();

    return 0;
}