#include <r3d/r3d.h>
#include <raymath.h>

int main(void)
{
    InitWindow(800, 450, "[r3d] - Stencil Effects");
    SetTargetFPS(60);

    R3D_Init(GetScreenWidth(), GetScreenHeight());
    R3D_SetAntiAliasingMode(R3D_ANTI_ALIASING_MODE_SMAA);

    // Create the meshes used in the scene
    R3D_Mesh plane = R3D_GenMeshPlane(20, 20, 1, 1);
    R3D_Mesh box = R3D_GenMeshCube(1.5f, 2.0f, 0.3f);
    R3D_Mesh sphere = R3D_GenMeshSphere(0.5f, 32, 32);

    R3D_Material matGround = R3D_GetDefaultMaterial();
    matGround.albedo.color = (Color){160, 160, 160, 255};

    R3D_Material matWall = R3D_GetDefaultMaterial();
    matWall.albedo.color = (Color){120, 80, 60, 255};

    // Main X-Ray sphere material
    // The first pass draws the sphere normally and marks its visible pixels
    // in the stencil buffer with the value 0x01
    R3D_Material matXraySolid = R3D_GetDefaultMaterial();
    matXraySolid.albedo.color = (Color){80, 140, 220, 255};
    matXraySolid.stencil.mode = R3D_COMPARE_ALWAYS;
    matXraySolid.stencil.ref = 0x01;
    matXraySolid.stencil.opPass = R3D_STENCIL_REPLACE;

    // Ghost X-Ray sphere material
    // The second pass ignores depth so the sphere can be drawn through the wall,
    // but only where the first pass did not already mark the stencil buffer
    R3D_Material matXrayGhost = R3D_GetDefaultMaterial();
    matXrayGhost.albedo.color = (Color){80, 140, 220, 60};
    matXrayGhost.depth.mode = R3D_COMPARE_ALWAYS;
    matXrayGhost.stencil.mode = R3D_COMPARE_NOTEQUAL;
    matXrayGhost.stencil.ref = 0x01;
    matXrayGhost.transparencyMode = R3D_TRANSPARENCY_ALPHA;
    matXrayGhost.unlit = true;

    // Main outline sphere material
    // The first pass draws the red sphere and marks its silhouette
    // in the stencil buffer with the value 0x02
    R3D_Material matOutlineSolid = R3D_GetDefaultMaterial();
    matOutlineSolid.albedo.color = (Color){220, 100, 80, 255};
    matOutlineSolid.stencil.mode = R3D_COMPARE_ALWAYS;
    matOutlineSolid.stencil.ref = 0x02;
    matOutlineSolid.stencil.opPass = R3D_STENCIL_REPLACE;

    // Outline material
    // The second pass draws the same sphere slightly larger, only on pixels
    // outside the silhouette already marked by the first pass
    R3D_Material matOutlineRing = R3D_GetDefaultMaterial();
    matOutlineRing.albedo.color = (Color){255, 220, 0, 255};
    matOutlineRing.stencil.mode = R3D_COMPARE_NOTEQUAL;
    matOutlineRing.stencil.ref = 0x02;
    matOutlineRing.cullMode = R3D_CULL_FRONT;
    matOutlineRing.unlit = true;

    // Configure lighting, shadows, and ambient color
    R3D_ENVIRONMENT_SET(ambient.color, (Color){10, 10, 15, 255});

    R3D_Light light = R3D_CreateLight(R3D_LIGHT_SPOT);
    R3D_LightLookAt(light, (Vector3){4, 8, 5}, (Vector3){0});
    R3D_SetShadowSoftness(light, 8.0f);
    R3D_SetLightActive(light, true);
    R3D_EnableShadow(light);

    Camera3D camera = {
        .position = {0, 3, 5},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 55
    };

    while (!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(BLACK);

            R3D_Begin(camera);
                // Base scene geometry
                R3D_DrawMesh(plane, matGround, (Vector3){  0.0f, -0.5f,  0.0f }, 1.0f);
                R3D_DrawMesh(box,   matWall,   (Vector3){  0.0f,  0.5f,  0.0f }, 1.0f);

                // X-Ray sphere: visible solid pass, then transparent pass through the wall
                R3D_DrawMesh(sphere, matXraySolid, (Vector3){ 0.0f, 0.5f, -1.5f }, 1.0f);
                R3D_DrawMesh(sphere, matXrayGhost, (Vector3){ 0.0f, 0.5f, -1.5f }, 1.0f);

                // Outline sphere: normal object pass, then slightly enlarged outline pass
                R3D_DrawMesh(sphere, matOutlineSolid, (Vector3){ 2.2f, 0.2f, 0.8f }, 1.00f);
                R3D_DrawMesh(sphere, matOutlineRing,  (Vector3){ 2.2f, 0.2f, 0.8f }, 1.08f);
            R3D_End();
        EndDrawing();
    }

    R3D_UnloadMesh(sphere);
    R3D_UnloadMesh(box);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
