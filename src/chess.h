#pragma once

#include "chess_types.h"
#include "chess_platform.h"
#include "chess_opengl.h"
#include "chess_asset.h"
#include "chess_camera.h"
#include "chess_draw_api.h"
#include "chess_math.h"
#include "chess_game_logic.h"

enum
{
    GAME_INPUT_CONTROLLER_KEYBOARD_0,
    GAME_INPUT_CONTROLLER_GAMEPAD_0,
    GAME_INPUT_CONTROLLER_GAMEPAD_1,
    GAME_INPUT_CONTROLLER_GAMEPAD_2,
    GAME_INPUT_CONTROLLER_GAMEPAD_3,
    GAME_INPUT_CONTROLLER_COUNT,
};

enum
{
    GAME_BUTTON_ACTION,
    GAME_BUTTON_CANCEL,
    GAME_BUTTON_START,
    GAME_BUTTON_COUNT
};

struct GameButtonState
{
    bool isDown;
    bool wasDown;
};

struct GameInputController
{
    bool isEnabled;
    bool isWireless;

    s16 cursorX;
    s16 cursorY;

    union
    {
        GameButtonState buttons[GAME_BUTTON_COUNT];

        struct
        {
            GameButtonState buttonAction;
            GameButtonState buttonCancel;
            GameButtonState buttonStart;
        };
    };
};

struct GameInput
{
    GameInputController controllers[GAME_INPUT_CONTROLLER_COUNT];
};

enum
{
    KEY_MOUSE_MIDDLE,
    KEY_SHIFT,

    KEY_COUNT
};

struct PieceDragState
{
    bool  isDragging;
    Vec3  worldPosition;
    Piece piece;
};

enum
{
    GAME_STATE_MENU,
    GAME_STATE_SETTINGS,
    GAME_STATE_PLAY
};

enum
{
    CURSOR_TYPE_POINTER,
    CURSOR_TYPE_FINGER,
    CURSOR_TYPE_PICKING,

    CURSOR_TYPE_COUNT
};

struct GameMemory
{
    bool           isInitialized;
    GameInput      input;
    PlatformAPI    platform;
    DrawAPI        draw;
    Assets         assets;
    Camera3D       camera3D;
    Camera2D       camera2D;
    Board          board;
    OpenGL         opengl;
    PieceDragState pieceDragState;
    u32            gameState;
    bool           soundEnabled;
    bool           showPiecesMoves;
    Rect           cursorRect;
};

#define GAME_UPDATE_AND_RENDER(name) bool name(GameMemory* memory, f32 delta)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderProc);