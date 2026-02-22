#include <r3d/r3d.h>
#include <raymath.h>

#ifndef RESOURCES_PATH
#	define RESOURCES_PATH "./"
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
    R3D_Model model = R3D_LoadModel(RESOURCES_PATH "models/Human.glb");

    // Load animations
    R3D_AnimationLib modelAnims = R3D_LoadAnimationLib(RESOURCES_PATH "models/Human.glb");
    R3D_AnimationPlayer modelPlayer = R3D_LoadAnimationPlayer(model.skeleton, modelAnims);

    // Create animation tree
    // ======== STM + TRAVEL EXAMPLE ========
    /*
    R3D_AnimationTree animTree = R3D_LoadAnimationTreeEx(modelPlayer, 9, 0);
    R3D_AnimationTreeNode* stmNode = R3D_CreateStmNodeEx(&animTree, 8, 10, true);

    R3D_AnimationState animState = {.speed = 0.5f,
                                    .play = true,
                                    .loop = true};
    R3D_AnimationTreeNode* animNode0 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "walk forward",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode1 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "sprint forward",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode2 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "walk left",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode3 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "sprint left",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode4 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "walk backward",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode5 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "sprint backward",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode6 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "walk right",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });
    R3D_AnimationTreeNode* animNode7 = R3D_CreateAnimationNode(&animTree,
                                                               (R3D_AnimationNodeParams){
                                                                   .name = "idle",
                                                                   .state = animState,
                                                                   .looper = true
                                                               });

    R3D_AnimationStmIndex stateIdx0 = R3D_CreateStmNodeState(stmNode, animNode0, 3);
    R3D_AnimationStmIndex stateIdx1 = R3D_CreateStmNodeState(stmNode, animNode1, 1);
    R3D_AnimationStmIndex stateIdx2 = R3D_CreateStmNodeState(stmNode, animNode2, 1);
    R3D_AnimationStmIndex stateIdx3 = R3D_CreateStmNodeState(stmNode, animNode3, 2);
    R3D_AnimationStmIndex stateIdx4 = R3D_CreateStmNodeState(stmNode, animNode4, 0);
    R3D_AnimationStmIndex stateIdx5 = R3D_CreateStmNodeState(stmNode, animNode5, 2);
    R3D_AnimationStmIndex stateIdx6 = R3D_CreateStmNodeState(stmNode, animNode6, 1);
    R3D_AnimationStmIndex stateIdx7 = R3D_CreateStmNodeState(stmNode, animNode7, 0);

    R3D_StmEdgeParams edgeParams = {.mode = R3D_STM_EDGE_ONDONE,
                                    .xFadeTime = 0.1};
    R3D_AnimationStmIndex edgeIdx0 = R3D_CreateStmNodeEdge(stmNode, stateIdx0, stateIdx3,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx1 = R3D_CreateStmNodeEdge(stmNode, stateIdx0, stateIdx1,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx2 = R3D_CreateStmNodeEdge(stmNode, stateIdx1, stateIdx2,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx3 = R3D_CreateStmNodeEdge(stmNode, stateIdx2, stateIdx0,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx4 = R3D_CreateStmNodeEdge(stmNode, stateIdx0, stateIdx0,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx5 = R3D_CreateStmNodeEdge(stmNode, stateIdx3, stateIdx4,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx6 = R3D_CreateStmNodeEdge(stmNode, stateIdx3, stateIdx5,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx7 = R3D_CreateStmNodeEdge(stmNode, stateIdx5, stateIdx6,
                                                           edgeParams);
    R3D_AnimationStmIndex edgeIdx8 = R3D_CreateStmNodeEdge(stmNode, stateIdx6, stateIdx7,
                                                           edgeParams);
    // R3D_AnimationStmIndex edgeIdx9 = R3D_CreateStmNodeEdge(stmNode, stateIdx5, stateIdx7,
    //                                                        edgeParams);

    R3D_TravelToStmState(stmNode, stateIdx7);
    R3D_AddRootAnimationNode(&animTree, stmNode);
    */

    // ======== SWITCH NODE EXAMPLE ========
    R3D_AnimationTree animTree = R3D_LoadAnimationTreeEx(modelPlayer, 6, 0);
    R3D_SwitchNodeParams switchParams = {.synced = true,
                                         .activeInput = 0,
                                         .xFadeTime = 0.4f};
    R3D_AnimationTreeNode* switchNode = R3D_CreateSwitchNode(&animTree, 5, switchParams);
    R3D_AnimationState     animParams = {.speed = 0.5f,
                                         .play = true,
                                         .loop = true};
    R3D_AnimationTreeNode* animNode;
    animNode = R3D_CreateAnimationNode(&animTree,
                                       (R3D_AnimationNodeParams){
                                           .name = "idle",
                                           .state = (R3D_AnimationState){
                                               .speed = 1.0f,
                                               .play = true,
                                               .loop = true
                                           }
                                       });
    R3D_AddAnimationNode(switchNode, animNode, 0);
    animNode = R3D_CreateAnimationNode(&animTree,
                                       (R3D_AnimationNodeParams){
                                           .name = "walk forward",
                                           .state = animParams
                                       });
    R3D_AddAnimationNode(switchNode, animNode, 1);
    animNode = R3D_CreateAnimationNode(&animTree,
                                       (R3D_AnimationNodeParams){
                                           .name = "run forward",
                                           .state = animParams
                                       });
    R3D_AddAnimationNode(switchNode, animNode, 2);
    animNode = R3D_CreateAnimationNode(&animTree,
                                       (R3D_AnimationNodeParams){
                                           .name = "walk backward",
                                           .state = animParams
                                       });
    R3D_AddAnimationNode(switchNode, animNode, 3);
    animNode = R3D_CreateAnimationNode(&animTree,
                                       (R3D_AnimationNodeParams){
                                           .name = "run backward",
                                           .state = animParams
                                       });
    R3D_AddAnimationNode(switchNode, animNode, 4);
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
        .target = {0, 0.75f, 0.0f},
        .up = {0, 1, 0},
        .fovy = 60
    };

    // Main loop
    while (!WindowShouldClose())
    {
        float delta = GetFrameTime();

        // ======== SWITCH NODE EXAMPLE ========
        if(IsKeyDown(KEY_ONE))
            switchParams.activeInput = 0;
        if(IsKeyDown(KEY_TWO))
            switchParams.activeInput = 1;
        if(IsKeyDown(KEY_THREE))
            switchParams.activeInput = 2;
        if(IsKeyDown(KEY_FOUR))
            switchParams.activeInput = 3;
        if(IsKeyDown(KEY_FIVE))
            switchParams.activeInput = 4;
        R3D_SetSwitchNodeParams(switchNode, switchParams);

        UpdateCamera(&camera, CAMERA_ORBITAL);
        R3D_UpdateAnimationTree(&animTree, delta);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            R3D_Begin(camera);
                R3D_DrawMesh(plane, R3D_MATERIAL_BASE, Vector3Zero(), 1.0f);
                R3D_DrawAnimatedModel(model, modelPlayer, Vector3Zero(), 1.0f);
            R3D_End();

            DrawText("Press '1' to idle", 10, GetScreenHeight()-114, 20, BLACK);
            DrawText("Press '2' to walk forward", 10, GetScreenHeight()-94, 20, BLACK);
            DrawText("Press '3' to run forward", 10, GetScreenHeight()-74, 20, BLACK);
            DrawText("Press '4' to walk backward", 10, GetScreenHeight()-54, 20, BLACK);
            DrawText("Press '5' to run backward", 10, GetScreenHeight()-34, 20, BLACK);
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
