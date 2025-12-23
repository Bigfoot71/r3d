#include <r3d/r3d.h>

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Particles example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set environment
    R3D_ENVIRONMENT_SET(background.color, (Color){4, 4, 4});
    R3D_ENVIRONMENT_SET(ambient.color, BLACK);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);

    // Create particle mesh and material
    R3D_Mesh sphere = R3D_GenMeshSphere(0.1f, 16, 32);
    R3D_Material material = R3D_GetDefaultMaterial();
    material.emission.color = (Color){255, 0, 0, 255};
    material.emission.energy = 1.0f;

    // Create interpolation curve for particle scaling
    R3D_InterpolationCurve curve = R3D_LoadInterpolationCurve(3);
    R3D_AddKeyframe(&curve, 0.0f, 0.0f);
    R3D_AddKeyframe(&curve, 0.5f, 1.0f);
    R3D_AddKeyframe(&curve, 1.0f, 0.0f);

    // Create particle system
    R3D_ParticleSystem particles = R3D_LoadParticleSystem(2048);
    particles.initialVelocity = (Vector3){0, 10.0f, 0};
    particles.scaleOverLifetime = &curve;
    particles.spreadAngle = 45.0f;
    particles.emissionRate = 2048;
    particles.lifetime = 2.0f;

    R3D_CalculateParticleSystemBoundingBox(&particles);

    // Setup camera
    Camera3D camera = {
        .position = {-7, 7, -7},
        .target = {0, 1, 0},
        .up = {0, 1, 0},
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    // Main loop
    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);
        R3D_UpdateParticleSystem(&particles, GetFrameTime());

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawParticleSystem(&particles, &sphere, &material);
            R3D_End();

            // Draw bounding box
            BeginMode3D(camera);
                DrawBoundingBox(particles.aabb, GREEN);
            EndMode3D();

            DrawFPS(10, 10);
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadInterpolationCurve(curve);
    R3D_UnloadParticleSystem(&particles);
    R3D_UnloadMesh(&sphere);
    R3D_UnloadMaterial(&material);
    R3D_Close();
    CloseWindow();

    return 0;
}
