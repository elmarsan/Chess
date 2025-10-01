#pragma once

#include <math.h>

#define PI            3.14159265359f
#define DEGTORAD(deg) ((deg) * (PI) / 180.0f)

inline Vec3 operator-(Vec3 a) { return { -a.x, -a.y, -a.z }; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }

inline Vec3 Cross(Vec3 a, Vec3 b)
{
    Vec3 result;

    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;

    return result;
}

inline float Dot(Vec3 a, Vec3 b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }

inline float Length(Vec3 a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }

inline Vec3 Norm(Vec3 a)
{
    Vec3 result = { 0 };

    float length = Length(a);
    if (length)
    {
        result.x = a.x / length;
        result.y = a.y / length;
        result.z = a.z / length;
    }

    return result;
}

inline Mat4x4 operator*(Mat4x4 a, Mat4x4 b)
{
    Mat4x4 result = { 0 };

    // Column 0
    result.e[0][0] = a.e[0][0] * b.e[0][0] + a.e[1][0] * b.e[0][1] + a.e[2][0] * b.e[0][2] + a.e[3][0] * b.e[0][3];
    result.e[0][1] = a.e[0][1] * b.e[0][0] + a.e[1][1] * b.e[0][1] + a.e[2][1] * b.e[0][2] + a.e[3][1] * b.e[0][3];
    result.e[0][2] = a.e[0][2] * b.e[0][0] + a.e[1][2] * b.e[0][1] + a.e[2][2] * b.e[0][2] + a.e[3][2] * b.e[0][3];
    result.e[0][3] = a.e[0][3] * b.e[0][0] + a.e[1][3] * b.e[0][1] + a.e[2][3] * b.e[0][2] + a.e[3][3] * b.e[0][3];

    // Column 1
    result.e[1][0] = a.e[0][0] * b.e[1][0] + a.e[1][0] * b.e[1][1] + a.e[2][0] * b.e[1][2] + a.e[3][0] * b.e[1][3];
    result.e[1][1] = a.e[0][1] * b.e[1][0] + a.e[1][1] * b.e[1][1] + a.e[2][1] * b.e[1][2] + a.e[3][1] * b.e[1][3];
    result.e[1][2] = a.e[0][2] * b.e[1][0] + a.e[1][2] * b.e[1][1] + a.e[2][2] * b.e[1][2] + a.e[3][2] * b.e[1][3];
    result.e[1][3] = a.e[0][3] * b.e[1][0] + a.e[1][3] * b.e[1][1] + a.e[2][3] * b.e[1][2] + a.e[3][3] * b.e[1][3];

    // Column 2
    result.e[2][0] = a.e[0][0] * b.e[2][0] + a.e[1][0] * b.e[2][1] + a.e[2][0] * b.e[2][2] + a.e[3][0] * b.e[2][3];
    result.e[2][1] = a.e[0][1] * b.e[2][0] + a.e[1][1] * b.e[2][1] + a.e[2][1] * b.e[2][2] + a.e[3][1] * b.e[2][3];
    result.e[2][2] = a.e[0][2] * b.e[2][0] + a.e[1][2] * b.e[2][1] + a.e[2][2] * b.e[2][2] + a.e[3][2] * b.e[2][3];
    result.e[2][3] = a.e[0][3] * b.e[2][0] + a.e[1][3] * b.e[2][1] + a.e[2][3] * b.e[2][2] + a.e[3][3] * b.e[2][3];

    // Column 3
    result.e[3][0] = a.e[0][0] * b.e[3][0] + a.e[1][0] * b.e[3][1] + a.e[2][0] * b.e[3][2] + a.e[3][0] * b.e[3][3];
    result.e[3][1] = a.e[0][1] * b.e[3][0] + a.e[1][1] * b.e[3][1] + a.e[2][1] * b.e[3][2] + a.e[3][1] * b.e[3][3];
    result.e[3][2] = a.e[0][2] * b.e[3][0] + a.e[1][2] * b.e[3][1] + a.e[2][2] * b.e[3][2] + a.e[3][2] * b.e[3][3];
    result.e[3][3] = a.e[0][3] * b.e[3][0] + a.e[1][3] * b.e[3][1] + a.e[2][3] * b.e[3][2] + a.e[3][3] * b.e[3][3];

    return result;
}

inline Mat4x4 Identity()
{
    Mat4x4 result = { 0 };

    result.e[0][0] = 1.0f;
    result.e[1][1] = 1.0f;
    result.e[2][2] = 1.0f;
    result.e[3][3] = 1.0f;

    return result;
}

inline Mat4x4 Translate(Mat4x4 mat, Vec3 translate)
{
    Mat4x4 result = mat;

    result.e[3][0] += translate.x;
    result.e[3][1] += translate.y;
    result.e[3][2] += translate.z;

    return result;
}

inline Mat4x4 Rotate(Mat4x4 mat, float angle, Vec3 rot)
{
    float c = cosf(angle);
    float s = sinf(angle);

    Vec3 norm = Norm(rot);

    float x = norm.x;
    float y = norm.y;
    float z = norm.z;

    Mat4x4 rotation = { 0 };

    rotation.e[0][0] = (1 - c) * (x * x) + c;
    rotation.e[0][1] = (1 - c) * x * y + s * z;
    rotation.e[0][2] = (1 - c) * x * z - s * y;

    rotation.e[1][0] = (1 - c) * x * y - s * z;
    rotation.e[1][1] = (1 - c) * (y * y) + c;
    rotation.e[1][2] = (1 - c) * y * z + s * x;

    rotation.e[2][0] = (1 - c) * x * z + s * y;
    rotation.e[2][1] = (1 - c) * y * z - s * x;
    rotation.e[2][2] = (1 - c) * (z * z) + c;

    rotation.e[3][3] = 1.0f;

    return mat * rotation;
}

// Right-hand coordinates system
inline Mat4x4 LookAt(Vec3 eye, Vec3 center, Vec3 worldUp)
{
    Vec3 forward = Norm(center - eye);
    Vec3 side    = Norm(Cross(forward, worldUp));
    Vec3 up      = Cross(side, forward);

    Mat4x4 result = { 0 };

    result.e[0][0] = side.x;
    result.e[1][0] = side.y;
    result.e[2][0] = side.z;

    result.e[0][1] = up.x;
    result.e[1][1] = up.y;
    result.e[2][1] = up.z;

    result.e[0][2] = -forward.x;
    result.e[1][2] = -forward.y;
    result.e[2][2] = -forward.z;

    result.e[3][0] = -Dot(side, eye);
    result.e[3][1] = -Dot(up, eye);
    result.e[3][2] = Dot(forward, eye);
    result.e[3][3] = 1.0f;

    return result;
}

// Right-hand coordinates system
inline Mat4x4 Perspective(float fov, float aspect, float zNear, float zFar)
{
    Mat4x4 result = { 0 };

    float cotan = 1.0f / tanf(fov / 2.0f);

    result.e[0][0] = cotan / aspect;
    result.e[1][1] = cotan;
    result.e[2][2] = (zNear + zFar) / (zNear - zFar);
    result.e[2][3] = -1.0f;
    result.e[3][2] = (2.0f * zNear * zFar) / (zNear - zFar);

    return result;
}