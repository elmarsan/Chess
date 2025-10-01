#pragma once

#include "chess_platform.h"
#include "chess_gfx.h"

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
    GAME_SOUND_MOVE,
    GAME_SOUND_ILLEGAL,
    GAME_SOUND_CHECK,

    GAME_SOUND_COUNT
};

struct GameMemory
{
    bool            isInitialized;
    GameInput       input;
    PlatformAPI     platform;
    GfxAPI          gfx;
    Sound           sounds[GAME_SOUND_COUNT];
    GfxVertexBuffer vbo;
    GfxPipeline     pipeline;
    GfxVertexArray  vao;
    // TODO: Determine number of textures
    GfxTexture textures[4];
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory* memory)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderProc);