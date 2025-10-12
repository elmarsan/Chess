#include "chess.h"
#include "chess_asset.cpp"
#include "chess_camera.cpp"
#include "chess_game_logic.h"

// TODO: Free gpu resources
// inline bool ButtonIsPressed(GameButtonState gameButtonState)
//{
//    return gameButtonState.isDown && !gameButtonState.wasDown;
//}

chess_internal Mat4x4 GetGridCellPosition(Mesh* meshes, u32 row, u32 col)
{
    CHESS_ASSERT(meshes);

    Mat4x4 gridModel    = MeshComputeModelMatrix(meshes, MESH_BOARD);
    Vec3   gridScale    = meshes[MESH_BOARD_GRID_SURFACE].scale;
    Vec3   gridPosition = meshes[MESH_BOARD_GRID_SURFACE].translate;
    f32    cellWidth    = gridScale.x / 8.0f;
    f32    cellDepth    = gridScale.z / 8.0f;
    f32    cellXOrigin  = (-gridScale.x + cellWidth) + col * (cellWidth * 2.0f);
    f32    cellZOrigin  = (gridScale.z - cellDepth) - row * (cellDepth * 2.0f);

    Mat4x4 cellModel = Identity();
    cellModel        = Translate(cellModel, { cellXOrigin, gridPosition.y, cellZOrigin });

    return gridModel * cellModel;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    PlatformAPI          platform           = memory->platform;
    GfxAPI               g                  = memory->gfx;
    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    GameInputController* gamepadController0 = &memory->input.controllers[GAME_INPUT_CONTROLLER_GAMEPAD_0];
    Assets*              assets             = &memory->assets;
    Camera*              camera             = &memory->camera;
    DrawAPI              draw               = memory->draw;

    // Init
    if (!memory->isInitialized)
    {
        memory->isInitialized = true;

        LoadGameAssets(memory);

        *camera = CameraInit({ 0, 0.35f, 0.66f },   // Position
                             { 0, -0.40f, -0.90f }, // Target
                             { 0, 0.90f, -0.40f },  // Up
                             -24.0f,                // Pitch
                             -90.0f,                // Yaw
                             45.0f,                 // Fov
                             5.0f                   // Distance
        );

        const char* vertexSource   = R"(
			#version 330
			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec2 aUV;
			layout(location = 2) in vec3 aNormal;
			
			out vec2 UV;
			
			uniform mat4 model;
			uniform mat4 view;
			uniform mat4 projection;
			void main()
			{
				UV = aUV;
				gl_Position = projection * view * model * vec4(aPos, 1.0);
			}
			)";
        const char* fragmentSource = R"(
			#version 330
			out vec4 FragColor;
			in vec2 UV;
			uniform sampler2D albedo;
			uniform sampler2D normal;

			void main()
			{
				FragColor = texture(albedo, UV);
			}
			)";

        GfxPipelineDesc desc = {};
        desc.vertexShader    = vertexSource;
        desc.FragmentShader  = fragmentSource;
        memory->pipeline     = g.GfxPipelineCreate(&desc);

        g.GfxEnableDepthTest();
    }

    // Update
#if 0
	platform.Log("%.4f fps", 1.0f / delta);
#endif

    Mesh* boardMesh = &assets->meshes[MESH_BOARD];
    if (keyboardController->buttonAction.isDown && !keyboardController->buttonAction.wasDown)
    {
        boardMesh->translate.x += 0.1f;
    }
    if (keyboardController->buttonCancel.isDown && !keyboardController->buttonCancel.wasDown)
    {
        boardMesh->translate.x -= 0.1f;
    }
    //  if (ButtonIsPressed(keyboardController->buttonStart))
    //  {
    //  }
    //  if (gamepadController0->isEnabled)
    //  {
    //      if (gamepadController0->buttonAction.isDown)
    //      {
    //      }
    //      if (gamepadController0->buttonCancel.isDown)
    //      {
    //      }
    //      if (ButtonIsPressed(gamepadController0->buttonStart))
    //      {
    //          platform.SoundPlay(&assets->sounds[GAME_SOUND_CHECK]);
    //      }
    //  }

    // Draw
    g.GfxClear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);

    Vec2U windowDimension = platform.WindowGetDimension();
    CameraUpdateProjection(camera, windowDimension.w, windowDimension.h);

    f32 up = 0.02f; // TODO: Fix depth testing

    Vec3 gridScale = assets->meshes[MESH_BOARD_GRID_SURFACE].scale;
    f32  cellWidth = gridScale.x / 8.0f;
    f32  cellDepth = gridScale.z / 8.0f;
    Vec2 cellSize{ cellWidth, cellDepth };

    Vec4 white{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vec4 black{ 0.0f, 0.0f, 0.0f, 1.0f };
    Vec4 yellow{ 1.0f, 1.0f, 0.0f, 1.0f };
    Vec4 blue{ 0.0f, 0.0f, 1.0f, 1.0f };

    //  draw.Begin();
    //  {
    //      draw.Begin3D(camera);

    //      for (u32 row = 0; row < 8; row++)
    //      {
    //          for (u32 col = 0; col < 8; col++)
    //          {
    //              Vec4 color = ((row + col) % 2 == 0) ? black : white;

    //              Mat4x4 gridModel = GetGridCellPosition(assets->meshes, row, col);
    //              draw.Plane3DV2(gridModel, color);
    //          }
    //      }

    //      draw.End3D();
    //  }
    //  draw.End();

    g.GfxPipelineBind(memory->pipeline);

    // Draw board
    {
        g.GfxTextureBind(assets->textures[TEXTURE_BOARD_ALBEDO], 0);
        g.GfxUniformSetInt("albedo", 0);

        u32 meshIndex = MESH_BOARD;

        Mat4x4 model = MeshComputeModelMatrix(assets->meshes, meshIndex);

        g.GfxUniformSetMat4("model", &model.e[0][0]);
        g.GfxUniformSetMat4("projection", &camera->projection.e[0][0]);
        g.GfxUniformSetMat4("view", &camera->view.e[0][0]);

        g.GfxVertexArrayBind(assets->meshes[meshIndex].vao);
        g.GfxDrawIndexed(assets->meshes[meshIndex].indicesCount);
    }

    // Draw pieces
    {
        g.GfxTextureBind(assets->textures[TEXTURE_WHITE_ALBEDO], 0);
        g.GfxUniformSetInt("albedo", 0);

        for (u32 row = 0; row < 8; row++)
        {
            for (u32 col = 0; col < 8; col++)
            {
                Vec4 color = ((row + col) % 2 == 0) ? black : white;

                Mat4x4 gridModel = GetGridCellPosition(assets->meshes, row, col);

                u32 meshIndex = MESH_QUEEN;

                g.GfxUniformSetMat4("model", &gridModel.e[0][0]);
                g.GfxUniformSetMat4("projection", &camera->projection.e[0][0]);
                g.GfxUniformSetMat4("view", &camera->view.e[0][0]);

                g.GfxVertexArrayBind(assets->meshes[meshIndex].vao);
                g.GfxDrawIndexed(assets->meshes[meshIndex].indicesCount);
            }
        }
    }
}