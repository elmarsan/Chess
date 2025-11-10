#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string>

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

#define chess_internal static;

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

#define KILOBYTES(value) ((value) * 1024LL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024LL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024LL)

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

struct Mat4x4
{
    f32 e[4][4]; // column-major layout

    // Arithmetic operators
    Vec4 operator*(const Vec4& rhs) const;
};

struct Mat3x3
{
    f32 e[3][3]; // column-major layout

    Mat3x3() = default;

    Mat3x3(f32 e00, f32 e01, f32 e02, f32 e10, f32 e11, f32 e12, f32 e20, f32 e21, f32 e22)
    {
        e[0][0] = e00;
        e[0][1] = e01;
        e[0][2] = e02;

        e[1][0] = e10;
        e[1][1] = e11;
        e[1][2] = e12;

        e[2][0] = e20;
        e[2][1] = e21;
        e[2][2] = e22;
    }
};

struct Rect
{
    constexpr Rect() : x(0.0f), y(0.0f), w(0.0f), h(0.0f) {}
    constexpr Rect(f32 x, f32 y, f32 w, f32 h) : x(x), y(y), w(w), h(h) {}

    f32 x;
    f32 y;
    f32 w;
    f32 h;
};

template <typename T>
struct Stack
{
    static constexpr u32 stackSize = 500;

    T   elements[stackSize];
    u32 top = 0u;

    void push(T element)
    {
        CHESS_ASSERT(top < stackSize);
        elements[top++] = element;
    }

    T pop()
    {
        CHESS_ASSERT(top > 0);
        return elements[--top];
    }

    T peek() const
    {
        CHESS_ASSERT(top > 0);
        return elements[top - 1];
    }

    void clear() { top = 0; }
};
