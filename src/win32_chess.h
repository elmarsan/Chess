#pragma once

#include <windows.h>

#include "chess.h"

struct Win32State
{
    bool          running;
    HWND          window;
    HDC           deviceContext;
    HGLRC         glContext;
    const char*   classname;
    GameInput     gameInput;
    s64           performanceCounterFreq;
    f32           deltaTime;
    LARGE_INTEGER beginTick;
};

// TODO: Get rid of this extern
extern Win32State win32State;

struct Win32GameCode
{
    HMODULE                  gameCodeDLL;
    FILETIME                 dllLastWriteTime;
    GameUpdateAndRenderProc* UpdateAndRender;
    bool                     isValid;
};

void Win32LogLastError(const char* functionName);