#include <r3d/r3d.h>
#include <raymath.h>

static bool IsKeyDownDelay(int key);
static const char* GetBloomModeName(void);
static void DrawTextRight(const char* text, int y, int fontSize, Color color);
static void AdjustBloomParam(float* param, int direction, float step, float min, float max);

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Bloom example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight());

    // Setup bloom and tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);
    R3D_ENVIRONMENT_SET(bloom.mode, R3D_BLOOM_MIX);
    R3D_ENVIRONMENT_SET(bloom.levels, 1.0f);

    // Set background
    R3D_ENVIRONMENT_SET(background.color, BLACK);

    // Create cube mesh and material
    R3D_Mesh cube = R3D_GenMeshCube(1.0f, 1.0f, 1.0f);
    R3D_Material material = R3D_GetDefaultMaterial();
    float hueCube = 0.0f;
    material.emission.color = ColorFromHSV(hueCube, 1.0f, 1.0f);
    material.emission.energy = 1.0f;
    material.albedo.color = BLACK;

    // Setup camera
    Camera3D camera = {
        .position = {0, 3.5f, 5},
        .target   = {0, 0, 0},
        .up       = {0, 1, 0},
        .fovy     = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Change cube color
        if (IsKeyDown(KEY_C)) {
            hueCube = Wrap(hueCube + 45.0f * delta, 0, 360);
            material.emission.color = ColorFromHSV(hueCube, 1.0f, 1.0f);
        }

        // Adjust bloom parameters
        float intensity = R3D_ENVIRONMENT_GET(bloom.intensity);
        int intensityDir = IsKeyDownDelay(KEY_RIGHT) - IsKeyDownDelay(KEY_LEFT);
        AdjustBloomParam(&intensity, intensityDir, 0.01f, 0.0f, INFINITY);
        R3D_ENVIRONMENT_SET(bloom.intensity, intensity);

        float radius = R3D_ENVIRONMENT_GET(bloom.filterRadius);
        int radiusDir = IsKeyDownDelay(KEY_UP) - IsKeyDownDelay(KEY_DOWN);
        AdjustBloomParam(&radius, radiusDir, 0.1f, 0.0f, INFINITY);
        R3D_ENVIRONMENT_SET(bloom.filterRadius, radius);

        int levelDir = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) - IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        float levels = R3D_ENVIRONMENT_GET(bloom.levels);
        AdjustBloomParam(&levels, levelDir, 0.01f, 0.0f, 1.0f);
        R3D_ENVIRONMENT_SET(bloom.levels, levels);

        // Draw scene
        if (IsKeyPressed(KEY_SPACE)) {
            R3D_ENVIRONMENT_SET(bloom.mode, (R3D_ENVIRONMENT_GET(bloom.mode) + 1) % (R3D_BLOOM_SCREEN + 1));
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);

            R3D_Begin(camera);
                R3D_DrawMesh(cube, material, Vector3Zero(), 1.0f);
            R3D_End();

            // Draw bloom info
            DrawTextRight(TextFormat("Mode: %s", GetBloomModeName()), 10, 20, LIME);
            DrawTextRight(TextFormat("Intensity: %.2f", R3D_ENVIRONMENT_GET(bloom.intensity)), 40, 20, LIME);
            DrawTextRight(TextFormat("Filter Radius: %.2f", R3D_ENVIRONMENT_GET(bloom.filterRadius)), 70, 20, LIME);
            DrawTextRight(TextFormat("Levels: %.2f", R3D_ENVIRONMENT_GET(bloom.levels)), 100, 20, LIME);

        EndDrawing();
    }

    R3D_UnloadMesh(cube);
    R3D_Close();

    CloseWindow();

    return 0;
}

bool IsKeyDownDelay(int key)
{
    return IsKeyPressedRepeat(key) || IsKeyPressed(key);
}

const char* GetBloomModeName(void)
{
    const char* modes[] = {"Disabled", "Mix", "Additive", "Screen"};
    int mode = R3D_ENVIRONMENT_GET(bloom.mode);
    return (mode >= 0 && mode <= R3D_BLOOM_SCREEN) ? modes[mode] : "Unknown";
}

void DrawTextRight(const char* text, int y, int fontSize, Color color)
{
    int width = MeasureText(text, fontSize);
    DrawText(text, GetScreenWidth() - width - 10, y, fontSize, color);
}

void AdjustBloomParam(float* param, int direction, float step, float min, float max)
{
    if (direction != 0)
        *param = Clamp(*param + direction * step, min, max);
}
