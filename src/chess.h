#pragma once

#include "chess_asset.h"
#include "chess_camera.h"
#include "chess_draw_api.h"
#include "chess_gfx.h"
#include "chess_platform.h"

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

struct GameMemory
{
    bool        isInitialized;
    GameInput   input;
    PlatformAPI platform;
    GfxAPI      gfx;
    DrawAPI     draw;
    Assets      assets;
    Camera      camera;
    GfxPipeline pipeline;
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory* memory, float delta)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderProc);