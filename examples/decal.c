#include "r3d/r3d_material.h"
#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
#endif

#define MAXDECALS 32

typedef struct Surface {
    Vector3 position;
    Vector3 normal;
    Matrix rotation;
    Matrix translation;
} Surface;

static Matrix MatrixTransform(Vector3 position, Quaternion rotation, Vector3 scale);
static bool RayIntersectsSurface(Ray ray, Surface* surface, float size, Vector3* intersectionOut);

int main(void)
{
    InitWindow(800, 450, "[r3d] - Decal example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Create decal
    R3D_Decal decal = R3D_DECAL_BASE;
    //decal.albedo = R3D_LoadAlbedoMap(RESOURCES_PATH "images/decal.png", WHITE);
    decal.normal = R3D_LoadNormalMap(RESOURCES_PATH "images/decal_normal.png", 1.0f);
    decal.normalThreshold = 89.0f;

    // Create room mesh, material and data
    float roomSize = 25.0f;
    R3D_Mesh meshPlane = R3D_GenMeshPlane(roomSize, roomSize, 1, 1);

    R3D_Material materialRoom = R3D_GetDefaultMaterial();
    materialRoom.albedo.color = GRAY;

    Surface surfaces[6] = {
        {{0.0f, roomSize / 2, 0.0f}, {0.0f, -1.0f, 0.0f}, MatrixRotateX(180.0f * DEG2RAD), MatrixTranslate(0.0f, roomSize / 2, 0.0f)},
        {{0.0f, -roomSize / 2, 0.0f}, {0.0f, 1.0f, 0.0f}, MatrixIdentity(), MatrixTranslate(0.0f, -roomSize / 2, 0.0f)},
        {{roomSize / 2, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, MatrixRotateZ(90.0f * DEG2RAD), MatrixTranslate(roomSize / 2, 0.0f, 0.0f)},
        {{-roomSize / 2, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, MatrixRotateZ(-90.0f * DEG2RAD), MatrixTranslate(-roomSize / 2, 0.0f, 0.0f)},
        {{0.0f, 0.0f, roomSize / 2}, {0.0f, 0.0f, -1.0f}, MatrixRotateX(-90.0f * DEG2RAD), MatrixTranslate(0.0f, 0.0f, roomSize / 2)},
        {{0.0f, 0.0f, -roomSize / 2}, {0.0f, 0.0f, 1.0f}, MatrixRotateX(90.0f * DEG2RAD), MatrixTranslate(0.0f, 0.0f, -roomSize / 2)}
    };

    // Setup light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightPosition(light, (Vector3) { roomSize * 0.3f, roomSize * 0.3f, roomSize * 0.3f });
    R3D_SetLightEnergy(light, 2.0f);
    R3D_SetLightActive(light, true);
    R3D_ENVIRONMENT_SET(ambient.color, DARKGRAY);

    // Setup camera
    Camera3D camera = (Camera3D){
        .position = (Vector3) {0.0f, 0.0f, 0.0f},
        .target = (Vector3) {0.0f, 0.0f, 1.0f},
        .up = (Vector3) {0.0f, 1.0f, 0.0f},
        .fovy = 70,
    };

    DisableCursor();

    // Decal state
    Vector3 decalScale = (Vector3){ 5.0f, 5.0f, 5.0f };
    Matrix targetDecalTransform = MatrixIdentity();
    R3D_InstanceBuffer instances = R3D_LoadInstanceBuffer(MAXDECALS, R3D_INSTANCE_POSITION | R3D_INSTANCE_ROTATION | R3D_INSTANCE_SCALE);
    int decalCount = 0;
    int decalIndex = 0;

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        UpdateCamera(&camera, CAMERA_FREE);

        // Compute ray from camera to target
        Ray hitRay = { camera.position, Vector3Normalize(Vector3Subtract(camera.target, camera.position)) };

        // Check ray interection
        Vector3 hitPoint = { 0 };
        Quaternion decalRotation = QuaternionIdentity();

        for (int i = 0; i < 6; i++)
        {
            if (RayIntersectsSurface(hitRay, &surfaces[i], roomSize, &hitPoint)) {
                decalRotation = QuaternionFromMatrix(surfaces[i].rotation);
                break;
            }
        }

        // Apply decal on mouse click
        if (IsMouseButtonPressed(0)) {
            R3D_UploadInstances(instances, R3D_INSTANCE_POSITION, decalIndex, 1, &hitPoint);
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
            Matrix transform = MatrixMultiply(surfaces[i].rotation, surfaces[i].translation);
            R3D_DrawMeshPro(meshPlane, materialRoom, transform);
        }

        if (decalCount > 0) {
            R3D_DrawDecalInstanced(decal, instances, decalCount);
        }

        R3D_DrawDecal(decal, MatrixTransform(hitPoint, decalRotation, decalScale));

        R3D_End();

        BeginMode3D(camera);
        DrawCubeWires(hitPoint, decalScale.x, decalScale.y, decalScale.z, WHITE);
        EndMode3D();

        DrawText("LEFT CLICK TO APPLY DECAL", 10, 10, 20, LIME);

        EndDrawing();
    }

    // Cleanup
    R3D_UnloadMesh(meshPlane);
    R3D_UnloadDecalMaps(decal);
    R3D_Close();

    CloseWindow();

    return 0;
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

static bool RayIntersectsSurface(Ray ray, Surface* surface, float size, Vector3* intersectionOut) {
    Vector3 surfaceNormal = surface->normal;
    Vector3 surfacePosition = surface->position;

    float d = -Vector3DotProduct(surfaceNormal, surfacePosition);
    float t = -(Vector3DotProduct(surfaceNormal, ray.position) + d) / Vector3DotProduct(surfaceNormal, ray.direction);

    if (t > 0) {
        // Calculate the intersection point
        Vector3 intersectionPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));

        Vector3 topLeft = Vector3Subtract(surfacePosition, (Vector3) { size / 2.0f, 0.0f, size / 2.0f });
        Vector3 bottomRight = Vector3Add(surfacePosition, (Vector3) { size / 2.0f, 0.0f, size / 2.0f });

        // Check if the intersection point is within the bounds of the surface
        if (intersectionPoint.x >= topLeft.x && intersectionPoint.x <= bottomRight.x &&
            intersectionPoint.z >= topLeft.z && intersectionPoint.z <= bottomRight.z) {

            // Offset the intersection point
            *intersectionOut = intersectionPoint;

            return true;
        }
    }

    return false;
}
