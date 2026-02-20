#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Skybox example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Create sphere mesh
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 32, 64);

    // Define procedural skybox parameters
    R3D_ProceduralSky skyParams = R3D_PROCEDURAL_SKY_BASE;
    skyParams.groundEnergy = 2.0f;
    skyParams.skyEnergy = 2.0f;
    skyParams.sunEnergy = 2.0f;

    // Load and generate skyboxes
    R3D_Cubemap skyProcedural = R3D_GenProceduralSky(1024, skyParams);
    R3D_Cubemap skyPanorama = R3D_LoadCubemap(RESOURCES_PATH "panorama/sky.hdr", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);

    // Generate ambient maps
    R3D_AmbientMap ambientProcedural = R3D_GenAmbientMap(skyProcedural, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    R3D_AmbientMap ambientPanorama = R3D_GenAmbientMap(skyPanorama, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);

    // Set default sky/ambient maps
    R3D_ENVIRONMENT_SET(background.sky, skyPanorama);
    R3D_ENVIRONMENT_SET(ambient.map, ambientPanorama);

    // Set tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_AGX);

    // Setup camera
    Camera3D camera = {
        .position = {0, 0, 10},
        .target = {0, 0, 0},
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

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (R3D_ENVIRONMENT_GET(background.sky.texture) == skyPanorama.texture) {
                R3D_ENVIRONMENT_SET(background.sky, skyProcedural);
                R3D_ENVIRONMENT_SET(ambient.map, ambientProcedural);
            }
            else {
                R3D_ENVIRONMENT_SET(background.sky, skyPanorama);
                R3D_ENVIRONMENT_SET(ambient.map, ambientPanorama);
            }
        }

        // Draw sphere grid
        R3D_Begin(camera);
            for (int x = 0; x <= 8; x++) {
                for (int y = 0; y <= 8; y++) {
                    R3D_Material material = R3D_MATERIAL_BASE;
                    material.orm.roughness = Remap((float)y, 0.0f, 8.0f, 0.0f, 1.0f);
                    material.orm.metalness = Remap((float)x, 0.0f, 8.0f, 0.0f, 1.0f);
                    R3D_DrawMesh(sphere, material, (Vector3) {(float)(x - 4) * 1.25f, ((float)y - 4) * 1.25f, 0.0f}, 1.0f);
                }
            }
        R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadAmbientMap(ambientProcedural);
    R3D_UnloadAmbientMap(ambientPanorama);
    R3D_UnloadCubemap(skyProcedural);
    R3D_UnloadCubemap(skyPanorama);
    R3D_UnloadMesh(sphere);
    R3D_Close();

    CloseWindow();

    return 0;
}
