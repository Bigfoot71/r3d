#include <r3d/r3d.h>
#include <math.h>

#define MAX_PARTICLES 4096

typedef struct {
    Vector3 pos;
    Vector3 vel;
    float life;
} Particle;

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Particles example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Set environment
    R3D_ENVIRONMENT_SET(background.color, (Color){4, 4, 4});
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_ADDITIVE);

    // Generate a gradient as emission texture for our particles
    Image image = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK);
    Texture texture = LoadTextureFromImage(image);
    UnloadImage(image);

    // Generate a quad mesh for our particles
    R3D_Mesh mesh = R3D_GenMeshQuad(0.25f, 0.25f, 1, 1, (Vector3){0, 0, 1});

    // Setup particle material
    R3D_Material material = R3D_GetDefaultMaterial();
    material.billboardMode = R3D_BILLBOARD_FRONT;
    material.blendMode = R3D_BLEND_ADDITIVE;
    material.albedo.texture = R3D_GetBlackTexture();
    material.emission.color = (Color){255, 0, 0, 255};
    material.emission.texture = texture;
    material.emission.energy = 1.0f;

    // Create particle instance buffer
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(MAX_PARTICLES, R3D_INSTANCE_POSITION);

    // Setup camera
    Camera3D camera = {
        .position = {-7, 7, -7},
        .target = {0, 1, 0},
        .up = {0, 1, 0},
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE
    };

    // CPU buffer for storing particles
    Particle particles[MAX_PARTICLES] = {0};
    Vector3 positions[MAX_PARTICLES];
    int particleCount = 0;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Spawn particles
        for (int i = 0; i < 10; i++) {
            if (particleCount < MAX_PARTICLES) {
                float angle = GetRandomValue(0, 360) * DEG2RAD;
                particles[particleCount].pos = (Vector3){0, 0, 0};
                particles[particleCount].vel = (Vector3){
                    cosf(angle) * GetRandomValue(20, 40) / 10.0f,
                    GetRandomValue(60, 80) / 10.0f,
                    sinf(angle) * GetRandomValue(20, 40) / 10.0f
                };
                particles[particleCount].life = 1.0f;
                particleCount++;
            }
        }

        // Update particles
        int alive = 0;
        for (int i = 0; i < particleCount; i++) {
            particles[i].vel.y -= 9.81f * dt;
            particles[i].pos.x += particles[i].vel.x * dt;
            particles[i].pos.y += particles[i].vel.y * dt;
            particles[i].pos.z += particles[i].vel.z * dt;
            particles[i].life -= dt * 0.5f;
            if (particles[i].life > 0) {
                positions[alive] = particles[i].pos;
                particles[alive] = particles[i];
                alive++;
            }
        }
        particleCount = alive;

        R3D_UploadInstances(instances, R3D_INSTANCE_POSITION, 0, particleCount, positions);

        BeginDrawing();
            R3D_Begin(camera);
                R3D_DrawMeshInstanced(mesh, material, instances, particleCount);
            R3D_End();
            DrawFPS(10, 10);
        EndDrawing();
    }

    R3D_UnloadInstanceBuffer(instances);
    R3D_UnloadMaterial(material);
    R3D_UnloadMesh(mesh);
    R3D_Close();

    CloseWindow();

    return 0;
}
