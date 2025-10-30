// TODO: Free gpu resources

#include "chess.h"
#include "chess_asset.cpp"
#include "chess_camera.cpp"
#include "chess_game_logic.cpp"

#define DEFAULT_FEN_STRING "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define COLOR_WHITE   Vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
#define COLOR_BLACK   Vec4{ 0.0f, 0.0f, 0.0f, 1.0f }
#define COLOR_YELLOW  Vec4{ 0.66f, 0.66f, 0.0f, 1.0f }
#define COLOR_BLUE    Vec4{ 0.67f, 0.84f, 0.90f, 1.0f }
#define COLOR_GREEN   Vec4{ 0.0f, 0.70f, 0.0f, 1.0f }
#define COLOR_MAGENTA Vec4{ 1.0f, 0.0f, 1.0f, 0.1f }
#define COLOR_RED     Vec4{ 0.66f, 0.0f, 0.0f, 1.0f }

#define COLOR_CHECK     COLOR_RED
#define COLOR_CAPTURE   COLOR_YELLOW
#define COLOR_PROMOTION COLOR_BLUE

chess_internal inline bool ButtonIsPressed(GameButtonState gameButtonState);
chess_internal Mat4x4      GetGridCellModel(Mesh* meshes, u32 cellIndex, bool gridCellScale);
chess_internal void        DragSelectedPiece(GameMemory* memory, f32 x, f32 y);
chess_internal inline void BeginPieceDrag(GameMemory* memory, u32 cellIndex);
chess_internal inline void EndPieceDrag(GameMemory* memory);
chess_internal inline void CancelPieceDrag(GameMemory* memory);
chess_internal inline bool IsDragging(GameMemory* memory);
chess_internal u32         GetDraggingPieceTargetCell(GameMemory* memory);

chess_internal inline bool ButtonIsPressed(GameButtonState gameButtonState)
{
    return gameButtonState.isDown && !gameButtonState.wasDown;
}

chess_internal Mat4x4 GetGridCellModel(Mesh* meshes, u32 cellIndex, bool gridCellScale = false)
{
    CHESS_ASSERT(meshes);
    VALIDATE_CELL_INDEX(cellIndex);

    u32 row = CELL_ROW(cellIndex);
    u32 col = CELL_COL(cellIndex);

    Mat4x4 gridModel    = MeshComputeModelMatrix(meshes, MESH_BOARD);
    Vec3   gridScale    = meshes[MESH_BOARD_GRID_SURFACE].scale;
    Vec3   gridPosition = meshes[MESH_BOARD_GRID_SURFACE].translate;
    f32    cellWidth    = gridScale.x / 8.0f;
    f32    cellDepth    = gridScale.z / 8.0f;
    f32    cellXOrigin  = (-gridScale.x + cellWidth) + col * (cellWidth * 2.0f);
    f32    cellZOrigin  = (gridScale.z - cellDepth) - row * (cellDepth * 2.0f);

    Mat4x4 cellModel = Identity();
    cellModel        = Translate(cellModel, { cellXOrigin, gridPosition.y, cellZOrigin });
    if (gridCellScale)
    {
        cellModel = Scale(cellModel, { cellWidth, 1.0f, cellDepth });
    }

    return gridModel * cellModel;
}

chess_internal void DragSelectedPiece(GameMemory* memory, f32 x, f32 y)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(memory->pieceDragState.isDragging);

    Vec2U dimension = memory->platform.WindowGetDimension();
    u32   width     = dimension.w;
    u32   height    = dimension.h;

    // Inverse transformation pipeline: convert from viewport to world coordinates
    // Get a direction vector from cursor pointer (x, y) and use it to cast a ray from the camera
    Mat4x4 projection        = memory->camera3D.projection;
    Mat4x4 view              = memory->camera3D.view;
    Mat4x4 inverseProjection = Inverse(projection);
    Mat4x4 inverseView       = Inverse(view);
    // Viewport to NDC
    Vec3 rayNdc{};
    rayNdc.x = (2.0f * x) / width - 1.0f;
    rayNdc.y = 1.0f - (2.0f * y) / height;
    // NDC to Clip
    Vec4 rayClip{ rayNdc.x, rayNdc.y, -1.0f, 1.0f };
    // Clip to View
    Vec4 rayView = inverseProjection * rayClip;
    rayView.z    = -1.0f;
    rayView.w    = 0;
    // View -> World
    Vec4 rayWorld4 = inverseView * rayView;
    Vec3 rayWorld{ rayWorld4.x, rayWorld4.y, rayWorld4.z };

    // Calculate piece position using ray-plane intersection
    // Ray vs Plane intersection
    // t = -(O · n + δ) / (D · n)
    // t = -(origin * plane normal + plane offset) / (ray direction * plane normal)
    Vec3 rayBegin    = memory->camera3D.position;
    Vec3 rayEnd      = { 0, -1, 0 };
    Vec3 boardNormal = { 0, 1, 0 }; // Board facing up

    f32 t = -((Dot(rayBegin, boardNormal) / Dot(rayWorld, boardNormal)));
    if (t >= 0)
    {
        Vec3 newDragPiecePosition = rayBegin + (rayWorld * t);
        Vec3 gridPosition         = memory->assets.meshes[MESH_BOARD_GRID_SURFACE].translate;
        Vec3 gridScale            = memory->assets.meshes[MESH_BOARD_GRID_SURFACE].scale;

        // Grid bounds
        if (newDragPiecePosition.x > gridScale.x)
        {
            newDragPiecePosition.x = gridScale.x;
        }
        if (newDragPiecePosition.x < -gridScale.x)
        {
            newDragPiecePosition.x = -gridScale.x;
        }
        if (newDragPiecePosition.z > gridScale.z)
        {
            newDragPiecePosition.z = gridScale.z;
        }
        if (newDragPiecePosition.z < -gridScale.z)
        {
            newDragPiecePosition.z = -gridScale.z;
        }

        memory->pieceDragState.worldPosition = { newDragPiecePosition.x, gridPosition.y, newDragPiecePosition.z };
    }
}

chess_internal inline void BeginPieceDrag(GameMemory* memory, u32 cellIndex)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(!IsDragging(memory));
    VALIDATE_CELL_INDEX(cellIndex);

    Mat4x4 cellModel = GetGridCellModel(memory->assets.meshes, cellIndex);

    memory->pieceDragState.piece         = BoardGetPiece(&memory->board, cellIndex);
    memory->pieceDragState.isDragging    = true;
    memory->pieceDragState.worldPosition = Vec3{ cellModel.e[3][0], cellModel.e[3][1], cellModel.e[3][2] };
}

chess_internal inline void EndPieceDrag(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(IsDragging(memory));

    PlatformAPI platform = memory->platform;
    Assets*     assets   = &memory->assets;

    u32 targetCell = GetDraggingPieceTargetCell(memory);
    u32 fromCell   = memory->pieceDragState.piece.cellIndex;

    u32   moveCount;
    Move* movelist = BoardGetPieceMoveList(&memory->board, fromCell, &moveCount);
    if (moveCount > 0)
    {
        bool validMove = false;
        for (u32 moveIndex = 0; moveIndex < moveCount; moveIndex++)
        {
            Move* move = &movelist[moveIndex];
            if (move->to == targetCell)
            {
                BoardDoMove(&memory->board, move);
                platform.Log("Board fen: %s", memory->board.fen);

                switch (move->type)
                {
                case MOVE_TYPE_CHECK:
                {
                    platform.SoundPlay(&assets->sounds[GAME_SOUND_CHECK]);
                    break;
                }
                default:
                {
                    platform.SoundPlay(&assets->sounds[GAME_SOUND_MOVE]);
                    break;
                }
                }

                validMove = true;
            }
        }

        if (!validMove)
        {
            platform.SoundPlay(&assets->sounds[GAME_SOUND_ILLEGAL]);
        }
    }
    FreePieceMoveList(movelist);

    memory->pieceDragState.isDragging    = false;
    memory->pieceDragState.worldPosition = Vec3{ -1.0f };
}

chess_internal inline void CancelPieceDrag(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(IsDragging(memory));

    memory->pieceDragState.isDragging    = false;
    memory->pieceDragState.worldPosition = Vec3{ -1.0f };
}

chess_internal inline bool IsDragging(GameMemory* memory)
{
    CHESS_ASSERT(memory);

    return memory->pieceDragState.isDragging;
}

#pragma warning(push)
#pragma warning(disable : 4715)
chess_internal u32 GetDraggingPieceTargetCell(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(IsDragging(memory));

    Vec3 draggingPiecePosition = memory->pieceDragState.worldPosition;

    Vec3 gridScale = memory->assets.meshes[MESH_BOARD_GRID_SURFACE].scale;
    f32  cellWidth = (gridScale.x / 8.0f) * 2.0f;
    f32  cellDepth = (gridScale.z / 8.0f) * 2.0f;

    u32 targetCellIndex;
    for (u32 row = 0; row < 8; row++)
    {
        f32 rowMinZ = gridScale.z - (cellDepth * (f32)row);
        f32 rowMaxZ = rowMinZ - cellDepth;

        if (draggingPiecePosition.z <= rowMinZ && draggingPiecePosition.z >= rowMaxZ)
        {
            for (u32 col = 0; col < 8; col++)
            {
                f32 colMinX = -gridScale.x + (cellWidth * f32(col));
                f32 colMaxX = colMinX + cellWidth;

                if (draggingPiecePosition.x >= colMinX && draggingPiecePosition.x <= colMaxX)
                {
                    return CELL_INDEX(row, col);
                }
            }
        }
    }

    CHESS_ASSERT(0);
}
#pragma warning(pop)

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    PlatformAPI          platform           = memory->platform;
    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    GameInputController* gamepadController0 = &memory->input.controllers[GAME_INPUT_CONTROLLER_GAMEPAD_0];
    Assets*              assets             = &memory->assets;
    Camera3D*            camera3D           = &memory->camera3D;
    Camera2D*            camera2D           = &memory->camera2D;
    DrawAPI              draw               = memory->draw;

    PFNGLBINDTEXTUREPROC             glBindTexture             = memory->opengl.glBindTexture;
    PFNGLCREATEPROGRAMPROC           glCreateProgram           = memory->opengl.glCreateProgram;
    PFNGLCREATESHADERPROC            glCreateShader            = memory->opengl.glCreateShader;
    PFNGLATTACHSHADERPROC            glAttachShader            = memory->opengl.glAttachShader;
    PFNGLDELETESHADERPROC            glDeleteShader            = memory->opengl.glDeleteShader;
    PFNGLLINKPROGRAMPROC             glLinkProgram             = memory->opengl.glLinkProgram;
    PFNGLDELETEPROGRAMPROC           glDeleteProgram           = memory->opengl.glDeleteProgram;
    PFNGLSHADERSOURCEPROC            glShaderSource            = memory->opengl.glShaderSource;
    PFNGLUSEPROGRAMPROC              glUseProgram              = memory->opengl.glUseProgram;
    PFNGLGETSHADERIVPROC             glGetShaderiv             = memory->opengl.glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog        = memory->opengl.glGetShaderInfoLog;
    PFNGLCOMPILESHADERPROC           glCompileShader           = memory->opengl.glCompileShader;
    PFNGLGETPROGRAMIVPROC            glGetProgramiv            = memory->opengl.glGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog       = memory->opengl.glGetProgramInfoLog;
    PFNGLGENBUFFERSPROC              glGenBuffers              = memory->opengl.glGenBuffers;
    PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays         = memory->opengl.glGenVertexArrays;
    PFNGLBINDBUFFERPROC              glBindBuffer              = memory->opengl.glBindBuffer;
    PFNGLBINDVERTEXARRAYPROC         glBindVertexArray         = memory->opengl.glBindVertexArray;
    PFNGLBUFFERDATAPROC              glBufferData              = memory->opengl.glBufferData;
    PFNGLBUFFERSUBDATAPROC           glBufferSubData           = memory->opengl.glBufferSubData;
    PFNGLDELETEBUFFERSPROC           glDeleteBuffers           = memory->opengl.glDeleteBuffers;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = memory->opengl.glEnableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer     = memory->opengl.glVertexAttribPointer;
    PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays      = memory->opengl.glDeleteVertexArrays;
    PFNGLACTIVETEXTUREPROC           glActiveTexture           = memory->opengl.glActiveTexture;
    PFNGLGENERATEMIPMAPPROC          glGenerateMipmap          = memory->opengl.glGenerateMipmap;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation      = memory->opengl.glGetUniformLocation;
    PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv        = memory->opengl.glUniformMatrix4fv;
    PFNGLUNIFORM1IPROC               glUniform1i               = memory->opengl.glUniform1i;
    PFNGLDEBUGMESSAGECALLBACKPROC    glDebugMessageCallback    = memory->opengl.glDebugMessageCallback;
    PFNGLDEBUGMESSAGECONTROLPROC     glDebugMessageControl     = memory->opengl.glDebugMessageControl;
    PFNGLDRAWELEMENTSPROC            glDrawElements            = memory->opengl.glDrawElements;
    PFNGLDRAWARRAYSPROC              glDrawArrays              = memory->opengl.glDrawArrays;
    PFNGLCLEARPROC                   glClear                   = memory->opengl.glClear;
    PFNGLENABLEPROC                  glEnable                  = memory->opengl.glEnable;

    // ----------------------------------------------------------------------------
    // Init
    if (!memory->isInitialized)
    {
        memory->isInitialized = true;

        LoadGameAssets(memory);

        *camera3D = Camera3DInit({ 0, 0.53f, 0.43f },     // Position
                                 { 0.0f, -0.81f, -1.0f }, // Target
                                 { -0.0f, 0.59f, -1.0f }, // Up
                                 -54.0f,                  // Pitch
                                 -90.0f,                  // Yaw
                                 45.0f,                   // Fov
                                 5.0f                     // Distance
        );

        Vec2U windowDimension = platform.WindowGetDimension();
        *camera2D             = Camera2DInit(windowDimension.w, windowDimension.h);

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

        memory->program = memory->opengl.ProgramBuild(vertexSource, fragmentSource);
        glEnable(GL_DEPTH_TEST);

        memory->board = BoardCreate(DEFAULT_FEN_STRING);
    }
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Update
    Vec2U windowDimension = platform.WindowGetDimension();
    Camera3DUpdateProjection(camera3D, windowDimension.w, windowDimension.h);
    Camera2DUpdateProjection(camera2D, windowDimension.w, windowDimension.h);

    if (keyboardController->buttonCancel.isDown && IsDragging(memory))
    {
        CancelPieceDrag(memory);
    }
    if (!keyboardController->buttonAction.isDown && IsDragging(memory))
    {
        EndPieceDrag(memory);
    }

    u32 cellIndex =
        draw.GetObjectAtPixel(keyboardController->cursorX, windowDimension.h - keyboardController->cursorY - 1);
    if (cellIndex >= 0 && cellIndex <= 64)
    {
        // TODO: Check piece color, game turn (white or black) and player input
        if (keyboardController->buttonAction.isDown)
        {
            if (!IsDragging(memory))
            {
                BeginPieceDrag(memory, cellIndex);
            }
        }
    }
    if (IsDragging(memory))
    {
        DragSelectedPiece(memory, keyboardController->cursorX, keyboardController->cursorY);
    }
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Draw
    draw.Begin(windowDimension.w, windowDimension.h);
    {
        draw.Begin2D(camera2D);
        {
#if CHESS_BUILD_DEBUG
            char frameTimeBuffer[20];
            sprintf(frameTimeBuffer, "Frame time %.2fms", 1000.0f * delta);
            draw.Text(frameTimeBuffer, 0, 30, COLOR_WHITE);
#endif

            Rect keyboardCursor{ (f32)keyboardController->cursorX, (f32)keyboardController->cursorY, 32, 32 };
            draw.RectTexture(keyboardCursor, { 64, 64, 39, 62 }, assets->textures[TEXTURE_2D_ATLAS], COLOR_BLUE);

            Rect gamepadCursor{ (f32)gamepadController0->cursorX, (f32)gamepadController0->cursorY, 32, 32 };
            draw.RectTexture(gamepadCursor, { 64, 64, 39, 62 }, assets->textures[TEXTURE_2D_ATLAS], COLOR_RED);
        }
        draw.End2D();

        draw.Begin3D(camera3D);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, assets->textures[TEXTURE_BOARD_ALBEDO].id);

        Mesh*  boardMesh = &assets->meshes[MESH_BOARD];
        Mat4x4 model     = MeshComputeModelMatrix(assets->meshes, MESH_BOARD);
        draw.Mesh(boardMesh, model, -1, COLOR_WHITE);

        draw.BeginMousePicking();
        {
            for (u32 row = 0; row < 8; row++)
            {
                for (u32 col = 0; col < 8; col++)
                {
                    u32    cellIndex = CELL_INDEX(row, col);
                    Piece  piece     = BoardGetPiece(&memory->board, cellIndex);
                    Mat4x4 cellModel = GetGridCellModel(assets->meshes, cellIndex);

                    if (piece.type != PIECE_TYPE_NONE)
                    {
                        Mesh* pieceMesh    = &assets->meshes[piece.meshIndex];
                        u32   pieceTexture = assets->textures[piece.textureIndex].id;
                        glBindTexture(GL_TEXTURE_2D, pieceTexture);
                        CHESS_ASSERT(pieceMesh);
                        draw.Mesh(pieceMesh, cellModel, cellIndex, COLOR_WHITE);
                    }
                }
            }
        }
        draw.EndMousePicking();

        if (IsDragging(memory))
        {
            u32    targetCell = GetDraggingPieceTargetCell(memory);
            Mat4x4 cellModel  = GetGridCellModel(assets->meshes, targetCell, true);
            draw.Plane3D(cellModel, COLOR_MAGENTA);
        }

        for (u32 row = 0; row < 8; row++)
        {
            for (u32 col = 0; col < 8; col++)
            {
                u32    cellIndex = CELL_INDEX(row, col);
                Piece  piece     = BoardGetPiece(&memory->board, cellIndex);
                Mat4x4 cellModel = GetGridCellModel(assets->meshes, cellIndex);

#if 0
				// Debug draw grid
                {
                    Mat4x4 cellModel = GetGridCellModel(assets->meshes, cellIndex, true);
                    Vec4   color     = ((row + col) % 2 == 0) ? COLOR_BLACK : COLOR_WHITE;
                    draw.Plane3D(cellModel, color);
                }
#endif

                if (piece.type != PIECE_TYPE_NONE)
                {
                    Mesh* pieceMesh    = &assets->meshes[piece.meshIndex];
                    u32   pieceTexture = assets->textures[piece.textureIndex].id;
                    glBindTexture(GL_TEXTURE_2D, pieceTexture);
                    CHESS_ASSERT(pieceMesh);

                    Vec4 tintColor = COLOR_WHITE;

                    u32 dragIndex = memory->pieceDragState.piece.cellIndex;
                    if (IsDragging(memory) && cellIndex == dragIndex)
                    {
                        // Draw dragging piece
                        {
                            Piece draggingPiece = memory->pieceDragState.piece;

                            Mat4x4 model       = Identity();
                            model              = Translate(model, memory->pieceDragState.worldPosition);
                            Mesh* pieceMesh    = &assets->meshes[draggingPiece.meshIndex];
                            u32   pieceTexture = assets->textures[draggingPiece.textureIndex].id;
                            glBindTexture(GL_TEXTURE_2D, pieceTexture);
                            CHESS_ASSERT(pieceMesh);
                            draw.Mesh(pieceMesh, model, -1, COLOR_WHITE);
                        }

                        // Draw origin cell
                        {
                            Mat4x4 cellModel = GetGridCellModel(assets->meshes, dragIndex, true);
                            draw.Plane3D(cellModel, COLOR_MAGENTA);
                        }

                        // Draw posible movements
                        {
                            u32   moveCount;
                            Move* movelist = BoardGetPieceMoveList(&memory->board, dragIndex, &moveCount);
                            if (moveCount > 0)
                            {
                                tintColor = COLOR_MAGENTA;

                                for (u32 i = 0; i < moveCount; i++)
                                {
                                    Move* move = &movelist[i];
                                    u32   row  = move->to / 8;
                                    u32   col  = move->to % 8;

                                    Vec4 color = COLOR_GREEN;
                                    if (move->type == MOVE_TYPE_CAPTURE)
                                    {
                                        color = COLOR_CAPTURE;
                                    }
                                    else if (move->type == MOVE_TYPE_CHECK)
                                    {
                                        color = COLOR_CHECK;
                                    }
                                    else if (move->type == MOVE_TYPE_PROMOTION)
                                    {
                                        color = COLOR_PROMOTION;
                                    }

                                    Mat4x4 cellToPosition = GetGridCellModel(assets->meshes, move->to, true);
                                    draw.Plane3D(cellToPosition, color);
                                }
                            }
                            FreePieceMoveList(movelist);
                        }
                    }
                    else
                    {
                        draw.Mesh(pieceMesh, cellModel, cellIndex, tintColor);
                    }
                }
            }
        }

        draw.End3D();
    }

    draw.End();
    // ----------------------------------------------------------------------------
}