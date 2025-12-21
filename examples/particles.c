#include "./common.h"

/* === Resources === */

static R3D_Mesh sphere = { 0 };
static R3D_Material material = { 0 };
static R3D_Skybox skybox = { 0 };
static Camera3D camera = { 0 };

static R3D_InterpolationCurve curve = { 0 };
static R3D_ParticleSystem particles = { 0 };

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Setup the background color and ambient light --- */

    R3D_ENVIRONMENT_SET(background.color, (Color) { 4, 4, 4 });
    R3D_ENVIRONMENT_SET(ambient.color, BLACK);

    /* --- Activate bloom in additive mode --- */

    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);

    /* --- Gen a sphere as particle mesh --- */

    sphere = R3D_GenMeshSphere(0.1f, 16, 32);

    /* --- Setup material of particle mesh --- */

    material = R3D_GetDefaultMaterial();
    material.emission.color = (Color) { 255, 0, 0, 255 };
    material.emission.energy = 1.0f;

    /* --- Create scale over time interpolation curve for particle system --- */

    curve = R3D_LoadInterpolationCurve(3);
    R3D_AddKeyframe(&curve, 0.0f, 0.0f);
    R3D_AddKeyframe(&curve, 0.5f, 1.0f);
    R3D_AddKeyframe(&curve, 1.0f, 0.0f);

    /* --- Create a particle system --- */

    particles = R3D_LoadParticleSystem(2048);
    particles.initialVelocity = (Vector3){ 0, 10.0f, 0 };
    particles.scaleOverLifetime = &curve;
    particles.spreadAngle = 45.0f;
    particles.emissionRate = 2048;
    particles.lifetime = 2.0f;

    /* --- Calculates the bounding box of the particle system (can be used for frustum culling) --- */

    R3D_CalculateParticleSystemBoundingBox(&particles);

    /* --- Setup the camera --- */

    camera = (Camera3D) {
        .position = (Vector3) { -7, 7, -7 },
        .target = (Vector3) { 0, 1, 0 },
        .up = (Vector3) { 0, 1, 0 },
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    return "[r3d] - Particles example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_ORBITAL);
    R3D_UpdateParticleSystem(&particles, GetFrameTime());
}

void Draw(void)
{
    R3D_Begin(camera);
        R3D_DrawParticleSystem(&particles, &sphere, &material);
    R3D_End();

    BeginMode3D(camera);
        DrawBoundingBox(particles.aabb, GREEN);
    EndMode3D();

    DrawFPS(10, 10);
}

void Close(void)
{
    R3D_UnloadInterpolationCurve(curve);
    R3D_UnloadParticleSystem(&particles);

    R3D_UnloadMesh(&sphere);
    R3D_UnloadMaterial(&material);

    R3D_Close();
}
