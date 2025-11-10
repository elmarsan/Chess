#pragma once

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

inline bool ButtonIsPressed(GameButtonState gameButtonState)
{
    return gameButtonState.isDown && !gameButtonState.wasDown;
}

inline bool ButtonIsDown(GameButtonState gameButtonState) { return gameButtonState.isDown; }

inline bool ButtonIsUp(GameButtonState gameButtonState) { return !gameButtonState.isDown; }

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

struct Sound
{
    void* handle;
    bool  isValid;
};

struct Image
{
    u8*  pixels;
    u32  width;
    u32  height;
    bool isValid;
    u32  bytesPerPixel;
};

struct FileReadResult
{
    void*       content;
    u64         contentSize;
    const char* filename;
};

#define PLATFORM_SOUND_LOAD(name) Sound name(const char* filename)
typedef PLATFORM_SOUND_LOAD(PlatformSoundLoadFunc);

#define PLATFORM_SOUND_PLAY(name) void name(Sound* sound)
typedef PLATFORM_SOUND_PLAY(PlatformSoundPlayFunc);

#define PLATFORM_SOUND_DESTROY(name) void name(Sound* sound)
typedef PLATFORM_SOUND_DESTROY(PlatformSoundDestroyFunc);

#define PLATFORM_IMAGE_LOAD(name) Image name(const char* filename)
typedef PLATFORM_IMAGE_LOAD(PlatformImageLoadFunc);

#define PLATFORM_IMAGE_DESTROY(name) void name(Image* image)
typedef PLATFORM_IMAGE_DESTROY(PlatformImageDestroyFunc);

#define PLATFORM_WINDOW_GET_DIMENSION(name) Vec2U name()
typedef PLATFORM_WINDOW_GET_DIMENSION(PlatformWindowGetDimensionFunc);

#define PLATFORM_WINDOW_RESIZE(name) void name(Vec2U dimension)
typedef PLATFORM_WINDOW_RESIZE(PlatformWindowResizeFunc);

#define PLATFORM_WINDOW_SET_FULLSCREEN(name) void name()
typedef PLATFORM_WINDOW_SET_FULLSCREEN(PlatformWindowSetFullscreenFunc);

#define PLATFORM_TIMER_GET_TICKS(name) f64 name()
typedef PLATFORM_TIMER_GET_TICKS(PlatformTimerGetTicksFunc);

#define PLATFORM_FILE_READ_ENTIRE(name) FileReadResult name(const char* filename)
typedef PLATFORM_FILE_READ_ENTIRE(PlatformFileReadEntireFunc);

#define PLATFORM_FILE_FREE_MEMORY(name) void name(void* memory)
typedef PLATFORM_FILE_FREE_MEMORY(PlatformFileFreeMemoryFunc);

#define PLATFORM_LOG(name) void name(const char* fmt, ...)
typedef PLATFORM_LOG(PlatformLogFunc);

struct PlatformAPI
{
    PlatformSoundLoadFunc*           SoundLoad;
    PlatformSoundPlayFunc*           SoundPlay;
    PlatformSoundDestroyFunc*        SoundDestroy;
    PlatformImageLoadFunc*           ImageLoad;
    PlatformImageDestroyFunc*        ImageDestroy;
    PlatformWindowGetDimensionFunc*  WindowGetDimension;
    PlatformWindowResizeFunc*        WindowResize;
    PlatformWindowSetFullscreenFunc* WindowSetFullscreen;
    PlatformTimerGetTicksFunc*       TimerGetTicks;
    PlatformFileReadEntireFunc*      FileReadEntire;
    PlatformFileFreeMemoryFunc*      FileFreeMemory;
    PlatformLogFunc*                 Log;
};

struct GameMemory
{
    GameInput   input;
    PlatformAPI platform;
    DrawAPI     draw;
    u64         permanentStorageSize;
    void*       permanentStorage;
};

#define GAME_UPDATE_AND_RENDER(name) bool name(GameMemory* memory, f32 delta)
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderProc);