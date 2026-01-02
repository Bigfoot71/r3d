#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

#define MAXDECALS 256

static bool RayCubeIntersection(Ray ray, Vector3 cubePos, Vector3 cubeSize, Vector3* intersectionPoint, Vector3* normal);
static Matrix MatrixTransform(Vector3 position, Quaternion rotation, Vector3 scale);

int main(void)
{
    InitWindow(800, 450, "[r3d] - Decal example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);

    // Load decal texture
    Texture2D texture = LoadTexture(RESOURCES_PATH "decal.png");

    // Create wall material
    R3D_Material materialWalls = R3D_GetDefaultMaterial();
    materialWalls.albedo.color = DARKGRAY;

    // Create decal material
    R3D_Decal decal;
    decal.material = R3D_GetDefaultMaterial();
    decal.material.albedo.texture = texture;

    // Create room mesh and transforms
    float roomSize = 32.0f;
    R3D_Mesh meshPlane = R3D_GenMeshPlane(roomSize, roomSize, 1, 1);

    Matrix matRoom[6];
    matRoom[0] = MatrixMultiply(MatrixRotateZ(90.0f * DEG2RAD), MatrixTranslate(roomSize / 2.0f, 0.0f, 0.0f));
    matRoom[1] = MatrixMultiply(MatrixRotateZ(-90.0f * DEG2RAD), MatrixTranslate(-roomSize / 2.0f, 0.0f, 0.0f));
    matRoom[2] = MatrixMultiply(MatrixRotateX(90.0f * DEG2RAD), MatrixTranslate(0.0f, 0.0f, -roomSize / 2.0f));
    matRoom[3] = MatrixMultiply(MatrixRotateX(-90.0f * DEG2RAD), MatrixTranslate(0.0f, 0.0f, roomSize / 2.0f));
    matRoom[4] = MatrixMultiply(MatrixRotateX(180.0f * DEG2RAD), MatrixTranslate(0.0f, roomSize / 2.0f, 0.0f));
    matRoom[5] = MatrixTranslate(0.0f, -roomSize / 2.0f, 0.0f);

    // Setup light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightEnergy(light, 2.0f);
    R3D_SetLightActive(light, true);

    // Setup camera
    Camera3D camera = (Camera3D) {
        .position = (Vector3) {0.0f, 0.0f, 0.0f},
        .target = (Vector3) {1.0f, 0.0f, 0.0f},
        .up = (Vector3) {0.0f, 1.0f, 0.0f},
        .fovy = 70,
    };

    DisableCursor();

    // Decal state
    Vector3 decalScale = {3.0f, 3.0f, 3.0f};
    Matrix targetDecalTransform = MatrixIdentity();
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(MAXDECALS, R3D_INSTANCE_POSITION | R3D_INSTANCE_ROTATION | R3D_INSTANCE_SCALE);
    int decalCount = 0;
    int decalIndex = 0;
    Vector3 targetPosition = {0};

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        UpdateCamera(&camera, CAMERA_FREE);

        // Compute ray from camera to target
        Ray hitRay = {camera.position, Vector3Normalize(Vector3Subtract(camera.target, camera.position))};

        Vector3 hitPoint = {0}, hitNormal = {0};
        if (RayCubeIntersection(hitRay, Vector3Zero(), (Vector3) { roomSize, roomSize, roomSize }, &hitPoint, &hitNormal)) {
            targetPosition = hitPoint;
        }

        // Compute decal rotation
        Quaternion decalRotation = QuaternionIdentity();
        if (hitNormal.x == -1.0f) decalRotation = QuaternionFromMatrix(MatrixRotateXYZ((Vector3) { -90.0f * DEG2RAD, 180.0f * DEG2RAD, 90.0f * DEG2RAD }));
        else if (hitNormal.x == 1.0f) decalRotation = QuaternionFromMatrix(MatrixRotateXYZ((Vector3) { -90.0f * DEG2RAD, 180.0f * DEG2RAD, -90.0f * DEG2RAD }));
        else if (hitNormal.y == -1.0f) decalRotation = QuaternionFromMatrix(MatrixRotateY(180.0f * DEG2RAD));
        else if (hitNormal.y == 1.0f) decalRotation = QuaternionFromMatrix(MatrixRotateZ(180.0f * DEG2RAD));
        else if (hitNormal.z == -1.0f) decalRotation = QuaternionFromMatrix(MatrixRotateX(90.0f * DEG2RAD));
        else if (hitNormal.z == 1.0f) decalRotation = QuaternionFromMatrix(MatrixRotateXYZ((Vector3) { -90.0f * DEG2RAD, 180.0f * DEG2RAD, 0 }));

        // Apply decal on mouse click
        if (IsMouseButtonPressed(0)) {
            R3D_UploadInstances(instances, R3D_INSTANCE_POSITION, decalIndex, 1, &targetPosition);
            R3D_UploadInstances(instances, R3D_INSTANCE_ROTATION, decalIndex, 1, &decalRotation);
            R3D_UploadInstances(instances, R3D_INSTANCE_SCALE, decalIndex, 1, &decalScale);
            decalIndex = (decalIndex + 1) % MAXDECALS;
            if (decalCount < MAXDECALS) decalCount++;
        }

        // Draw scene
        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);

            for (int i = 0; i < 6; i++) {
                R3D_DrawMeshPro(meshPlane, materialWalls, matRoom[i]);
            }

            if (decalCount > 0) {
                R3D_DrawDecalInstanced(decal, instances, decalCount);
            }

            R3D_DrawDecal(decal, MatrixTransform(targetPosition, decalRotation, decalScale));

        R3D_End();

        BeginMode3D(camera);
        DrawCubeWires(targetPosition, decalScale.x, decalScale.y, decalScale.z, WHITE);
        EndMode3D();

        DrawText("LEFT CLICK TO APPLY DECAL", 10, 10, 20, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(meshPlane);
    R3D_Close();

    CloseWindow();

    return 0;
}

bool RayCubeIntersection(Ray ray, Vector3 cubePosition, Vector3 cubeSize, Vector3* intersectionPoint, Vector3* normal)
{
    Vector3 halfSize = { cubeSize.x / 2, cubeSize.y / 2, cubeSize.z / 2 };

    Vector3 min = { cubePosition.x - halfSize.x, cubePosition.y - halfSize.y, cubePosition.z - halfSize.z };
    Vector3 max = { cubePosition.x + halfSize.x, cubePosition.y + halfSize.y, cubePosition.z + halfSize.z };

    float tMin = (min.x - ray.position.x) / ray.direction.x;
    float tMax = (max.x - ray.position.x) / ray.direction.x;

    if (tMin > tMax) { float temp = tMin; tMin = tMax; tMax = temp; }

    float tYMin = (min.y - ray.position.y) / ray.direction.y;
    float tYMax = (max.y - ray.position.y) / ray.direction.y;

    if (tYMin > tYMax) { float temp = tYMin; tYMin = tYMax; tYMax = temp; }

    if ((tMin > tYMax) || (tYMin > tMax)) return false;

    if (tYMin > tMin) tMin = tYMin;
    if (tYMax < tMax) tMax = tYMax;

    float tZMin = (min.z - ray.position.z) / ray.direction.z;
    float tZMax = (max.z - ray.position.z) / ray.direction.z;

    if (tZMin > tZMax) { float temp = tZMin; tZMin = tZMax; tZMax = temp; }

    if ((tMin > tZMax) || (tZMin > tMax)) return false;

    if (tZMin > tMin) tMin = tZMin;
    if (tZMax < tMax) tMax = tZMax;

    if (tMin < 0) {
        tMin = tMax;
        if (tMin < 0) return false;
    }

    *intersectionPoint = (Vector3){ ray.position.x + ray.direction.x * tMin,
                                     ray.position.y + ray.direction.y * tMin,
                                     ray.position.z + ray.direction.z * tMin };

    if (fabs(intersectionPoint->x - min.x) < 0.001f) normal->x = -1.0f; // Left face
    else if (fabs(intersectionPoint->x - max.x) < 0.001f) normal->x = 1.0f; // Right face

    if (fabs(intersectionPoint->y - min.y) < 0.001f) normal->y = -1.0f; // Bottom face
    else if (fabs(intersectionPoint->y - max.y) < 0.001f) normal->y = 1.0f; // Top face

    if (fabs(intersectionPoint->z - min.z) < 0.001f) normal->z = -1.0f; // Near face
    else if (fabs(intersectionPoint->z - max.z) < 0.001f) normal->z = 1.0f; // Far face

    return true;
}

static Matrix MatrixTransform(Vector3 position, Quaternion rotation, Vector3 scale)
{
    float xx = rotation.x * rotation.x;
    float yy = rotation.y * rotation.y;
    float zz = rotation.z * rotation.z;
    float xy = rotation.x * rotation.y;
    float xz = rotation.x * rotation.z;
    float yz = rotation.y * rotation.z;
    float wx = rotation.w * rotation.x;
    float wy = rotation.w * rotation.y;
    float wz = rotation.w * rotation.z;

    return (Matrix) {
        scale.x * (1.0 - 2.0 * (yy + zz)), scale.y * 2.0 * (xy - wz),         scale.z * 2.0 * (xz + wy),         position.x,
        scale.x * 2.0 * (xy + wz),         scale.y * (1.0 - 2.0 * (xx + zz)), scale.z * 2.0 * (yz - wx),         position.y,
        scale.x * 2.0 * (xz - wy),         scale.y * 2.0 * (yz + wx),         scale.z * (1.0 - 2.0 * (xx + yy)), position.z,
        0.0,                               0.0,                               0.0,                               1.0
    };
}
