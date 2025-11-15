#include "chess.h"
#include "chess_asset.cpp"
#include "chess_camera.cpp"
#include "chess_game_logic.cpp"

#define COLOR_WHITE        Vec4{ 1.0f, 1.0f, 1.0f, 1.0f }
#define COLOR_BLACK        Vec4{ 0.0f, 0.0f, 0.0f, 1.0f }
#define COLOR_BLUE         Vec4{ 0.67f, 0.84f, 0.90f, 1.0f }
#define COLOR_RED          Vec4{ 0.66f, 0.0f, 0.0f, 1.0f }
#define COLOR_RED_BRIGHT   Vec4{ 0.96f, 0.24f, 0.16f, 1.0f }
#define COLOR_GRAY         Vec4{ 0.41f, 0.41f, 0.41f, 0.9f }
#define COLOR_PURPLE       Vec4{ 0.6980f, 0.4f, 1.0f, 1.0f }
#define COLOR_PURPLE_LIGHT Vec4{ 0.7686f, 0.53725f, 1.0f, 0.5882f }
#define COLOR_MAGENTA      Vec4{ 1.0f, 0.1176f, 0.5568f, 1.0f }

#define UI_COLOR_TEXT         COLOR_WHITE
#define UI_COLOR_TEXT_HOVER   COLOR_BLACK
#define UI_COLOR_WIDGET_HOVER COLOR_GRAY
#define UI_COLOR_ICON         COLOR_GRAY
#define UI_COLOR_ICON_HOVER   COLOR_BLACK

chess_internal Mat4x4 GetPieceModel(Mesh* meshes, u32 cellIndex);
chess_internal void   DrawBoardCell(GameMemory* memory, u32 cellIndex, Vec4 color, const Rect* textureRect, Vec3 scale);
chess_internal void   DragSelectedPiece(GameMemory* memory, f32 x, f32 y);
chess_internal void   BeginPieceDrag(GameMemory* memory, u32 cellIndex);
chess_internal void   EndPieceDrag(GameMemory* memory);
chess_internal void   CancelPieceDrag(GameMemory* memory);
chess_internal bool   IsDragging(GameMemory* memory);
chess_internal u32    GetDraggingPieceTargetCell(GameMemory* memory);
chess_internal void   PlaySound(GameMemory* memory, u32 soundIndex);
chess_internal bool   UIButton(GameMemory* memory, const char* text, Rect rect);
chess_internal bool   UIButton(GameMemory* memory, Rect rect, Rect textureRect);
chess_internal bool UISelector(GameMemory* memory, Rect rect, const char* label, const char** options, u32 optionCount,
                               u32* selectedOptionIndex);
chess_internal void SetCursorType(GameMemory* memory, u32 type);
chess_internal void RestartGame(GameMemory* memory);
chess_internal GameInputController* GetPlayerController(GameMemory* memory);

chess_internal Mat4x4 GetPieceModel(Mesh* meshes, u32 cellIndex)
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

    return gridModel * cellModel;
}

chess_internal void DrawBoardCell(GameMemory* memory, u32 cellIndex, Vec4 color, const Rect* textureRect = nullptr,
                                  Vec3 scale = { 1.0f })
{
    CHESS_ASSERT(memory);
    VALIDATE_CELL_INDEX(cellIndex);

    GameState* state  = (GameState*)memory->permanentStorage;
    Mesh*      meshes = state->assets.meshes;
    DrawAPI    draw   = memory->draw;

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
    cellModel        = Scale(cellModel, Vec3{ cellWidth, 1.0f, cellDepth } * scale);

    Mat4x4 model = gridModel * cellModel;

    if (textureRect)
    {
        draw.PlaneTexture3D(cellModel, color, state->assets.textures[TEXTURE_2D_ATLAS], *textureRect);
    }
    else
    {
        draw.Plane3D(cellModel, color);
    }
}

chess_internal void DragSelectedPiece(GameMemory* memory, f32 x, f32 y)
{
    CHESS_ASSERT(memory);

    GameState* state  = (GameState*)memory->permanentStorage;
    Assets*    assets = &state->assets;
    CHESS_ASSERT(state->pieceDragState.isDragging);

    Vec2U dimension = memory->platform.WindowGetDimension();
    u32   width     = dimension.w;
    u32   height    = dimension.h;

    // Inverse transformation pipeline: convert from viewport to world coordinates
    // Get a direction vector from cursor pointer (x, y) and use it to cast a ray from the camera
    Mat4x4 projection        = state->camera3D.projection;
    Mat4x4 view              = state->camera3D.view;
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
    Vec3 rayBegin    = state->camera3D.position;
    Vec3 rayEnd      = { 0, -1, 0 };
    Vec3 boardNormal = { 0, 1, 0 }; // Board facing up

    f32 t = -((Dot(rayBegin, boardNormal) / Dot(rayWorld, boardNormal)));
    if (t >= 0)
    {
        Vec3 newDragPiecePosition = rayBegin + (rayWorld * t);
        // Vec3 gridPosition         = memory->assets.meshes[MESH_BOARD_GRID_SURFACE].translate;
        // Vec3 gridScale            = memory->assets.meshes[MESH_BOARD_GRID_SURFACE].scale;
        Vec3 gridPosition = assets->meshes[MESH_BOARD_GRID_SURFACE].translate;
        Vec3 gridScale    = assets->meshes[MESH_BOARD_GRID_SURFACE].scale;

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

        state->pieceDragState.worldPosition = { newDragPiecePosition.x, gridPosition.y, newDragPiecePosition.z };
    }
}

chess_internal inline void BeginPieceDrag(GameMemory* memory, u32 cellIndex)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(!IsDragging(memory));
    VALIDATE_CELL_INDEX(cellIndex);

    GameState* state      = (GameState*)memory->permanentStorage;
    Mat4x4     pieceModel = GetPieceModel(state->assets.meshes, cellIndex);

    state->pieceDragState.piece         = BoardGetPiece(&state->board, cellIndex);
    state->pieceDragState.isDragging    = true;
    state->pieceDragState.worldPosition = Vec3{ pieceModel.e[3][0], pieceModel.e[3][1], pieceModel.e[3][2] };
}

chess_internal inline void EndPieceDrag(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(IsDragging(memory));

    PlatformAPI platform = memory->platform;
    GameState*  state    = (GameState*)memory->permanentStorage;
    Assets*     assets   = &state->assets;
    Board*      board    = &state->board;

    u32 targetCell = GetDraggingPieceTargetCell(memory);
    u32 fromCell   = state->pieceDragState.piece.cellIndex;

    u32   moveCount;
    Move* movelist = BoardGetPieceMoveList(board, fromCell, &moveCount);
    if (moveCount > 0)
    {
        bool validMove = false;
        for (u32 moveIndex = 0; moveIndex < moveCount; moveIndex++)
        {
            Move* move = &movelist[moveIndex];
            if (move->to == targetCell)
            {
                BoardMoveDo(board, move);
                platform.Log("Board fen: %s", board->deltas.peek().fen.c_str());

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

    state->pieceDragState.isDragging    = false;
    state->pieceDragState.worldPosition = Vec3{ -1.0f };
}

chess_internal inline void CancelPieceDrag(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(IsDragging(memory));

    GameState* state = (GameState*)memory->permanentStorage;

    state->pieceDragState.isDragging    = false;
    state->pieceDragState.worldPosition = Vec3{ -1.0f };
}

chess_internal inline bool IsDragging(GameMemory* memory)
{
    CHESS_ASSERT(memory);

    GameState* state = (GameState*)memory->permanentStorage;
    return state->pieceDragState.isDragging;
}

chess_internal u32 GetDraggingPieceTargetCell(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(IsDragging(memory));

    GameState* state = (GameState*)memory->permanentStorage;

    Vec3 draggingPiecePosition = state->pieceDragState.worldPosition;

    Vec3 gridScale = state->assets.meshes[MESH_BOARD_GRID_SURFACE].scale;
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
    return 0;
}

chess_internal inline void PlaySound(GameMemory* memory, u32 soundIndex)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(soundIndex >= 0 && soundIndex < GAME_SOUND_COUNT);

    GameState* state = (GameState*)memory->permanentStorage;

    if (state->soundEnabled)
    {
        PlatformAPI platform = memory->platform;
        Assets*     assets   = &state->assets;

        platform.SoundPlay(&assets->sounds[soundIndex]);
    }
}

chess_internal bool UIButton(GameMemory* memory, const char* text, Rect rect)
{
    CHESS_ASSERT(memory);

    GameInputController* controller = GetPlayerController(memory);
    DrawAPI              draw       = memory->draw;

    bool pressed = false;

    f32 cursorX = (f32)controller->cursorX;
    f32 cursorY = (f32)controller->cursorY;

    Vec2 textSize   = draw.TextGetSize(text);
    f32  btnCenterX = rect.x + (rect.w / 2.0f);
    f32  btnCenterY = rect.y + (rect.h / 2.0f);
    f32  textX      = btnCenterX - (textSize.w / 2.0f);
    f32  textY      = btnCenterY + (textSize.h / 2.0f);
    textX           = (f32)(int)textX;
    textY           = (f32)(int)textY;

    if (PointInRect(rect, { cursorX, cursorY }))
    {
        draw.Rect(rect, UI_COLOR_WIDGET_HOVER);
        draw.Text(text, textX, textY, UI_COLOR_TEXT_HOVER);
        SetCursorType(memory, CURSOR_TYPE_FINGER);

        if (ButtonIsPressed(controller->buttonAction))
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

    GameState*           state        = (GameState*)memory->permanentStorage;
    GameInputController* controller   = GetPlayerController(memory);
    DrawAPI              draw         = memory->draw;
    Texture              textureAtlas = state->assets.textures[TEXTURE_2D_ATLAS];

    bool pressed = false;

    f32 cursorX = (f32)controller->cursorX;
    f32 cursorY = (f32)controller->cursorY;

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

        if (ButtonIsPressed(controller->buttonAction))
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

    GameState*           state      = (GameState*)memory->permanentStorage;
    GameInputController* controller = GetPlayerController(memory);
    DrawAPI              draw       = memory->draw;
    Texture              btnTexture = state->assets.textures[TEXTURE_2D_ATLAS];

    bool pressed = false;

    bool actionBtnPressed = ButtonIsPressed(controller->buttonAction);
    f32  cursorX          = (f32)controller->cursorX;
    f32  cursorY          = (f32)controller->cursorY;

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

        f32 x = (f32)(int)rect.x;
        f32 y = (f32)(int)(textAreaCenterY + (textSize.h / 2.0f));

        draw.Text(label, x, y, textColor);
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
    f32  textX           = (f32)(int)(textAreaCenterX - (textSize.w / 2.0f));
    f32  textY           = (f32)(int)(textAreaCenterY + (textSize.h / 2.0f));

    draw.Text(option, textX, textY, textColor);

    return pressed;
}

chess_internal inline void SetCursorType(GameMemory* memory, u32 type)
{
    CHESS_ASSERT(memory);
    CHESS_ASSERT(type >= 0 && type < CURSOR_TYPE_COUNT);

    GameState* state = (GameState*)memory->permanentStorage;

    constexpr Rect rects[3] = {
        textureCursorPointer, // Pointer
        textureCursorFinger,  // Finger
        textureCursorPick,    // Pick
    };

    state->cursorTexture = rects[type];
}

chess_internal inline void RestartGame(GameMemory* memory)
{
    CHESS_ASSERT(memory);

    GameState* state = (GameState*)memory->permanentStorage;
    state->board     = BoardCreate(DEFAULT_FEN_STRING);
    state->gameState = GAME_STATE_PLAY;
}

// Get current player controller
// White player uses mouse/keyboard
// Black player uses gamepad if available, otherwise mouse/keyboard
// When game is over white player has the control
chess_internal inline GameInputController* GetPlayerController(GameMemory* memory)
{
    CHESS_ASSERT(memory);

    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    GameInputController* gamepadController0 = &memory->input.controllers[GAME_INPUT_CONTROLLER_GAMEPAD_0];

    GameState* state = (GameState*)memory->permanentStorage;

    u32 turnColor   = BoardGetTurn(&state->board);
    u32 boardResult = BoardGetGameResult(&state->board);

    GameInputController* result;

    if (turnColor == PIECE_COLOR_WHITE || boardResult != BOARD_GAME_RESULT_NONE)
    {
        result = keyboardController;
    }
    else if (turnColor == PIECE_COLOR_BLACK)
    {
        if (gamepadController0->isEnabled)
        {
            result = gamepadController0;
        }
        else
        {
            result = keyboardController;
        }
    }

    CHESS_ASSERT(result);
    return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    GameState*  state    = (GameState*)memory->permanentStorage;
    PlatformAPI platform = memory->platform;
    DrawAPI     draw     = memory->draw;
    Assets*     assets   = &state->assets;
    Camera3D*   camera3D = &state->camera3D;
    Camera2D*   camera2D = &state->camera2D;
    Board*      board    = &state->board;

    // ----------------------------------------------------------------------------
    // Init
    if (!state->isInitialized)
    {
        state->isInitialized   = true;
        state->soundEnabled    = true;
        state->showPiecesMoves = true;
        state->gameState       = GAME_STATE_MENU;
        state->gameStarted     = false;

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

        state->board = BoardCreate(DEFAULT_FEN_STRING);
    }
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Update
    Vec2U windowDimension = platform.WindowGetDimension();
    Camera3DUpdateProjection(camera3D, windowDimension.w, windowDimension.h);
    Camera2DUpdateProjection(camera2D, windowDimension.w, windowDimension.h);

    SetCursorType(memory, CURSOR_TYPE_POINTER);

    GameInputController* playerController = GetPlayerController(memory);
    Rect                 playerCursor     = { (f32)playerController->cursorX, (f32)playerController->cursorY, 32, 32 };
    u32                  turnColor        = BoardGetTurn(board);
    Vec4                 playerColor      = turnColor == PIECE_COLOR_WHITE ? COLOR_BLUE : COLOR_RED;
    u32                  boardResult      = BoardGetGameResult(board);

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
        Piece targetPiece = BoardGetPiece(board, cellIndex);
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

    if (state->gameState == GAME_STATE_PLAY && !state->gameStarted)
    {
        state->gameStarted = true;
    }
    if (ButtonIsPressed(playerController->buttonStart) && turnColor == PIECE_COLOR_WHITE)
    {
        // Switch gameplay to menu
        if (state->gameState == GAME_STATE_PLAY)
        {
            state->gameState = GAME_STATE_MENU;
        }
        else if (state->gameState == GAME_STATE_MENU)
        {
            // Switch menu to gameplay
            if (state->gameStarted)
            {
                state->gameState = GAME_STATE_PLAY;
            }
        }
        // Switch settings to menu
        else if (state->gameState == GAME_STATE_SETTINGS)
        {
            state->gameState = GAME_STATE_MENU;
        }
    }

    if (boardResult != BOARD_GAME_RESULT_NONE && state->gameState != GAME_STATE_END)
    {
        platform.Log("Game has terminated");
        state->gameState = GAME_STATE_END;
    }
    //  ---------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Draw
    Material boardMaterial;
    boardMaterial.albedo    = assets->textures[TEXTURE_BOARD_ALBEDO];
    boardMaterial.normalMap = assets->textures[TEXTURE_BOARD_NORMAL];
    boardMaterial.specular  = Vec3{ 0.2f };
    boardMaterial.shininess = 64.0f;

    Material whiteMaterial;
    whiteMaterial.albedo    = assets->textures[TEXTURE_WHITE_ALBEDO];
    whiteMaterial.normalMap = assets->textures[TEXTURE_WHITE_NORMAL];
    whiteMaterial.specular  = Vec3{ 0.5f };
    whiteMaterial.shininess = 64.0f;

    Material blackMaterial;
    blackMaterial.albedo    = assets->textures[TEXTURE_BLACK_ALBEDO];
    blackMaterial.normalMap = assets->textures[TEXTURE_BLACK_NORMAL];
    blackMaterial.specular  = Vec3{ 0.35f };
    blackMaterial.shininess = 24.0f;

    draw.Begin(windowDimension.w, windowDimension.h);
    {

#if CHESS_BUILD_DEBUG
        draw.Begin2D(camera2D);

        char frameTimeBuffer[20];
        sprintf(frameTimeBuffer, "Frame time %.2fms", 1000.0f * delta);
        draw.Text(frameTimeBuffer, 0, 30, COLOR_WHITE);

        draw.End2D();
#endif

        switch (state->gameState)
        {
        case GAME_STATE_MENU:
        {
            draw.Begin2D(camera2D);

            u32  btnCount    = 3;
            bool gameStarted = BoardMoveCanUndo(board);
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
                    state->gameState = GAME_STATE_PLAY;
                }
            }
            else
            {
                if (UIButton(memory, "Continue", btnRect))
                {
                    state->gameState = GAME_STATE_PLAY;
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
                state->gameState = GAME_STATE_SETTINGS;
            }

            btnRect.y += btnRect.h + margin;
            if (UIButton(memory, "Exit", btnRect))
            {
                return false;
            }

            draw.RectTexture(playerCursor, state->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);

            draw.End2D();
            break;
        }
        case GAME_STATE_SETTINGS:
        {
            draw.Begin2D(camera2D);

            // TODO: Fix settings rect height
            f32 width  = windowDimension.w * 0.75f;
            f32 height = windowDimension.h * 0.30f;
            f32 x      = (windowDimension.w - width) / 2.0f;
            f32 y      = (windowDimension.h - height) / 2.0f;
            x          = (f32)(int)x;
            y          = (f32)(int)y;

            draw.Text("GAME SETTINGS", x, y - 15.0f, COLOR_WHITE);
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
            {
                selectorRect.y += selectorH + margin;

                static const char* options[2]     = { "Off", "On" };
                static u32         selectedOption = 1;
                if (UISelector(memory, selectorRect, "Show pieces moves", options, ARRAY_COUNT(options),
                               &selectedOption))
                {
                    if (strcmp(options[selectedOption], "On") == 0)
                    {
                        state->showPiecesMoves = true;
                    }
                    else
                    {
                        state->showPiecesMoves = false;
                    }
                }
            }

            // Sound
            {
                selectorRect.y += selectorH + margin;

                static const char* options[2]     = { "Off", "On" };
                static u32         selectedOption = 1;
                if (UISelector(memory, selectorRect, "Sound", options, ARRAY_COUNT(options), &selectedOption))
                {
                    if (strcmp(options[selectedOption], "On") == 0)
                    {
                        state->soundEnabled = true;
                    }
                    else
                    {
                        state->soundEnabled = false;
                    }
                }
            }

            draw.RectTexture(playerCursor, state->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);

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

                if (BoardMoveCanUndo(board))
                {
                    if (UIButton(memory, btnRect, textureArrowUndo))
                    {
                        BoardMoveUndo(board);
                    }
                }

                draw.RectTexture(playerCursor, state->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);
                draw.End2D();
            }

            // 3D
            {
                draw.Begin3D(camera3D);

                // Lightning
                {
                    Light directional;
                    directional.position = { -0.2f, -1.0f, -0.3f, 0.0f };
                    directional.ambient  = { 0.2f };
                    directional.diffuse  = { 0.5f };
                    directional.specular = { 1.0f, 1.0f, 1.0f };

                    Light point;
                    point.position = { 0, 3.0f, -2.2f, 1.0f };
                    point.ambient  = { 0.2f };
                    point.diffuse  = { 0.4f };
                    point.specular = { 0.8f };
                    point.constant = 1.0f;
                    point.linear   = 0.14f;

                    draw.LightAdd(directional);
                    draw.LightAdd(point);
                }

                Mesh*  boardMesh = &assets->meshes[MESH_BOARD];
                Mat4x4 model     = MeshComputeModelMatrix(assets->meshes, MESH_BOARD);
                draw.Mesh(boardMesh, model, -1, boardMaterial);

                draw.BeginMousePicking();
                {
                    for (u32 row = 0; row < 8; row++)
                    {
                        for (u32 col = 0; col < 8; col++)
                        {
                            u32    cellIndex  = CELL_INDEX(row, col);
                            Piece  piece      = BoardGetPiece(board, cellIndex);
                            Mat4x4 pieceModel = GetPieceModel(assets->meshes, cellIndex);

                            if (piece.type != PIECE_TYPE_NONE)
                            {
                                Mesh*    pieceMesh = &assets->meshes[piece.meshIndex];
                                Material material  = piece.color == PIECE_COLOR_WHITE ? whiteMaterial : blackMaterial;
                                draw.Mesh(pieceMesh, pieceModel, cellIndex, material);
                            }
                        }
                    }
                }
                draw.EndMousePicking();

                // Draw pieces
                {
                    for (u32 row = 0; row < 8; row++)
                    {
                        for (u32 col = 0; col < 8; col++)
                        {
                            u32    cellIndex  = CELL_INDEX(row, col);
                            Piece  piece      = BoardGetPiece(board, cellIndex);
                            Mat4x4 pieceModel = GetPieceModel(assets->meshes, cellIndex);

#if 0
							// Debug draw grid
							{
								Vec4 color = ((row + col) % 2 == 0) ? COLOR_BLACK : COLOR_WHITE;
								DrawBoardCell(memory, cellIndex, color);
							}
#endif

                            if (piece.type != PIECE_TYPE_NONE)
                            {
                                Mesh*    pieceMesh = &assets->meshes[piece.meshIndex];
                                Material material  = piece.color == PIECE_COLOR_WHITE ? whiteMaterial : blackMaterial;

                                u32 dragIndex = state->pieceDragState.piece.cellIndex;
                                if (IsDragging(memory) && cellIndex == dragIndex)
                                {
                                    // Draw dragging piece
                                    {
                                        Piece draggingPiece = state->pieceDragState.piece;

                                        Mat4x4 model    = Identity();
                                        model           = Translate(model, state->pieceDragState.worldPosition);
                                        Mesh* pieceMesh = &assets->meshes[draggingPiece.meshIndex];
                                        draw.Mesh(pieceMesh, model, -1, material);
                                    }
                                }
                                else
                                {
                                    draw.Mesh(pieceMesh, pieceModel, cellIndex, material);
                                }
                            }
                        }
                    }
                }

                {
                    if (state->showPiecesMoves)
                    {
                        // Draw check
                        if (BoardInCheck(board))
                        {
                            u32 kingCell = BoardGetKingCell(board);
                            DrawBoardCell(memory, kingCell, COLOR_RED);
                        }

                        // Draw last move
                        if (BoardGameStarted(board))
                        {
                            Move lastMove = BoardMoveGetLast(board);
                            DrawBoardCell(memory, lastMove.from, COLOR_PURPLE_LIGHT);
                            DrawBoardCell(memory, lastMove.to, COLOR_PURPLE_LIGHT);
                        }

                        if (IsDragging(memory))
                        {
                            // Draw origin cell
                            u32 origin = state->pieceDragState.piece.cellIndex;
                            DrawBoardCell(memory, origin, COLOR_PURPLE_LIGHT);

                            // Draw legal movements
                            u32   dragIndex = state->pieceDragState.piece.cellIndex;
                            u32   moveCount;
                            Move* movelist = BoardGetPieceMoveList(board, dragIndex, &moveCount);
                            if (moveCount > 0)
                            {
                                for (u32 i = 0; i < moveCount; i++)
                                {
                                    Move* move = &movelist[i];
                                    u32   row  = move->to / 8;
                                    u32   col  = move->to % 8;

                                    Piece targetPiece = BoardGetPiece(board, move->to);

                                    if (move->type == MOVE_TYPE_CAPTURE ||
                                        (move->type == MOVE_TYPE_CHECK && targetPiece.type != PIECE_TYPE_NONE))
                                    {
                                        DrawBoardCell(memory, move->to, COLOR_MAGENTA, &textureCircleOutlined,
                                                      Vec3{ 0.75f });
                                    }
                                    else
                                    {
                                        Vec3 scale{ 0.35f, 1.0f, 0.35f };
                                        if (GetDraggingPieceTargetCell(memory) == move->to)
                                        {
                                            scale = { 0.45f, 1.0f, 0.45f };
                                        }

                                        DrawBoardCell(memory, move->to, COLOR_PURPLE, &textureCircleOutlined, scale);
                                    }
                                }
                            }
                            FreePieceMoveList(movelist);
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

                draw.RectTexture(playerCursor, state->cursorTexture, assets->textures[TEXTURE_2D_ATLAS], playerColor);
            }
            draw.End2D();

            draw.Begin3D(camera3D);
            {
                Mesh*  boardMesh = &assets->meshes[MESH_BOARD];
                Mat4x4 model     = MeshComputeModelMatrix(assets->meshes, MESH_BOARD);
                draw.Mesh(boardMesh, model, -1, boardMaterial);

                for (u32 row = 0; row < 8; row++)
                {
                    for (u32 col = 0; col < 8; col++)
                    {
                        u32    cellIndex  = CELL_INDEX(row, col);
                        Piece  piece      = BoardGetPiece(board, cellIndex);
                        Mat4x4 pieceModel = GetPieceModel(assets->meshes, cellIndex);

                        if (piece.type != PIECE_TYPE_NONE)
                        {
                            Mesh*    pieceMesh = &assets->meshes[piece.meshIndex];
                            Material material  = piece.color == PIECE_COLOR_WHITE ? whiteMaterial : blackMaterial;
                            draw.Mesh(pieceMesh, pieceModel, cellIndex, material);
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