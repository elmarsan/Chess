#pragma once

#include "chess_types.h"

struct Sound
{
    const char* filename;
    void*       handle;
    bool        isValid;
};

struct Image
{
    const char* filename;
    u8*         pixels;
    u32         width;
    u32         height;
    bool        isValid;
    u32         bytesPerPixel;
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

#define PLATFORM_TIMER_GET_TICKS(name) f64 name()
typedef PLATFORM_TIMER_GET_TICKS(PlatformTimerGetTicksFunc);

struct PlatformAPI
{
    PlatformSoundLoadFunc*          SoundLoad;
    PlatformSoundPlayFunc*          SoundPlay;
    PlatformSoundDestroyFunc*       SoundDestroy;
    PlatformImageLoadFunc*          ImageLoad;
    PlatformImageDestroyFunc*       ImageDestroy;
    PlatformWindowGetDimensionFunc* WindowGetDimension;
    PlatformTimerGetTicksFunc*      TimerGetTicks;
};