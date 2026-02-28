#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#   define RESOURCES_PATH "./"
#endif

int main(void)
{
    // Initialize window
    InitWindow(800, 450, "[r3d] - Animation tree example");
    SetTargetFPS(60);

    // Initialize R3D with FXAA
    R3D_Init(GetScreenWidth(), GetScreenHeight());
    R3D_SetAntiAliasingMode(R3D_ANTI_ALIASING_MODE_FXAA);

    // Setup environment sky
    R3D_Cubemap cubemap = R3D_LoadCubemap(RESOURCES_PATH "panorama/indoor.hdr", R3D_CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.skyBlur, 0.3f);
    R3D_ENVIRONMENT_SET(background.energy, 0.6f);
    R3D_ENVIRONMENT_SET(background.sky, cubemap);

    // Setup environment ambient
    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(cubemap, R3D_AMBIENT_ILLUMINATION);
    R3D_ENVIRONMENT_SET(ambient.map, ambientMap);
    R3D_ENVIRONMENT_SET(ambient.energy, 0.25f);

    // Setup tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_FILMIC);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.75f);

    // Generate a ground plane and load the animated model
    R3D_Mesh plane = R3D_GenMeshPlane(10, 10, 1, 1);
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "models/YBot.glb");

    // Load animations
    R3D_AnimationLib modelAnims = R3D_LoadAnimationLib(RESOURCES_PATH "models/YBot.glb");
    R3D_AnimationPlayer modelPlayer = R3D_LoadAnimationPlayer(model.skeleton, modelAnims);

    // Create & define animation tree structure
    R3D_AnimationTree animTree = R3D_LoadAnimationTreeEx(modelPlayer, 12, 0);

    R3D_AnimationState animState = {
        .speed = 0.8f,
        .play  = true,
        .loop  = true
    };
    R3D_StmEdgeParams edgeParams = {
        .mode      = R3D_STM_EDGE_ONDONE,
        .status    = R3D_STM_EDGE_AUTO,
        .xFadeTime = 0.0f
    };
    R3D_StmEdgeParams fadedEdgeParams = {
        .mode      = R3D_STM_EDGE_ONDONE,
        .status    = R3D_STM_EDGE_AUTO,
        .xFadeTime = 0.3f
    };
    R3D_AnimationNodeParams loopingAnimParams = {
        .state  = animState,
        .looper = true
    };

    R3D_AnimationTreeNode* leftRightStmNode = R3D_CreateStmNode(&animTree, 4, 4);
    {
        TextCopy(loopingAnimParams.name, "walk left");
        R3D_AnimationTreeNode* animNode0 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);
        R3D_AnimationTreeNode* animNode1 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);

        TextCopy(loopingAnimParams.name, "walk right");
        R3D_AnimationTreeNode* animNode2 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);
        R3D_AnimationTreeNode* animNode3 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);

        R3D_AnimationStmIndex stateIdx0 = R3D_CreateStmNodeState(leftRightStmNode, animNode0, 1);
        R3D_AnimationStmIndex stateIdx1 = R3D_CreateStmNodeState(leftRightStmNode, animNode1, 1);
        R3D_AnimationStmIndex stateIdx2 = R3D_CreateStmNodeState(leftRightStmNode, animNode2, 1);
        R3D_AnimationStmIndex stateIdx3 = R3D_CreateStmNodeState(leftRightStmNode, animNode3, 1);
        R3D_CreateStmNodeEdge(leftRightStmNode, stateIdx0, stateIdx1, edgeParams);
        R3D_CreateStmNodeEdge(leftRightStmNode, stateIdx1, stateIdx2, fadedEdgeParams);
        R3D_CreateStmNodeEdge(leftRightStmNode, stateIdx2, stateIdx3, edgeParams);
        R3D_CreateStmNodeEdge(leftRightStmNode, stateIdx3, stateIdx0, fadedEdgeParams);
    }

    R3D_AnimationTreeNode* forwBackStmNode = R3D_CreateStmNode(&animTree, 4, 4);
    {
        TextCopy(loopingAnimParams.name, "walk forward");
        R3D_AnimationTreeNode* animNode0 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);
        R3D_AnimationTreeNode* animNode1 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);

        TextCopy(loopingAnimParams.name, "walk backward");
        R3D_AnimationTreeNode* animNode2 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);
        R3D_AnimationTreeNode* animNode3 = R3D_CreateAnimationNode(&animTree, loopingAnimParams);

        R3D_AnimationStmIndex stateIdx0 = R3D_CreateStmNodeState(forwBackStmNode, animNode0, 1);
        R3D_AnimationStmIndex stateIdx1 = R3D_CreateStmNodeState(forwBackStmNode, animNode1, 1);
        R3D_AnimationStmIndex stateIdx2 = R3D_CreateStmNodeState(forwBackStmNode, animNode2, 1);
        R3D_AnimationStmIndex stateIdx3 = R3D_CreateStmNodeState(forwBackStmNode, animNode3, 1);
        R3D_CreateStmNodeEdge(forwBackStmNode, stateIdx0, stateIdx1, edgeParams);
        R3D_CreateStmNodeEdge(forwBackStmNode, stateIdx1, stateIdx2, fadedEdgeParams);
        R3D_CreateStmNodeEdge(forwBackStmNode, stateIdx2, stateIdx3, edgeParams);
        R3D_CreateStmNodeEdge(forwBackStmNode, stateIdx3, stateIdx0, fadedEdgeParams);
    }

    R3D_SwitchNodeParams switchParams = {
        .synced      = false,
        .activeInput = 0,
        .xFadeTime   = 0.4f
    };
    R3D_AnimationTreeNode* switchNode = R3D_CreateSwitchNode(&animTree, 3, switchParams);
    R3D_AnimationTreeNode* idleNode = R3D_CreateAnimationNode(&animTree, (R3D_AnimationNodeParams){
        .name  = "idle",
        .state = animState
    });
    R3D_AddAnimationNode(switchNode, idleNode, 0);
    R3D_AddAnimationNode(switchNode, leftRightStmNode, 1);
    R3D_AddAnimationNode(switchNode, forwBackStmNode, 2);
    R3D_AddRootAnimationNode(&animTree, switchNode);

    // Setup lights with shadows
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1.0f, -1.0f, -1.0f});
    R3D_SetLightActive(light, true);
    R3D_SetLightRange(light, 10.0f);
    R3D_EnableShadow(light);

    // Setup camera
    Camera3D camera = {
        .position = {0, 1.5f, 3.0f},
        .target   = {0, 0.75f, 0.0f},
        .up       = {0, 1, 0},
        .fovy     = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        if (IsKeyDown(KEY_ONE))   switchParams.activeInput = 0;
        if (IsKeyDown(KEY_TWO))   switchParams.activeInput = 1;
        if (IsKeyDown(KEY_THREE)) switchParams.activeInput = 2;
        R3D_SetSwitchNodeParams(switchNode, switchParams);

        UpdateCamera(&camera, CAMERA_ORBITAL);
        R3D_UpdateAnimationTree(&animTree, delta);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            R3D_Begin(camera);
                R3D_DrawMesh(plane, R3D_MATERIAL_BASE, Vector3Zero(), 1.0f);
                R3D_DrawAnimatedModel(model, modelPlayer, Vector3Zero(), 1.0f);
            R3D_End();
            DrawText("Press '1' to idle",                       10, GetScreenHeight()-74, 20, BLACK);
            DrawText("Press '2' to walk left and right",        10, GetScreenHeight()-54, 20, BLACK);
            DrawText("Press '3' to walk forward and backward",  10, GetScreenHeight()-34, 20, BLACK);
        EndDrawing();
    }

    // Cleanup
    R3D_UnloadAnimationTree(animTree);
    R3D_UnloadAnimationPlayer(modelPlayer);
    R3D_UnloadAnimationLib(modelAnims);
    R3D_UnloadModel(model, true);
    R3D_UnloadMesh(plane);
    R3D_Close();

    CloseWindow();

    return 0;
}
