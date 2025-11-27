#include "./common.h"
#include "r3d.h"

#define MAXDECALS 64

/* === Resources === */

static R3D_Mesh meshPlane;
static R3D_Material materialWalls = { 0 };
static Texture2D texture = { 0 };
static R3D_Decal decal = { 0 };
static Camera3D camera = { 0 };
static R3D_Light light = { 0 };

/* === Data === */

float roomSize = 32.0f;
Matrix matRoom[6];

Vector3 decalScale = { 3.0f, 3.0f, 3.0f };
Matrix targetDecalTransform = { 0 };

Matrix decalTransforms[MAXDECALS];

int decalCount = 0;
int decalIndex = 0;

Vector3 targetPosition = { 0 };

/* === Helper Functions === */

static bool RayCubeIntersection(Ray ray, Vector3 cubePosition, Vector3 cubeSize, Vector3* intersectionPoint, Vector3* normal) {
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

static void DrawTransformedCube(Matrix transform, Color color) {
    static Vector3 vertices[8] = {
        { -0.5f, -0.5f, -0.5f },
        {  0.5f, -0.5f, -0.5f },
        {  0.5f,  0.5f, -0.5f },
        { -0.5f,  0.5f, -0.5f },
        { -0.5f, -0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        { -0.5f,  0.5f,  0.5f },
    };

    Vector3 transformedVertices[8];

    for (int i = 0; i < 8; i++) {
        transformedVertices[i] = Vector3Transform(vertices[i], transform);
    }

    for (int i = 0; i < 4; i++) {
        DrawLine3D(transformedVertices[i], transformedVertices[(i + 1) % 4], color);           // Bottom
        DrawLine3D(transformedVertices[i + 4], transformedVertices[(i + 1) % 4 + 4], color);   // Top
        DrawLine3D(transformedVertices[i], transformedVertices[i + 4], color);                 // Sides
    }
}

/* === Example === */

const char* Init(void)
{
    /* --- Initialize R3D with its internal resolution --- */

    R3D_Init(GetScreenWidth(), GetScreenHeight(), 0);
    SetTargetFPS(60);

    /* --- Load textures --- */

    texture = LoadTexture(RESOURCES_PATH "decal.png");

    /* --- Create materials --- */

    materialWalls = R3D_GetDefaultMaterial();
    materialWalls.albedo.color = DARKGRAY;

    decal.material = R3D_GetDefaultMaterial();
    decal.material.albedo.texture = texture;
    decal.material.blendMode = R3D_BLEND_ALPHA;

    /* --- Create a plane along with the transformation matrices to place them to represent a room --- */

    meshPlane = R3D_GenMeshPlane(roomSize, roomSize, 1, 1, true);

    matRoom[0] = MatrixMultiply(MatrixRotateZ(90.0f * DEG2RAD), MatrixTranslate(roomSize / 2.0f, 0.0f, 0.0f));
    matRoom[1] = MatrixMultiply(MatrixRotateZ(-90.0f * DEG2RAD), MatrixTranslate(-roomSize / 2.0f, 0.0f, 0.0f));
    matRoom[2] = MatrixMultiply(MatrixRotateX(90.0f * DEG2RAD), MatrixTranslate(0.0f, 0.0f, -roomSize / 2.0f));
    matRoom[3] = MatrixMultiply(MatrixRotateX(-90.0f * DEG2RAD), MatrixTranslate(0.0f, 0.0f, roomSize / 2.0f));
    matRoom[4] = MatrixMultiply(MatrixRotateX(180.0f * DEG2RAD), MatrixTranslate(0.0f, roomSize / 2.0f, 0.0f));
    matRoom[5] = MatrixTranslate(0.0f, -roomSize / 2.0f, 0.0f);

    /* --- Setup the scene lighting --- */

    light = R3D_CreateLight(R3D_LIGHT_OMNI);
    R3D_SetLightActive(light, true);

    /* --- Setup the camera --- */

    camera = (Camera3D){
        .position = (Vector3) { 0.0f, 0.0f, 0.0f },
        .target = (Vector3) { roomSize / 2.0f, 0.0f, 0.0f },
        .up = (Vector3) { 0.0f, 1.0f, 0.0f },
        .fovy = 70,
    };

    DisableCursor();

    return "[r3d] - Decal example";
}

void Update(float delta)
{
    UpdateCamera(&camera, CAMERA_FREE);

    /* --- Find intersection point of camera target on cube --- */

    Ray hitRay = {
        .position = camera.position,
        .direction = Vector3Normalize(Vector3Subtract(camera.target, camera.position))
    };

    Vector3 hitPoint = { 0 };
    Vector3 hitNormal = { 0 };

    if (RayCubeIntersection(hitRay, Vector3Zero(), (Vector3) { roomSize, roomSize, roomSize }, &hitPoint, &hitNormal)) {
        targetPosition = hitPoint;
    }

    /* --- Create transformation matrix at intersection point --- */

    Matrix translation = MatrixTranslate(targetPosition.x, targetPosition.y, targetPosition.z);
    Matrix scaling = MatrixScale(decalScale.x, decalScale.y, decalScale.z);
    Matrix rotation = MatrixIdentity();

    if (hitNormal.x == -1.0f)
        rotation = MatrixRotateXYZ((Vector3) { -90.0f * DEG2RAD, 180.0f * DEG2RAD, 90.0f * DEG2RAD });
    else if (hitNormal.x == 1.0f)
        rotation = MatrixRotateXYZ((Vector3) { -90.0f * DEG2RAD, 180.0f * DEG2RAD, -90.0f * DEG2RAD });
    else if (hitNormal.y == -1.0f)
        rotation = MatrixRotateY(180.0f * DEG2RAD);
    else if (hitNormal.y == 1.0f)
        rotation = MatrixRotateZ(180.0f * DEG2RAD);
    else if (hitNormal.z == -1.0f)
        rotation = MatrixRotateX(90.0f * DEG2RAD);
    else if (hitNormal.z == 1.0f)
        rotation = MatrixRotateXYZ((Vector3) { -90.0f * DEG2RAD, 180.0f * DEG2RAD, 0 });

    targetDecalTransform = MatrixMultiply(MatrixMultiply(scaling, rotation), translation);

    /* --- Input --- */

    if (IsMouseButtonPressed(0)) {
        decalTransforms[decalIndex] = targetDecalTransform;
        decalIndex++;
        if (decalIndex >= MAXDECALS) decalIndex = 0;
        if (decalCount < MAXDECALS) decalCount++;
    }
}

void Draw(void)
{
    R3D_Begin(camera);

    /* --- Draw the faces of our "room" --- */
    for (int i = 0; i < 6; i++) {
        R3D_DrawMesh(&meshPlane, &materialWalls, matRoom[i]);
    }

    /* --- Draw applied decals --- */
    for (int i = 0; i < decalCount; i++) {
        R3D_DrawDecal(&decal, decalTransforms[i]);
    }

    /* --- Draw targeting decal --- */
    R3D_DrawDecal(&decal, targetDecalTransform);

    R3D_End();

    /* --- Show decal projection box --- */
    BeginMode3D(camera);
    DrawTransformedCube(targetDecalTransform, WHITE);
    EndMode3D();

    DrawText("LEFT CLICK TO APPLY DECAL", 10, 10, 20, LIME);
}

void Close(void)
{
    R3D_UnloadMesh(&meshPlane);
    R3D_Close();
}
