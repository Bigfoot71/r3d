#include <r3d/r3d.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdio.h>

#define X_INSTANCES 10
#define Y_INSTANCES 10
#define INSTANCE_COUNT (X_INSTANCES * Y_INSTANCES)

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - DoF example");
    SetTargetFPS(60);

    // Initialize R3D with FXAA
    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);

    // Configure depth of field and background
    R3D_ENVIRONMENT_SET(background.color, BLACK);
    R3D_ENVIRONMENT_SET(dof.mode, R3D_DOF_ENABLED);
    R3D_ENVIRONMENT_SET(dof.focusPoint, 2.0f);
    R3D_ENVIRONMENT_SET(dof.focusScale, 3.0f);
    R3D_ENVIRONMENT_SET(dof.maxBlurSize, 20.0f);
    R3D_ENVIRONMENT_SET(dof.debugMode, false);

    // Create directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){0, -1, 0});
    R3D_SetLightActive(light, true);

    // Create sphere mesh and default material
    R3D_Mesh meshSphere = R3D_GenMeshSphere(0.2f, 64, 64);
    R3D_Material matDefault = R3D_GetDefaultMaterial();

    // Generate instance matrices and colors
    float spacing = 0.5f;
    float offsetX = (X_INSTANCES * spacing) / 2.0f;
    float offsetZ = (Y_INSTANCES * spacing) / 2.0f;
    int idx = 0;
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(INSTANCE_COUNT, R3D_INSTANCE_POSITION | R3D_INSTANCE_COLOR);
    Vector3* positions = R3D_MapInstances(instances, R3D_INSTANCE_POSITION);
    Color* colors = R3D_MapInstances(instances, R3D_INSTANCE_COLOR);
    for (int x = 0; x < X_INSTANCES; x++) {
        for (int y = 0; y < Y_INSTANCES; y++) {
            positions[idx] = (Vector3) {x * spacing - offsetX, 0, y * spacing - offsetZ};
            colors[idx] = (Color){rand()%256, rand()%256, rand()%256, 255};
            idx++;
        }
    }
    R3D_UnmapInstances(instances, R3D_INSTANCE_POSITION | R3D_INSTANCE_COLOR);

    // Setup camera
    Camera3D camDefault = {
        .position = {0, 2, 2},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        // Rotate camera
        Matrix rotation = MatrixRotate(camDefault.up, 0.1f * delta);
        Vector3 view = Vector3Subtract(camDefault.position, camDefault.target);
        view = Vector3Transform(view, rotation);
        camDefault.position = Vector3Add(camDefault.target, view);

        // Adjust DoF based on mouse
        Vector2 mousePos = GetMousePosition();
        float focusPoint = 0.5f + (5.0f - (mousePos.y / GetScreenHeight()) * 5.0f);
        float focusScale = 0.5f + (5.0f - (mousePos.x / GetScreenWidth()) * 5.0f);
        R3D_ENVIRONMENT_SET(dof.focusPoint, focusPoint);
        R3D_ENVIRONMENT_SET(dof.focusScale, focusScale);

        float mouseWheel = GetMouseWheelMove();
        if (mouseWheel != 0.0f) {
            float maxBlur = R3D_ENVIRONMENT_GET(dof.maxBlurSize);
            R3D_ENVIRONMENT_SET(dof.maxBlurSize, maxBlur + mouseWheel * 0.1f);
        }

        if (IsKeyPressed(KEY_F1)) {
            R3D_ENVIRONMENT_SET(dof.debugMode, !R3D_ENVIRONMENT_GET(dof.debugMode));
        }

        BeginDrawing();
            ClearBackground(BLACK);

            // Render scene
            R3D_Begin(camDefault);
                R3D_DrawMeshInstanced(meshSphere, matDefault, instances, INSTANCE_COUNT);
            R3D_End();

            // Display DoF values
            char dofText[128];
            snprintf(dofText, sizeof(dofText), "Focus Point: %.2f\nFocus Scale: %.2f\nMax Blur Size: %.2f\nDebug Mode: %d",
                R3D_ENVIRONMENT_GET(dof.focusPoint), R3D_ENVIRONMENT_GET(dof.focusScale),
                R3D_ENVIRONMENT_GET(dof.maxBlurSize), R3D_ENVIRONMENT_GET(dof.debugMode));

            DrawText(dofText, 10, 30, 20, (Color) {255, 255, 255, 127});

            // Display instructions
            DrawText("F1: Toggle Debug Mode\nScroll: Adjust Max Blur Size\nMouse Left/Right: Shallow/Deep DoF\nMouse Up/Down: Adjust Focus Point Depth", 300, 10, 20, (Color) {255, 255, 255, 127});

            // Display FPS
            char fpsText[32];
            snprintf(fpsText, sizeof(fpsText), "FPS: %d", GetFPS());
            DrawText(fpsText, 10, 10, 20, (Color) {255, 255, 255, 127});

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadInstanceBuffer(instances);
    R3D_UnloadMesh(meshSphere);
    R3D_Close();

    CloseWindow();

    return 0;
}
