#include <r3d/r3d.h>
#include <raymath.h>
#include <stddef.h>

#ifndef RESOURCES_PATH
#define RESOURCES_PATH "./"
#endif

#define GRAVITY     -15.0f
#define MOVE_SPEED  5.0f
#define JUMP_FORCE  8.0f

#define CAPSULE_CENTER(caps) Vector3Scale(Vector3Add((caps).start, (caps).end), 0.5f)
#define BOX_CENTER(box) Vector3Scale(Vector3Add((box).min, (box).max), 0.5f)

int main(void)
{
    InitWindow(800, 450, "[r3d] - Kinematics Example");
    SetTargetFPS(60);

    R3D_Init(GetScreenWidth(), GetScreenHeight());
    R3D_SetTextureFilter(TEXTURE_FILTER_ANISOTROPIC_8X);

    R3D_Cubemap sky = R3D_GenProceduralSky(1024, R3D_PROCEDURAL_SKY_BASE);
    R3D_AmbientMap ambient = R3D_GenAmbientMap(sky, R3D_AMBIENT_ILLUMINATION | R3D_AMBIENT_REFLECTION);
    R3D_ENVIRONMENT_SET(background.sky, sky);
    R3D_ENVIRONMENT_SET(ambient.map, ambient);

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3) {-1, -1, -1});
    R3D_SetLightRange(light, 16.0f);
    R3D_SetLightActive(light, true);
    R3D_EnableShadow(light);

    // Load materials
    R3D_AlbedoMap baseAlbedo = R3D_LoadAlbedoMap(RESOURCES_PATH "images/placeholder.png", WHITE);

    R3D_Material groundMat = R3D_GetDefaultMaterial();
    groundMat.uvScale = (Vector2) {250.0f, 250.0f};
    groundMat.albedo = baseAlbedo;

    R3D_Material slopeMat = R3D_GetDefaultMaterial();
    slopeMat.albedo.color = (Color) {255,255,0,255};
    slopeMat.albedo.texture = baseAlbedo.texture;

    // Ground
    R3D_Mesh groundMesh = R3D_GenMeshPlane(1000, 1000, 1, 1);
    BoundingBox groundBox = {.min = {-500, -1, -500}, .max = {500, 0, 500}};

    // Slope obstacle
    R3D_MeshData slopeMeshData = R3D_GenMeshDataSlope(2, 2, 2, (Vector3) {0, 1, -1});
    R3D_Mesh slopeMesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, slopeMeshData, NULL, R3D_STATIC_MESH);
    Matrix slopeTransform = MatrixTranslate(0, 1, 5);

    // Player capsule
    R3D_Capsule capsule = {.start = {0, 0.5f, 0}, .end = {0, 1.5f, 0}, .radius = 0.5f};
    R3D_Mesh capsMesh = R3D_GenMeshCapsule(0.5f, 1.0f, 64, 32);
    Vector3 velocity = {0, 0, 0};

    // Camera
    float cameraAngle = 0.0f;
    float cameraPitch = 30.0f;
    Camera3D camera = {
        .position = {0, 5, 5},
        .target = CAPSULE_CENTER(capsule),
        .up = {0, 1, 0},
        .fovy = 60
    };

    DisableCursor();

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        // Camera rotation
        Vector2 mouseDelta = GetMouseDelta();
        cameraAngle -= mouseDelta.x * 0.15f;
        cameraPitch = Clamp(cameraPitch + mouseDelta.y * 0.15f, -7.5f, 80.0f);

        // Movement input relative to camera
        int dx = IsKeyDown(KEY_A) - IsKeyDown(KEY_D);
        int dz = IsKeyDown(KEY_W) - IsKeyDown(KEY_S);
        
        Vector3 moveInput = {0};
        if (dx != 0 || dz != 0) {
            float angleRad = cameraAngle * DEG2RAD;
            Vector3 right = {cosf(angleRad), 0, -sinf(angleRad)};
            Vector3 forward = {sinf(angleRad), 0, cosf(angleRad)};
            moveInput = Vector3Normalize(Vector3Add(Vector3Scale(right, dx), Vector3Scale(forward, dz)));
        }

        // Check grounded
        bool isGrounded = R3D_IsCapsuleGroundedBox(capsule, 0.01f, groundBox, NULL) ||
                          R3D_IsCapsuleGroundedMesh(capsule, 0.3f, slopeMeshData, slopeTransform, NULL);

        // Jump and apply gravity
        if (isGrounded && IsKeyPressed(KEY_SPACE)) velocity.y = JUMP_FORCE;
        if (!isGrounded) velocity.y += GRAVITY * dt;
        else if (velocity.y < 0) velocity.y = 0;

        // Calculate total movement
        Vector3 movement = Vector3Scale(moveInput, MOVE_SPEED * dt);
        movement.y = velocity.y * dt;

        // Apply movement with collision
        movement = R3D_SlideCapsuleMesh(capsule, movement, slopeMeshData, slopeTransform, NULL);
        capsule.start = Vector3Add(capsule.start, movement);
        capsule.end = Vector3Add(capsule.end, movement);

        // Ground clamp
        if (capsule.start.y < 0.5f) {
            float correction = 0.5f - capsule.start.y;
            capsule.start.y += correction;
            capsule.end.y += correction;
            velocity.y = 0;
        }

        // Update camera position
        Vector3 target = CAPSULE_CENTER(capsule);
        float pitchRad = cameraPitch * DEG2RAD;
        float angleRad = cameraAngle * DEG2RAD;
        camera.position = (Vector3) {
            target.x - sinf(angleRad) * cosf(pitchRad) * 5.0f,
            target.y + sinf(pitchRad) * 5.0f,
            target.z - cosf(angleRad) * cosf(pitchRad) * 5.0f
        };
        camera.target = target;

        BeginDrawing();
            ClearBackground(BLACK);
            R3D_Begin(camera);
                R3D_DrawMeshPro(slopeMesh, slopeMat, slopeTransform);
                R3D_DrawMesh(groundMesh, groundMat, Vector3Zero(), 1.0f);
                R3D_DrawMesh(capsMesh, R3D_MATERIAL_BASE, CAPSULE_CENTER(capsule), 1.0f);
            R3D_End();
            DrawFPS(10, 10);
            DrawText(isGrounded ? "GROUNDED" : "AIRBORNE", 10, GetScreenHeight() - 30, 20, isGrounded ? LIME : YELLOW);
        EndDrawing();
    }

    R3D_UnloadMeshData(slopeMeshData);
    R3D_UnloadMesh(groundMesh);
    R3D_UnloadMesh(slopeMesh);
    R3D_UnloadMesh(capsMesh);
    R3D_Close();

    CloseWindow();

    return 0;
}
