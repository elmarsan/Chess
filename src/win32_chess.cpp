#include <windows.h>
#include <windowsx.h>
#include <xinput.h>

#include "chess.cpp"
#include "win32_opengl.cpp"
#include "chess_draw_api_opengl.cpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Win32State
{
    bool          running;
    HWND          window;
    GameInput     gameInput;
    s64           performanceCounterFreq;
    f32           deltaTime;
    LARGE_INTEGER beginTick;
    bool          isFullscreen;
    RECT          borderlessRect;
    ma_engine     miniaudioEngine;
};

struct Win32GameCode
{
    HMODULE                  gameCodeDLL;
    FILETIME                 dllLastWriteTime;
    GameUpdateAndRenderProc* UpdateAndRender;
    bool                     isValid;
};

static Win32State win32State = { 0 };

chess_internal inline void Win32LogLastError(const char* functionName)
{
    DWORD errorCode      = GetLastError();
    LPSTR errorMsgBuffer = 0;

    // clang-format off
	DWORD flags =
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_MAX_WIDTH_MASK; // No break line in error message
    // clang-format on

    DWORD msgSize = FormatMessageA(flags, 0, errorCode, 0, (LPSTR)&errorMsgBuffer, 0, 0);
    if (msgSize)
    {
        CHESS_LOG("-----------------------------");
        CHESS_LOG("[WIN32] error calling function '%s', code '%d': %s", functionName, errorCode, errorMsgBuffer);
        CHESS_LOG("-----------------------------");
    }
    if (errorMsgBuffer)
    {
        LocalFree(errorMsgBuffer);
    }
}

// ----------------------------------------------------------------------------
// XInput
typedef DWORD(WINAPI* XInputGetStateFunc)(DWORD, XINPUT_STATE*);
typedef DWORD(WINAPI* XInputGetCapabilitiesFunc)(DWORD, DWORD, XINPUT_CAPABILITIES*);
// typedef DWORD(WINAPI* XInputGetBatteryInformationFunc)(DWORD, DWORD, XINPUT_BATTERY_INFORMATION*);

chess_internal XInputGetStateFunc        XInputGetStateProc;
chess_internal XInputGetCapabilitiesFunc XInputGetCapabilitiesProc;
// global XInputGetBatteryInformationFunc XInputGetBatteryInformationProc;

#define XInputGetState        XInputGetStateProc
#define XInputGetCapabilities XInputGetCapabilitiesProc
// #define XInputGetBatteryInformation XInputGetBatteryInformationProc

chess_internal void Win32XInputInit()
{
    CHESS_LOG("[WIN32] initializing XInput...");

    // Windows 8 (XInput 1.4), DirectX SDK (XInput 1.3), Windows Vista (XInput 9.1.0)
    const char* library   = "xinput1_4.dll";
    HMODULE     XInputDLL = LoadLibraryA(library);

    if (!XInputDLL)
    {
        CHESS_LOG("[WIN32] unable to load XInput dll: '%s'", library);
        CHESS_ASSERT(0);

        // TODO: Error handling in RELEASE build
    }
    else
    {
        XInputGetStateProc        = (XInputGetStateFunc)GetProcAddress(XInputDLL, "XInputGetState");
        XInputGetCapabilitiesProc = (XInputGetCapabilitiesFunc)GetProcAddress(XInputDLL, "XInputGetCapabilities");
        // XInputGetBatteryInformationProc =
        //     (XInputGetBatteryInformationFunc)GetProcAddress(XInputDLL, "XInputGetBatteryInformation");

        // TODO: Error handling in RELEASE build
        CHESS_ASSERT(XInputGetStateProc);
        CHESS_ASSERT(XInputGetCapabilitiesProc);
        // CHESS_ASSERT(XInputGetBatteryInformationProc);
    }
}

chess_internal inline f32 Win32GetControllerStick(SHORT stickValue, SHORT deadzone)
{
    f32 value = 0.0f;

    if (stickValue < -deadzone)
    {
        value = (f32)((stickValue + deadzone) / (32768.0f - deadzone));
    }
    else if (stickValue > deadzone)
    {
        value = (f32)((stickValue - deadzone) / (32768.0f - deadzone));
    }

    return value;
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Sound
chess_internal void Win32MiniaudioInit()
{
    CHESS_LOG("[WIN32] initializing miniaudio library...");

    ma_result result = ma_engine_init(NULL, &win32State.miniaudioEngine);
    if (result != MA_SUCCESS)
    {
        const char* errorMsg = ma_result_description(result);
        CHESS_LOG("[WIN32] unable to init miniaudio: '%s'", errorMsg);
        CHESS_ASSERT(0);
    }
}

chess_internal void Win32MiniaudioDestroy() { ma_engine_uninit(&win32State.miniaudioEngine); }

PLATFORM_SOUND_LOAD(Win32SoundLoad)
{
    CHESS_LOG("[WIN32] loading sound: '%s'...", filename);
    Sound result = {};

    ma_sound* maSound = (ma_sound*)malloc(sizeof(ma_sound));
    memset(maSound, 0, sizeof(ma_sound));
    ma_result initSoundResult = ma_sound_init_from_file(&win32State.miniaudioEngine, filename, 0, NULL, NULL, maSound);
    if (initSoundResult == MA_SUCCESS)
    {
        result.handle  = maSound;
        result.isValid = true;
    }
    else
    {
        const char* errorMsg = ma_result_description(initSoundResult);
        CHESS_LOG("[WIN32] unable to load sound '%s': '%s'", filename, errorMsg);
        CHESS_ASSERT(0);

        result.isValid = false;
    }

    return result;
}

PLATFORM_SOUND_PLAY(Win32SoundPlay)
{
    ma_result result = ma_sound_start((ma_sound*)sound->handle);
    if (result != MA_SUCCESS)
    {
        const char* errorMsg = ma_result_description(result);
        CHESS_LOG("[WIN32] unable to play sound: '%s'", errorMsg);
    }
}

PLATFORM_SOUND_DESTROY(Win32SoundDestroy)
{
    if (sound && sound->handle)
    {
        CHESS_ASSERT(sound->isValid);
        free(sound->handle);
        sound->handle = 0;
    }
    else
    {
        CHESS_ASSERT(0);
    }
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Image
PLATFORM_IMAGE_LOAD(Win32ImageLoad)
{
    CHESS_LOG("[WIN32] loading image: '%s'...", filename);

    Image result   = { 0 };
    result.isValid = false;

    int width, height, numChannels;
    u8* pixels = stbi_load(filename, &width, &height, &numChannels, 0);
    if (pixels)
    {
        result.pixels        = pixels;
        result.width         = (u32)width;
        result.height        = (u32)height;
        result.isValid       = true;
        result.bytesPerPixel = (u32)numChannels;
    }
    else
    {
        CHESS_LOG("[WIN32] unable to load image '%s'", filename);
        CHESS_ASSERT(0);
    }

    return result;
}

PLATFORM_IMAGE_DESTROY(Win32ImageDestroy)
{
    if (image && image->pixels)
    {
        CHESS_ASSERT(image->isValid);
        stbi_image_free(image->pixels);
        image->pixels  = 0;
        image->width   = 0;
        image->height  = 0;
        image->isValid = false;
    }
    else
    {
        CHESS_ASSERT(0);
    }
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Window
PLATFORM_WINDOW_GET_DIMENSION(Win32WindowGetDimension)
{
    Vec2U result;

    RECT rect;
    GetClientRect(win32State.window, &rect);
    int width  = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    result.w = (u32)width;
    result.h = (u32)height;

    return result;
}

chess_internal inline void Win32ResetInput()
{
    GameInputController* gamepadController = &win32State.gameInput.controllers[GAME_INPUT_CONTROLLER_GAMEPAD_0];
    bool                 controllerEnabled = gamepadController->isEnabled;
    memset(&win32State.gameInput, 0, sizeof(GameInput));

    if (controllerEnabled)
    {
        Vec2U dimension              = Win32WindowGetDimension();
        gamepadController->isEnabled = true;
        gamepadController->cursorX   = dimension.w / 2;
        gamepadController->cursorY   = dimension.y / 2;
    }
}

PLATFORM_WINDOW_RESIZE(Win32WindowResize)
{
    win32State.isFullscreen = false;
    int x                   = (GetSystemMetrics(SM_CXSCREEN) - dimension.w) / 2;
    int y                   = (GetSystemMetrics(SM_CYSCREEN) - dimension.h) / 2;
    SetWindowPos(win32State.window, HWND_TOP, x, y, dimension.w, dimension.h, SWP_SHOWWINDOW);
    Win32ResetInput();
}

PLATFORM_WINDOW_SET_FULLSCREEN(Win32WindowSetFullscreen)
{
    win32State.isFullscreen = true;
    GetWindowRect(win32State.window, &win32State.borderlessRect);
    SetWindowPos(win32State.window, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                 SWP_SHOWWINDOW);
    Win32ResetInput();
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Timer
chess_internal inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

chess_internal inline f64 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    return (f64)(end.QuadPart - start.QuadPart) / (f64)win32State.performanceCounterFreq;
}

PLATFORM_TIMER_GET_TICKS(Win32TimerGetTicks)
{
    LARGE_INTEGER currentTick = Win32GetWallClock();
    return Win32GetSecondsElapsed(win32State.beginTick, currentTick);
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// File
PLATFORM_FILE_READ_ENTIRE(Win32FileReadEntire)
{
    CHESS_LOG("[WIN32] reading entire file: '%s'", filename);

    FileReadResult result = { 0 };

    FILE* file = fopen(filename, "rb");
    if (file)
    {
        fseek(file, 0, SEEK_END);
        // Note: ftell() returns long, which is 32-bit on Windows.
        // This means it will overflow for files larger than 2 GB.
        // In this project the files are small, so we shouldn't hit that limitation.
        long size = ftell(file);
        if (size != -1L)
        {
            result.contentSize = (u64)size;
            result.filename    = filename;
            result.content     = new u8[result.contentSize + 1];

            fseek(file, 0, SEEK_SET);
            fread(result.content, 1, result.contentSize, file);
            fclose(file);
        }
        else
        {
            CHESS_LOG("[WIN32] unable to read the end of the file '%s'", filename);
            CHESS_ASSERT(0);
        }
    }
    else
    {
        CHESS_LOG("[WIN32] unable to open file '%s'", filename);
        CHESS_ASSERT(0);
    }

    return result;
}

PLATFORM_FILE_FREE_MEMORY(Win32FileFreeMemory)
{
    if (memory)
    {
        delete[] (u8*)memory;
    }
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Logging
PLATFORM_LOG(Win32Log)
{
#if CHESS_BUILD_DEBUG
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
#endif
}
// ----------------------------------------------------------------------------

chess_internal inline FILETIME Win32GetLastWriteTime(const char* filename)
{
    FILETIME lastWriteTime = {};

    WIN32_FIND_DATAA findData;
    HANDLE           findHandle = FindFirstFileA(filename, &findData);
    if (findHandle != INVALID_HANDLE_VALUE)
    {
        lastWriteTime = findData.ftLastWriteTime;
        FindClose(findHandle);
    }

    return lastWriteTime;
}

chess_internal Win32GameCode Win32LoadGameCode(const char* gameDLLFilepath, const char* copyDLLFilepath)
{
    Win32GameCode result = {};

    // TODO: Get proper path
    result.dllLastWriteTime = Win32GetLastWriteTime(gameDLLFilepath);

    CopyFileA(gameDLLFilepath, copyDLLFilepath, FALSE);

    result.gameCodeDLL = LoadLibraryA(copyDLLFilepath);
    if (result.gameCodeDLL)
    {
        result.UpdateAndRender = (GameUpdateAndRenderProc*)GetProcAddress(result.gameCodeDLL, "GameUpdateAndRender");

        result.isValid = result.UpdateAndRender;
    }
    else
    {
        Win32LogLastError("LoadLibraryA");
    }

    if (!result.isValid)
    {
        CHESS_LOG("[WIN32] unable to load game code: '%s'", gameDLLFilepath);
        result.UpdateAndRender = 0;
    }

    return result;
}

chess_internal void Win32UnloadGameCode(Win32GameCode* gameCode)
{
    if (gameCode->gameCodeDLL)
    {
        CHESS_LOG("[WIN32] releasing game code...");
        FreeLibrary(gameCode->gameCodeDLL);
        // BOOL success = FreeLibrary(gameCode->gameCodeDLL);
        // CHESS_ASSERT(success);
        gameCode->gameCodeDLL = 0;
    }

    gameCode->dllLastWriteTime = {};
    gameCode->isValid          = false;
    gameCode->UpdateAndRender  = 0;

    Sleep(100);
}

chess_internal void Win32UpdateGameButtonState(GameButtonState* buttonState, bool isDown)
{
    CHESS_ASSERT(buttonState);
    buttonState->isDown = isDown;
}

chess_internal void Win32ProcessPendingMessages(Win32State* state, GameInputController* keyboardController)
{
    CHESS_ASSERT(state);
    CHESS_ASSERT(keyboardController);

    MSG msg;
    while (PeekMessageA(&msg, state->window, 0, 0, PM_REMOVE))
    {
        switch (msg.message)
        {
        case WM_SYSKEYUP:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            u32  vkCode        = (u32)msg.wParam;
            bool wasDown       = (msg.lParam >> 30) & 1;
            bool isDown        = ((msg.lParam >> 31) & 1) == 0;
            bool altKeyWasDown = (msg.lParam >> 29) & 1;

            if (vkCode == VK_ESCAPE)
            {
                Win32UpdateGameButtonState(&keyboardController->buttonStart, isDown);
            }
            if (vkCode == VK_F4 && altKeyWasDown)
            {
                CHESS_LOG("[WIN32] ALT+F4 pressed, closing chess...");
                state->running = false;
            }
            if (vkCode == VK_F11 && !wasDown)
            {
                // Exit fullscreen
                if (state->isFullscreen)
                {
                    u32 w = (u32)state->borderlessRect.right - (u32)state->borderlessRect.left;
                    u32 h = (u32)state->borderlessRect.bottom - (u32)state->borderlessRect.top;
                    Win32WindowResize(Vec2U{ w, h });
                }
                // Enter fullscreen
                else
                {
                    Win32WindowSetFullscreen();
                }
            }
            break;
        }
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
        {
            Win32UpdateGameButtonState(&keyboardController->buttonAction, msg.wParam & MK_LBUTTON);
            break;
        }
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
        {
            Win32UpdateGameButtonState(&keyboardController->buttonCancel, msg.wParam & MK_RBUTTON);
            break;
        }
        case WM_MOUSEMOVE:
        {
            keyboardController->cursorX = GET_X_LPARAM(msg.lParam);
            keyboardController->cursorY = GET_Y_LPARAM(msg.lParam);
#if 0
            CHESS_LOG("%d %d", keyboardController->cursorX, keyboardController->cursorY);
#endif
            break;
        }
        default:
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            break;
        }
        }
    }
}

LRESULT CALLBACK Win32WndProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    switch (msg)
    {
    case WM_SIZE:
    {
        CHESS_LOG("[WIN32] WM_SIZE");
        Vec2U dimension = Win32WindowGetDimension();
        RendererSetViewport(0, 0, dimension.w, dimension.h);
        break;
    }
    case WM_CLOSE:
    {
        CHESS_LOG("[WIN32] WM_CLOSE");
        win32State.running = false;
        break;
    }
    default:
    {
        result = DefWindowProcA(window, msg, wParam, lParam);
        break;
    }
    }

    return result;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdline, int cmdshow)
{
#if CHESS_BUILD_DEBUG
    AllocConsole();
    FILE* fconsole;
    freopen_s(&fconsole, "CONOUT$", "w", stdout);
    CHESS_LOG("[WIN32] Chess debug console");
#endif

    LARGE_INTEGER perfCounterFrequencyResult;
    QueryPerformanceFrequency(&perfCounterFrequencyResult);
    win32State.performanceCounterFreq = perfCounterFrequencyResult.QuadPart;

    win32State.beginTick = Win32GetWallClock();

    char  workingDirectory[MAX_PATH];
    DWORD length = GetCurrentDirectoryA(MAX_PATH, workingDirectory);
    if (length == 0)
    {
        Win32LogLastError("GetCurrentDirectoryA");
    }
    else
    {
        CHESS_LOG("[WIN32] working directory: '%s'", workingDirectory);
    }

    WNDCLASSEXA windowClass   = {};
    windowClass.cbSize        = sizeof(WNDCLASSEXA);
    windowClass.hInstance     = hInstance;
    windowClass.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc   = Win32WndProc;
    windowClass.lpszClassName = "ChessWindowClassname";

    if (!RegisterClassExA(&windowClass))
    {
        // TODO: Error handling in RELEASE build
        Win32LogLastError("RegisterClassExA");
        CHESS_ASSERT(0);
        return 1;
    }

    // 16:9
    //	2560x1440
    //	1920x1080
    //	1366x768
    //	1280x720
    u32 width  = 1280;
    u32 height = 720;
    int x      = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y      = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    DWORD style = WS_POPUPWINDOW | WS_VISIBLE;
    win32State.window =
        CreateWindowExA(0, windowClass.lpszClassName, "Chess", style, x, y, width, height, 0, 0, hInstance, 0);
    if (!win32State.window)
    {
        // TODO: Error handling in RELEASE build
        Win32LogLastError("CreateWindowExA");
        CHESS_ASSERT(0);
        return 1;
    }

    ShowCursor(FALSE);

    HDC deviceContext = GetWindowDC(win32State.window);

    Win32XInputInit();
    Win32MiniaudioInit();

    HGLRC glContext = { 0 };
    RendererInit(windowClass.lpszClassName, deviceContext, glContext);
    DrawAPI draw      = DrawApiCreate();
    Vec2U   dimension = Win32WindowGetDimension();
    draw.Init(dimension.w, dimension.h);

    // TODO: Get proper path
    const char*   gameDLLFilepath     = "Chess.dll";
    const char*   tempGameDLLFilepath = "CopyChess.dll";
    Win32GameCode game                = Win32LoadGameCode(gameDLLFilepath, tempGameDLLFilepath);

    GameMemory gameMemory                   = {};
    gameMemory.input                        = win32State.gameInput;
    gameMemory.platform.SoundLoad           = Win32SoundLoad;
    gameMemory.platform.SoundPlay           = Win32SoundPlay;
    gameMemory.platform.SoundDestroy        = Win32SoundDestroy;
    gameMemory.platform.ImageLoad           = Win32ImageLoad;
    gameMemory.platform.ImageDestroy        = Win32ImageDestroy;
    gameMemory.platform.WindowGetDimension  = Win32WindowGetDimension;
    gameMemory.platform.WindowResize        = Win32WindowResize;
    gameMemory.platform.WindowSetFullscreen = Win32WindowSetFullscreen;
    gameMemory.platform.TimerGetTicks       = Win32TimerGetTicks;
    gameMemory.platform.FileReadEntire      = Win32FileReadEntire;
    gameMemory.platform.FileFreeMemory      = Win32FileFreeMemory;
    gameMemory.platform.Log                 = Win32Log;
    gameMemory.draw                         = draw;

    // TODO: Figure out right amount of memory
    gameMemory.permanentStorageSize = MEGABYTES(256);
    gameMemory.permanentStorage =
        VirtualAlloc(NULL, (size_t)gameMemory.permanentStorageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!gameMemory.permanentStorage)
    {
        Win32LogLastError("VirtualAlloc");
        CHESS_ASSERT(0);
        return 1;
    }

    // Init controllers
    win32State.gameInput.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0].isEnabled = true;
    for (u32 i = GAME_INPUT_CONTROLLER_GAMEPAD_0; i < ARRAY_COUNT(win32State.gameInput.controllers); i++)
    {
        GameInputController* controller = &win32State.gameInput.controllers[i];
        controller->isEnabled           = false;
        controller->cursorX             = dimension.w / 2;
        controller->cursorY             = dimension.y / 2;
    }

    LARGE_INTEGER frameStartTime = Win32GetWallClock();

    win32State.running = true;
    while (win32State.running)
    {
        FILETIME lastGameDLLWriteTime = Win32GetLastWriteTime(gameDLLFilepath);
        if (CompareFileTime(&lastGameDLLWriteTime, &game.dllLastWriteTime) == 1)
        {
#if CHESS_BUILD_DEBUG
            SYSTEMTIME lastDLLWriteTime, previousDLLWriteTime;
            FileTimeToSystemTime(&lastGameDLLWriteTime, &lastDLLWriteTime);
            FileTimeToSystemTime(&game.dllLastWriteTime, &previousDLLWriteTime);

            CHESS_LOG("[WIN32] previous game dll write time: %02d:%02d:%02d%02d", previousDLLWriteTime.wHour,
                      previousDLLWriteTime.wMinute, previousDLLWriteTime.wSecond, previousDLLWriteTime.wMilliseconds);
            CHESS_LOG("[WIN32] current game dll write time: %02d:%02d:%02d%02d", lastDLLWriteTime.wHour,
                      lastDLLWriteTime.wMinute, lastDLLWriteTime.wSecond, lastDLLWriteTime.wMilliseconds);
#endif
            CHESS_LOG("[WIN32] reloading game code...");
            Win32UnloadGameCode(&game);
            game = Win32LoadGameCode(gameDLLFilepath, tempGameDLLFilepath);
            CHESS_ASSERT(game.isValid);
        }

        for (u32 i = 0; i < ARRAY_COUNT(win32State.gameInput.controllers); i++)
        {
            GameInputController* controller = &win32State.gameInput.controllers[i];

            for (u32 j = 0; j < GAME_BUTTON_COUNT; j++)
            {
                controller->buttons[j].wasDown = controller->buttons[j].isDown;
            }
        }

        Win32ProcessPendingMessages(&win32State, &win32State.gameInput.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0]);

        for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++)
        {
            XINPUT_STATE controllerState;
            DWORD        result = XInputGetState(controllerIndex, &controllerState);

            GameInputController* gamepadControler = &win32State.gameInput.controllers[controllerIndex + 1];

            if (result == ERROR_SUCCESS)
            {
                // CHESS_LOG("[WIN32] XInput controller: '%d' is connected", controllerIndex);
                gamepadControler->buttonAction.isDown = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A;
                gamepadControler->buttonCancel.isDown = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_B;
                gamepadControler->buttonStart.isDown  = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_START;

                SHORT deadzone   = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
                f32   leftStickX = Win32GetControllerStick(controllerState.Gamepad.sThumbLX, deadzone);
                f32   leftStickY = Win32GetControllerStick(controllerState.Gamepad.sThumbLY, deadzone);

                gamepadControler->cursorX += (s16)(leftStickX * 3.2f);
                gamepadControler->cursorY -= (s16)(leftStickY * 3.2f);

                Vec2U dimension = Win32WindowGetDimension();

                gamepadControler->cursorX = Clamp(gamepadControler->cursorX, 0, dimension.w);
                gamepadControler->cursorY = Clamp(gamepadControler->cursorY, 0, dimension.y);

                // Transition from disconnected to connected
                if (!gamepadControler->isEnabled)
                {
                    // CHESS_LOG("[WIN32] XInput controller: '%d' connected", controllerIndex);
                    gamepadControler->isEnabled = true;

                    XINPUT_CAPABILITIES controllerCaps;
                    DWORD getCapsResult = XInputGetCapabilities(controllerIndex, XINPUT_FLAG_GAMEPAD, &controllerCaps);
                    if (getCapsResult == ERROR_SUCCESS)
                    {
                        if (controllerCaps.Flags & XINPUT_CAPS_WIRELESS)
                        {
                            gamepadControler->isWireless = true;
                            CHESS_LOG("[WIN32] XInput controller: '%d' connected (wireless)", controllerIndex);

                            // TODO: For getting battery info should use Windows.Gaming.Input
                            // https://learn.microsoft.com/en-us/uwp/api/windows.gaming.input.arcadestick.trygetbatteryreport?view=winrt-26100
                            // XINPUT_BATTERY_INFORMATION batteryInfo;
                            // DWORD getBatteryInfoResult = XInputGetBatteryInformation(controllerIndex,
                            // BATTERY_DEVTYPE_GAMEPAD, &batteryInfo);
                            // if (getBatteryInfoResult == ERROR_SUCCESS)
                            //{
                            //}
                            // else
                            //{
                            //	CHESS_LOG("[WIN32] XInput unable to get controller: '%d' battery info",
                            // controllerIndex);
                            //}
                        }
                        else
                        {
                            gamepadControler->isWireless = false;
                            CHESS_LOG("[WIN32] XInput controller: '%d' connected (wired)", controllerIndex);
                        }
                    }
                    else
                    {
                        CHESS_LOG("[WIN32] XInput unable to get controller: '%d' capabilities", controllerIndex);
                        // At this point controller is connected, getting the capabilities shouldn't fail
                        CHESS_ASSERT(0);
                    }
                }
            }
            else if (result == ERROR_DEVICE_NOT_CONNECTED)
            {
                // CHESS_LOG("[WIN32] XInput controller: '%d' is not connected", controllerIndex);
                // Transition from connected to disconnected
                if (gamepadControler->isEnabled)
                {
                    CHESS_LOG("[WIN32] XInput controller: '%d' disconnected", controllerIndex);
                    gamepadControler->isEnabled = false;
                }
            }
            else
            {
                CHESS_LOG("[WIN32] XInputGetState unable to get controller '%lu' state, error code: '%lu'",
                          controllerIndex, result);
            }
        }
        gameMemory.input = win32State.gameInput;

        bool running = game.UpdateAndRender(&gameMemory, win32State.deltaTime);
        SwapBuffers(deviceContext);

        LARGE_INTEGER frameEndTime = Win32GetWallClock();
        win32State.deltaTime       = Win32GetSecondsElapsed(frameStartTime, frameEndTime);
        frameStartTime             = frameEndTime;

        if (!running)
        {
            win32State.running = false;
        }
#if 0
		f32 msPerFrame = 1000.0f * win32State.deltaTime;
		CHESS_LOG("Delta: %.4f ms/f  %.4f fps", msPerFrame, 1.0f / win32State.deltaTime);
#endif
    }

    // Clean up
    draw.Destroy();
    Win32MiniaudioDestroy();
    RendererDestroy(glContext);
    ReleaseDC(win32State.window, deviceContext);
    DestroyWindow(win32State.window);

#if CHESS_BUILD_DEBUG
    FreeConsole();
#endif
    return 0;
}