#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Probe example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Setup environment sky
    R3D_Cubemap cubemap = R3D_LoadCubemap(RESOURCES_PATH "panorama/indoor.hdr", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.skyBlur, 0.3f);
    R3D_ENVIRONMENT_SET(background.sky, cubemap);

    // Setup environment ambient
    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(cubemap, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    R3D_ENVIRONMENT_SET(ambient.map, ambientMap);
    R3D_ENVIRONMENT_SET(ambient.energy, 0.2f);

    // Setup tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);

    // Create meshes
    R3D_Mesh plane = R3D_GenMeshPlane(30, 30, 1, 1);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 64, 64);
    R3D_Material material = R3D_GetDefaultMaterial();

    // Create light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0, 10, 5}, (Vector3){0});
    R3D_SetLightActive(light, true);
    R3D_EnableShadow(light);

    // Create probe
    R3D_Probe probe = R3D_CreateProbe(R3D_PROBE_ILLUMINATION | R3D_PROBE_REFLECTION);
    R3D_SetProbePosition(probe, (Vector3) {0, 1, 0});
    R3D_SetProbeShadows(probe, true);
    R3D_SetProbeFalloff(probe, 0.5f);
    R3D_SetProbeActive(probe, true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 3.0f, 6.0f},
        .target = {0, 0.5f, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);

                material.orm.roughness = 0.5f;
                material.orm.metalness = 0.0f;
                R3D_DrawMesh(plane, material, Vector3Zero(), 1.0f);

                for (int i = -1; i <= 1; i++) {
                    material.orm.roughness = fabsf((float)i) * 0.4f;
                    material.orm.metalness = 1.0f - fabsf((float)i);
                    R3D_DrawMesh(sphere, material, (Vector3) {(float)i * 3.0f, 1.0f, 0}, 2.0f);
                }

            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadAmbientMap(ambientMap);
    R3D_UnloadCubemap(cubemap);
    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
