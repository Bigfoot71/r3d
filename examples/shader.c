#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Shader example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Setup environment
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 10, 255});
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);

    // Create meshes
    R3D_Mesh plane = R3D_GenMeshPlane(1000, 1000, 1, 1);
    R3D_Mesh torus = R3D_GenMeshTorus(0.5f, 0.1f, 32, 16);

    // Create material
    R3D_Material material = R3D_GetDefaultMaterial();
    material.shader = R3D_LoadSurfaceShader(RESOURCES_PATH "shaders/material.glsl");

    // Generate a texture for custom sampler
    Image image = GenImageChecked(512, 512, 16, 32, WHITE, BLACK);
    Texture texture = LoadTextureFromImage(image);
    UnloadImage(image);

    // Set custom sampler
    R3D_SetSurfaceShaderSampler(material.shader, "u_texture", texture);

    // Load a screen shader
    R3D_ScreenShader* shader = R3D_LoadScreenShader(RESOURCES_PATH "shaders/screen.glsl");
    R3D_SetScreenShaderChain(&shader, 1);

    // Create light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){0, 10, 5}, (Vector3){0});
    R3D_EnableShadow(light);
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = {
        .position = {0, 2, 2},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(RAYWHITE);

            float time = 2.0f * GetTime();
            R3D_SetScreenShaderUniform(shader, "u_time", &time);
            R3D_SetSurfaceShaderUniform(material.shader, "u_time", &time);

            R3D_Begin(camera);
                R3D_DrawMesh(plane, R3D_MATERIAL_BASE, (Vector3) {0, -0.5f, 0}, 1.0f);
                R3D_DrawMesh(torus, material, Vector3Zero(), 1.0f);
            R3D_End();

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadSurfaceShader(material.shader);
    R3D_UnloadScreenShader(shader);
    R3D_UnloadMesh(torus);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
