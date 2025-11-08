#include "chess.h"
#include "chess_asset.cpp"
#include "chess_camera.cpp"
#include "chess_game_logic.cpp"

#define DEFAULT_FEN_STRING "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

#define COLOR_WHITE         Vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
#define COLOR_BLACK         Vec4{ 0.0f, 0.0f, 0.0f, 1.0f }
#define COLOR_BLUE          Vec4{ 0.67f, 0.84f, 0.90f, 1.0f }
#define COLOR_GREEN         Vec4{ 0.0f, 0.70f, 0.0f, 1.0f }
#define COLOR_MAGENTA       Vec4{ 1.0f, 0.0f, 1.0f, 0.1f }
#define COLOR_RED           Vec4{ 0.66f, 0.0f, 0.0f, 1.0f }
#define COLOR_RED_CRIMSON   Vec4{ 0.86f, 0.07f, 0.23f, 1.0f }
#define COLOR_RED_CADMIUM   Vec4{ 0.82f, 0.16f, 0.16f, 1.0f }
#define COLOR_RED_FIREBRICK Vec4{ 0.69f, 0.13f, 0.13f, 1.0f }
#define COLOR_RED_BURGUNDY  Vec4{ 0.50f, 0.0f, 0.12f, 1.0f }
#define COLOR_RED_BRIGHT    Vec4{ 0.96f, 0.24f, 0.16f, 1.0f }
#define COLOR_LIGHT_GRAY    Vec4{ 0.41f, 0.41f, 0.41f, 0.9f }

#define COLOR_CHECK     COLOR_RED
#define COLOR_CAPTURE   Vec4{ 0.66f, 0.66f, 0.0f, 1.0f }
#define COLOR_PROMOTION COLOR_BLUE
#define COLOR_ENPASSANT Vec4{ 0.98f, 0.49f, 0.11f, 1.0f }
#define COLOR_CASTLING  Vec4{ 0.68f, 0.64f, 0.92f, 0.8f }

#define UI_COLOR_TEXT         COLOR_WHITE
#define UI_COLOR_TEXT_HOVER   COLOR_BLACK
#define UI_COLOR_WIDGET_HOVER COLOR_LIGHT_GRAY
#define UI_COLOR_ICON         COLOR_LIGHT_GRAY
#define UI_COLOR_ICON_HOVER   COLOR_BLACK

chess_internal Mat4x4      GetGridCellModel(Mesh* meshes, u32 cellIndex, bool gridCellScale);
chess_internal void        DragSelectedPiece(GameMemory* memory, f32 x, f32 y);
chess_internal inline void BeginPieceDrag(GameMemory* memory, u32 cellIndex);
chess_internal inline void EndPieceDrag(GameMemory* memory);
chess_internal inline void CancelPieceDrag(GameMemory* memory);
chess_internal inline bool IsDragging(GameMemory* memory);
chess_internal u32         GetDraggingPieceTargetCell(GameMemory* memory);
chess_internal inline void PlaySound(GameMemory* memory, u32 soundIndex);
chess_internal bool        UIButton(GameMemory* memory, const char* text, Rect rect);
chess_internal bool        UIButton(GameMemory* memory, Rect rect, Rect textureRect);
chess_internal bool UISelector(GameMemory* memory, Rect rect, const char* label, const char** options, u32 optionCount,
                               u32* selectedOptionIndex);
chess_internal inline void SetCursorType(GameMemory* memory, u32 type);
chess_internal inline void RestartGame(GameMemory* memory);

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
                BoardMoveDo(&memory->board, move);
                platform.Log("Board fen: %s", memory->board.history.peek().c_str());

                switch (move->type)
                {
                case MOVE_TYPE_CHECK:
                {
                    PlaySound(memory, GAME_SOUND_CHECK);
                    break;
                }
                default:
                {
                    PlaySound(memory, GAME_SOUND_MOVE);
                    break;
                }
                }

                validMove = true;
                break;
            }
        }

        if (!validMove)
        {
            PlaySound(memory, GAME_SOUND_ILLEGAL);
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

chess_internal inline void PlaySound(GameMemory* memory, u32 soundIndex)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(soundIndex >= 0 && soundIndex < GAME_SOUND_COUNT);

    if (memory->soundEnabled)
    {
        PlatformAPI platform = memory->platform;
        Assets*     assets   = &memory->assets;

        platform.SoundPlay(&assets->sounds[soundIndex]);
    }
}

chess_internal bool UIButton(GameMemory* memory, const char* text, Rect rect)
{
    CHESS_ASSERT(memory);

    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    DrawAPI              draw               = memory->draw;

    bool pressed = false;

    f32 cursorX = (f32)keyboardController->cursorX;
    f32 cursorY = (f32)keyboardController->cursorY;

    Vec2 textSize   = draw.TextGetSize(text);
    f32  btnCenterX = rect.x + (rect.w / 2.0f);
    f32  btnCenterY = rect.y + (rect.h / 2.0f);
    f32  textX      = btnCenterX - (textSize.w / 2.0f);
    f32  textY      = btnCenterY + (textSize.h / 2.0f);

    if (PointInRect(rect, { cursorX, cursorY }))
    {
        draw.Rect(rect, UI_COLOR_WIDGET_HOVER);
        draw.Text(text, textX, textY, UI_COLOR_TEXT_HOVER);
        SetCursorType(memory, CURSOR_TYPE_FINGER);

        if (ButtonIsPressed(keyboardController->buttonAction))
        {
            pressed = true;
        }
    }
    else
    {
        draw.Text(text, textX, textY, UI_COLOR_TEXT);
    }

    return pressed;
}

chess_internal bool UIButton(GameMemory* memory, Rect rect, Rect textureRect)
{
    CHESS_ASSERT(memory);

    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    DrawAPI              draw               = memory->draw;
    Assets*              assets             = &memory->assets;
    Texture              textureAtlas       = assets->textures[TEXTURE_2D_ATLAS];

    bool pressed = false;

    f32 cursorX = (f32)keyboardController->cursorX;
    f32 cursorY = (f32)keyboardController->cursorY;

    f32 btnCenterX = rect.x + (rect.w / 2.0f);
    f32 btnCenterY = rect.y + (rect.h / 2.0f);

    Rect textureDst;
    textureDst.x = btnCenterX - textureRect.w / 2.0f;
    textureDst.y = btnCenterY - textureRect.h / 2.0f;
    textureDst.w = textureRect.w;
    textureDst.h = textureRect.h;

    Vec4 iconColor = UI_COLOR_TEXT;
    if (PointInRect(rect, { cursorX, cursorY }))
    {
        iconColor = UI_COLOR_TEXT_HOVER;
        SetCursorType(memory, CURSOR_TYPE_FINGER);
        draw.Rect(rect, UI_COLOR_WIDGET_HOVER);

        if (ButtonIsPressed(keyboardController->buttonAction))
        {
            pressed = true;
        }
    }

    draw.RectTexture(textureDst, textureRect, textureAtlas, iconColor);

    return pressed;
}

chess_internal bool UISelector(GameMemory* memory, Rect rect, const char* label, const char** options, u32 optionCount,
                               u32* selectedOptionIndex)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(options);
    CHESS_ASSERT(selectedOptionIndex);
    CHESS_ASSERT(*selectedOptionIndex <= optionCount - 1);

    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    DrawAPI              draw               = memory->draw;
    Assets*              assets             = &memory->assets;
    Texture              btnTexture         = assets->textures[TEXTURE_2D_ATLAS];

    bool pressed = false;

    bool actionBtnPressed = ButtonIsPressed(keyboardController->buttonAction);
    f32  cursorX          = (f32)keyboardController->cursorX;
    f32  cursorY          = (f32)keyboardController->cursorY;

    f32 iconBtnWidth  = textureArrowRight.w;
    f32 iconBtnHeight = textureArrowRight.h;
    CHESS_ASSERT(iconBtnWidth == textureArrowLeft.w);
    CHESS_ASSERT(iconBtnHeight == textureArrowLeft.h);

    Vec4 textColor = UI_COLOR_TEXT;
    Vec4 iconColor = UI_COLOR_ICON;

    // Selector
    if (PointInRect(rect, { cursorX, cursorY }))
    {
        draw.Rect(rect, UI_COLOR_WIDGET_HOVER);
        textColor = UI_COLOR_TEXT_HOVER;
        iconColor = UI_COLOR_ICON_HOVER;
    }

    // Label
    {
        Vec2 textSize        = draw.TextGetSize(label);
        f32  textAreaCenterY = rect.y + (rect.h / 2.0f);

        draw.Text(label, rect.x, textAreaCenterY + (textSize.h / 2.0f), textColor);
    }

    Rect leftArrowRect;
    leftArrowRect.x = rect.x + (rect.w / 2.0f);
    leftArrowRect.y = rect.y;
    leftArrowRect.w = iconBtnWidth;
    leftArrowRect.h = rect.h;

    Rect rightArrowRect;
    rightArrowRect.x = (rect.x + rect.w) - iconBtnWidth;
    rightArrowRect.y = rect.y;
    rightArrowRect.w = iconBtnWidth;
    rightArrowRect.h = rect.h;

    // Previous option
    if (*selectedOptionIndex > 0 && optionCount > 0)
    {
        if (PointInRect(leftArrowRect, { cursorX, cursorY }))
        {
            SetCursorType(memory, CURSOR_TYPE_FINGER);
            if (actionBtnPressed)
            {
                (*selectedOptionIndex)--;
                pressed = true;
            }
        }

        draw.RectTexture(leftArrowRect, textureArrowLeft, btnTexture, iconColor);
    }
    // Next option
    if (*selectedOptionIndex < optionCount - 1)
    {
        if (PointInRect(rightArrowRect, { cursorX, cursorY }))
        {
            SetCursorType(memory, CURSOR_TYPE_FINGER);
            if (actionBtnPressed)
            {
                (*selectedOptionIndex)++;
                pressed = true;
            }
        }

        draw.RectTexture(rightArrowRect, textureArrowRight, btnTexture, iconColor);
    }

    // Text area covers space between right and left icons
    Rect textArea;
    textArea.x = leftArrowRect.x + iconBtnWidth;
    textArea.y = rect.y;
    textArea.w = rect.w / 2.0f - (iconBtnWidth * 2.0f);
    textArea.h = rect.h;

    const char* option = options[*selectedOptionIndex];

    Vec2 textSize        = draw.TextGetSize(option);
    f32  textAreaCenterX = textArea.x + (textArea.w / 2.0f);
    f32  textAreaCenterY = textArea.y + (textArea.h / 2.0f);
    f32  textX           = textAreaCenterX - (textSize.w / 2.0f);
    f32  textY           = textAreaCenterY + (textSize.h / 2.0f);

    draw.Text(option, textX, textY, textColor);

    return pressed;
}

chess_internal inline void SetCursorType(GameMemory* memory, u32 type)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(type >= 0 && type < CURSOR_TYPE_COUNT);

    constexpr Rect rects[3] = {
        textureCursorPointer, // Pointer
        textureCursorFinger,  // Finger
        textureCursorPick,    // Pick
    };

    memory->cursorTexture = rects[type];
}

chess_internal inline void RestartGame(GameMemory* memory)
{
    CHESS_ASSERT(memory);

    memory->board     = BoardCreate(DEFAULT_FEN_STRING);
    memory->gameState = GAME_STATE_PLAY;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    PlatformAPI platform = memory->platform;
    Assets*     assets   = &memory->assets;
    Camera3D*   camera3D = &memory->camera3D;
    Camera2D*   camera2D = &memory->camera2D;
    DrawAPI     draw     = memory->draw;

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
        memory->isInitialized   = true;
        memory->soundEnabled    = true;
        memory->showPiecesMoves = true;
        memory->gameState       = GAME_STATE_MENU;
        memory->gameStarted     = false;
        SetCursorType(memory, CURSOR_TYPE_POINTER);

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

        memory->board = BoardCreate(DEFAULT_FEN_STRING);
    }
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Update
    Vec2U windowDimension = platform.WindowGetDimension();
    Camera3DUpdateProjection(camera3D, windowDimension.w, windowDimension.h);
    Camera2DUpdateProjection(camera2D, windowDimension.w, windowDimension.h);

    SetCursorType(memory, CURSOR_TYPE_POINTER);

    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    GameInputController* gamepadController0 = &memory->input.controllers[GAME_INPUT_CONTROLLER_GAMEPAD_0];

    GameInputController* playerController;
    Rect                 playerCursor;
    Vec4                 playerColor;
    u32                  turnColor   = BoardGetTurn(&memory->board);
    u32                  boardResult = BoardGetGameResult(&memory->board);

    // Get current player controller
    // White player uses mouse/keyboard
    // Black player uses gamepad if available, otherwise mouse/keyboard
    // When game is over white player has the control
    if (turnColor == PIECE_COLOR_WHITE || boardResult != BOARD_GAME_RESULT_NONE)
    {
        playerController = keyboardController;
        playerColor      = COLOR_BLUE;
    }
    else if (turnColor == PIECE_COLOR_BLACK)
    {
        playerColor = COLOR_RED;
        if (gamepadController0->isEnabled)
        {
            playerController = gamepadController0;
        }
        else
        {
            playerController = keyboardController;
        }
    }
    else
    {
        // TODO
        CHESS_ASSERT(0);
    }
    playerCursor = { (f32)playerController->cursorX, (f32)playerController->cursorY, 32, 32 };

    if (IsDragging(memory) && ButtonIsDown(playerController->buttonCancel))
    {
        CancelPieceDrag(memory);
    }
    if (IsDragging(memory) && ButtonIsUp(playerController->buttonAction))
    {
        EndPieceDrag(memory);
    }

    u32 cellIndex = draw.GetObjectAtPixel(playerController->cursorX, windowDimension.h - playerController->cursorY - 1);
    if (cellIndex >= 0 && cellIndex <= 64)
    {
        Piece targetPiece = BoardGetPiece(&memory->board, cellIndex);
        if (targetPiece.color == turnColor)
        {
            SetCursorType(memory, CURSOR_TYPE_FINGER);

            if (!IsDragging(memory) && ButtonIsDown(playerController->buttonAction))
            {
                BeginPieceDrag(memory, cellIndex);
            }
        }
    }
    if (IsDragging(memory))
    {
        SetCursorType(memory, CURSOR_TYPE_PICKING);
        DragSelectedPiece(memory, playerController->cursorX, playerController->cursorY);
    }

    if (memory->gameState == GAME_STATE_PLAY && !memory->gameStarted)
    {
        memory->gameStarted = true;
    }
    if (ButtonIsPressed(playerController->buttonStart) && turnColor == PIECE_COLOR_WHITE)
    {
        // Switch gameplay to menu
        if (memory->gameState == GAME_STATE_PLAY)
        {
            memory->gameState = GAME_STATE_MENU;
        }
        else if (memory->gameState == GAME_STATE_MENU)
        {
            // Switch menu to gameplay
            if (memory->gameStarted)
            {
                memory->gameState = GAME_STATE_PLAY;
            }
        }
        // Switch settings to menu
        else if (memory->gameState == GAME_STATE_SETTINGS)
        {
            memory->gameState = GAME_STATE_MENU;
        }
    }

    if (boardResult != BOARD_GAME_RESULT_NONE && memory->gameState != GAME_STATE_END)
    {
        platform.Log("Game has terminated");
        memory->gameState = GAME_STATE_END;
    }
    //  ---------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Draw
    draw.Begin(windowDimension.w, windowDimension.h);
    {

#if CHESS_BUILD_DEBUG
        draw.Begin2D(camera2D);

        char frameTimeBuffer[20];
        sprintf(frameTimeBuffer, "Frame time %.2fms", 1000.0f * delta);
        draw.Text(frameTimeBuffer, 0, 30, COLOR_WHITE);

        draw.End2D();
#endif

        switch (memory->gameState)
        {
        case GAME_STATE_MENU:
        {
            draw.Begin2D(camera2D);

            u32  btnCount    = 3;
            bool gameStarted = BoardMoveCanUndo(&memory->board);
            if (gameStarted)
            {
                btnCount++;
            }

            f32 w      = 150;
            f32 h      = 50;
            f32 x      = (windowDimension.w / 2.0f) - (w / 2.0f);
            f32 y      = (windowDimension.h / 2.0f) - ((h / 2.0f) * (f32)btnCount);
            f32 margin = 20.0f;

            Rect btnRect{ x, y, w, h };

            if (!gameStarted)
            {
                if (UIButton(memory, "New game", btnRect))
                {
                    memory->gameState = GAME_STATE_PLAY;
                }
            }
            else
            {
                if (UIButton(memory, "Continue", btnRect))
                {
                    memory->gameState = GAME_STATE_PLAY;
                }

                btnRect.y += btnRect.h + margin;
                if (UIButton(memory, "Restart", btnRect))
                {
                    RestartGame(memory);
                }
            }

            btnRect.y += btnRect.h + margin;
            if (UIButton(memory, "Settings", btnRect))
            {
                memory->gameState = GAME_STATE_SETTINGS;
            }

            btnRect.y += btnRect.h + margin;
            if (UIButton(memory, "Exit", btnRect))
            {
                return false;
            }

            draw.RectTexture(playerCursor, memory->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);

            draw.End2D();
            break;
        }
        case GAME_STATE_SETTINGS:
        {
            draw.Begin2D(camera2D);

            f32 width  = windowDimension.w * 0.75f;
            f32 height = windowDimension.h * 0.20f;
            f32 x      = (windowDimension.w - width) / 2.0f;
            f32 y      = (windowDimension.h - height) / 2.0f;

            const char* settingsLabel = "GAME SETTINGS";
            draw.Text(settingsLabel, x, y - 15.0f, COLOR_WHITE);

            draw.Rect({ x, y, width, height }, Vec4{ 0.0f, 0.0f, 0.0f, 0.7f });

            f32  margin    = 20.0f;
            f32  selectorX = x + margin;
            f32  selectorY = y + margin;
            f32  selectorW = width - (margin * 2.0f);
            f32  selectorH = 35.0f;
            Rect selectorRect{ selectorX, selectorY, selectorW, selectorH };

            // Resolution
            {
                static const char* options[4]     = { "1280x720", "1366x768", "1920x1080", "Fullscreen" };
                static const Vec2U resolutions[3] = { { 1280, 720 }, { 1366, 768 }, { 1920, 1080 } };
                static u32         selectedOption = 2;
                if (UISelector(memory, selectorRect, "Resolution", options, ARRAY_COUNT(options), &selectedOption))
                {
                    if (selectedOption == 3)
                    {
                        platform.WindowSetFullscreen();
                    }
                    else
                    {
                        Vec2U dimension = resolutions[selectedOption];
                        platform.WindowResize(dimension);
                    }
                }
            }

            // Show pieces moves
            //          {
            //              selectorRect.y += selectorH + margin;

            //              static const char* options[2]     = { "Off", "On" };
            //              static u32         selectedOption = 1;
            //              if (UISelector(memory, selectorRect, "Show pieces moves", options, ARRAY_COUNT(options),
            //                             &selectedOption))
            //              {
            //                  if (strcmp(options[selectedOption], "On") == 0)
            //                  {
            //                      memory->showPiecesMoves = true;
            //                  }
            //                  else
            //                  {
            //                      memory->showPiecesMoves = false;
            //                  }
            //              }
            //          }

            // Sound
            {
                selectorRect.y += selectorH + margin;

                static const char* options[2]     = { "Off", "On" };
                static u32         selectedOption = 1;
                if (UISelector(memory, selectorRect, "Sound", options, ARRAY_COUNT(options), &selectedOption))
                {
                    if (strcmp(options[selectedOption], "On") == 0)
                    {
                        memory->soundEnabled = true;
                    }
                    else
                    {
                        memory->soundEnabled = false;
                    }
                }
            }

            draw.RectTexture(playerCursor, memory->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);

            draw.End2D();
            break;
        }
        case GAME_STATE_PLAY:
        {
            // 2D
            {
                draw.Begin2D(camera2D);

                f32  margin = 20.0f;
                Rect btnRect;
                btnRect.w = 84.0f;
                btnRect.h = 84.0f;
                btnRect.x = margin;
                btnRect.y = (windowDimension.h - btnRect.h) - margin;

                if (BoardMoveCanUndo(&memory->board))
                {
                    if (UIButton(memory, btnRect, textureArrowUndo))
                    {
                        BoardMoveUndo(&memory->board);
                    }
                }

                draw.RectTexture(playerCursor, memory->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);
                draw.End2D();
            }

            // 3D
            {
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
							Vec4 color = ((row + col) % 2 == 0) ? COLOR_BLACK : COLOR_WHITE;
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
                                    // if (memory->showPiecesMoves)
                                    //{
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
                                            else if (move->type == MOVE_TYPE_ENPASSANT)
                                            {
                                                color = COLOR_ENPASSANT;
                                            }
                                            else if (move->type == MOVE_TYPE_CASTLING)
                                            {
                                                color = COLOR_CASTLING;
                                            }

                                            Mat4x4 cellToPosition = GetGridCellModel(assets->meshes, move->to, true);
                                            draw.Plane3D(cellToPosition, color);
                                        }
                                    }
                                    FreePieceMoveList(movelist);
                                    //}
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
            break;
        }
        case GAME_STATE_END:
        {
            draw.Begin2D(camera2D);
            {
                char textResultBuffer[30];
                Vec4 textColor;

                if (boardResult == BOARD_GAME_RESULT_WIN)
                {
                    sprintf(textResultBuffer, "%s", "White wins");
                    textColor = COLOR_BLUE;
                }
                else if (boardResult == BOARD_GAME_RESULT_LOSE)
                {
                    sprintf(textResultBuffer, "%s", "Black wins");
                    textColor = COLOR_RED_BRIGHT;
                }
                else
                {
                    sprintf(textResultBuffer, "%s", "Draw");
                    textColor = UI_COLOR_TEXT_HOVER;
                }

                Vec2 textSize = draw.TextGetSize(textResultBuffer);

                f32 margin = 20.0f;
                f32 w      = 200.0f;
                f32 h      = 75.0f;

                {
                    f32 x = margin;
                    f32 y = margin;
                    draw.Rect({ x, y, w, h }, UI_COLOR_WIDGET_HOVER);

                    Vec2 textSize   = draw.TextGetSize(textResultBuffer);
                    f32  btnCenterX = x + (w / 2.0f);
                    f32  btnCenterY = y + (h / 2.0f);
                    f32  textX      = btnCenterX - (textSize.w / 2.0f);
                    f32  textY      = btnCenterY + (textSize.h / 2.0f);

                    draw.Text(textResultBuffer, textX, textY, textColor);
                }

                {
                    f32 x = (windowDimension.w - w) - margin;
                    f32 y = margin;

                    Rect btnRect{ x, y, w, h };

                    if (UIButton(memory, "New game", { x, y, w, h }))
                    {
                        RestartGame(memory);
                    }

                    btnRect.y += btnRect.h + margin;
                    if (UIButton(memory, "Exit", btnRect))
                    {
                        return false;
                    }
                }

                draw.RectTexture(playerCursor, memory->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);
            }
            draw.End2D();

            draw.Begin3D(camera3D);
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, assets->textures[TEXTURE_BOARD_ALBEDO].id);

                Mesh*  boardMesh = &assets->meshes[MESH_BOARD];
                Mat4x4 model     = MeshComputeModelMatrix(assets->meshes, MESH_BOARD);
                draw.Mesh(boardMesh, model, -1, COLOR_WHITE);

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
            draw.End();

            break;
        }
        }
    }

    draw.End();

    return true;
    // ----------------------------------------------------------------------------
}