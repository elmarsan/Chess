#pragma once

#include <stdint.h>

#if CHESS_BUILD_DEBUG
#include <assert.h>
#include <stdio.h>

#define CHESS_LOG(fmt, ...)                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        printf(fmt, __VA_ARGS__);                                                                                      \
        printf("\n");                                                                                                  \
    } while (0)

#define CHESS_ASSERT(expr) assert(expr)
#else
#define CHESS_LOG(...)
#define CHESS_ASSERT(expr)
#endif

#define ARRAY_COUNT(arr) sizeof(arr) / sizeof(arr[0])

#define internal static;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

struct Mat4x4
{
    f32 e[4][4]; // column-major layout
};

union Vec4
{
    f32 e[4];
    struct
    {
        f32 x, y, z, w;
    };
    struct
    {
        f32 r, g, b, a;
    };
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

union Vec3
{
    f32 e[3];
    struct
    {
        f32 x, y, z;
    };
    struct
    {
        f32 r, g, b;
    };
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float scalar) : x(scalar), y(scalar), z(scalar) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    // Arithmetic operators
    Vec3  operator+(const Vec3& rhs) const;
    Vec3& operator+=(const Vec3& rhs);
    Vec3  operator-(const Vec3& rhs) const;
    Vec3& operator-=(const Vec3& rhs);
    Vec3  operator*(float scalar) const;
    Vec3  operator*(const Vec3& rhs) const;
    Vec3& operator*=(float scalar);
    Vec3  operator-() const;
    bool  operator==(const Vec3& rhs) const;
};

union Vec2
{
    f32 e[2];
    struct
    {
        f32 x, y;
    };
    struct
    {
        f32 w, h;
    };
    Vec2() : x(0), y(0) {}
    Vec2(float scalar) : x(scalar), y(scalar) {}
    Vec2(float x, float y) : x(x), y(y) {}

    // Arithmetic operators
    Vec2 operator*(float scalar);
};

union Vec2U
{
    u32 e[2];
    struct
    {
        u32 x, y;
    };
    struct
    {
        u32 w, h;
    };
    Vec2U() : x(0), y(0) {}
    Vec2U(u32 scalar) : x(scalar), y(scalar) {}
    Vec2U(u32 x, u32 y) : x(x), y(y) {}
};