#pragma once

#include "chess_types.h"
#include "chess_camera.h"
#include "chess_draw_api.h"
#include "chess_platform.h"
#include "chess_asset.h"
#include "chess_math.h"
#include "chess_game_logic.h"

enum
{
    GAME_STATE_MENU,
    GAME_STATE_SETTINGS,
    GAME_STATE_PLAY,
    GAME_STATE_END
};

enum
{
    CURSOR_TYPE_POINTER,
    CURSOR_TYPE_FINGER,
    CURSOR_TYPE_PICKING,

    CURSOR_TYPE_COUNT
};

struct PieceDragState
{
    bool  isDragging;
    Vec3  worldPosition;
    Piece piece;
};

struct GameState
{
    bool           isInitialized;
    Assets         assets;
    Camera3D       camera3D;
    Camera2D       camera2D;
    Board          board;
    PieceDragState pieceDragState;
    u32            gameState;
    Rect           cursorTexture;
    bool           gameStarted;
    // Settings
    bool vsyncEnabled;
    bool fullscreenEnabled;
    bool soundEnabled;
    bool showPiecesMovesEnabled;
};