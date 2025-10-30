#pragma once

#define PI            3.14159265359f
#define DEGTORAD(deg) ((deg) * (PI) / 180.0f)

Vec2 Vec2::operator*(f32 scalar) { return Vec2{ x * scalar, y * scalar }; }

inline bool operator==(const Vec2U& lhs, const Vec2U& rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }

inline bool operator!=(const Vec2U& lhs, const Vec2U& rhs) { return !(lhs == rhs); }

Vec3 Vec3::operator+(const Vec3& rhs) const { return Vec3{ x + rhs.x, y + rhs.y, z + rhs.z }; }

Vec3& Vec3::operator+=(const Vec3& rhs)
{
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}

Vec3 Vec3::operator-(const Vec3& rhs) const { return Vec3{ x - rhs.x, y - rhs.y, z - rhs.z }; }

Vec3& Vec3::operator-=(const Vec3& rhs)
{
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}

Vec3 Vec3::operator*(f32 scalar) const { return Vec3{ x * scalar, y * scalar, z * scalar }; }

Vec3 Vec3::operator*(const Vec3& rhs) const { return Vec3{ x * rhs.x, y * rhs.y, z * rhs.z }; }

Vec3& Vec3::operator*=(f32 scalar)
{
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

Vec3 Vec3::operator-() const { return Vec3{ -x, -y, -z }; }

bool Vec3::operator==(const Vec3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }

inline Vec3 Cross(const Vec3& a, const Vec3& b)
{
    Vec3 result;

    result.x = a.y * b.z - a.z * b.y;
    result.y = a.z * b.x - a.x * b.z;
    result.z = a.x * b.y - a.y * b.x;

    return result;
}

inline f32 Dot(const Vec3& a, const Vec3& b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }

inline f32 Length(const Vec3& a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }

inline Vec3 Norm(const Vec3& a)
{
    Vec3 result{ 0 };

    f32 length = Length(a);
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

Vec4 Mat4x4::operator*(const Vec4& vec4) const
{
    Vec4 result{};

    result.x = e[0][0] * vec4.x + e[1][0] * vec4.y + e[2][0] * vec4.z + e[3][0] * vec4.w;
    result.y = e[0][1] * vec4.x + e[1][1] * vec4.y + e[2][1] * vec4.z + e[3][1] * vec4.w;
    result.z = e[0][2] * vec4.x + e[1][2] * vec4.y + e[2][2] * vec4.z + e[3][2] * vec4.w;
    result.w = e[0][3] * vec4.x + e[1][3] * vec4.y + e[2][3] * vec4.z + e[3][3] * vec4.w;

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

inline Mat4x4 Rotate(Mat4x4 mat, f32 angle, Vec3 rot)
{
    f32 c = cosf(angle);
    f32 s = sinf(angle);

    Vec3 norm = Norm(rot);

    f32 x = norm.x;
    f32 y = norm.y;
    f32 z = norm.z;

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

inline Mat4x4 Scale(Mat4x4 mat4, Vec3 scale)
{
    Mat4x4 scaleMat = Identity();

    scaleMat.e[0][0] *= scale.x;
    scaleMat.e[1][1] *= scale.y;
    scaleMat.e[2][2] *= scale.z;

    return mat4 * scaleMat;
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
inline Mat4x4 Perspective(f32 fov, f32 aspect, f32 zNear, f32 zFar)
{
    Mat4x4 result = { 0 };

    f32 cotan = 1.0f / tanf(fov / 2.0f);

    result.e[0][0] = cotan / aspect;
    result.e[1][1] = cotan;
    result.e[2][2] = (zNear + zFar) / (zNear - zFar);
    result.e[2][3] = -1.0f;
    result.e[3][2] = (2.0f * zNear * zFar) / (zNear - zFar);

    return result;
}

inline f32 Cofactor(Mat3x3 mat3)
{
    return mat3.e[0][0] * (mat3.e[1][1] * mat3.e[2][2] - mat3.e[1][2] * mat3.e[2][1]) -
           mat3.e[0][1] * (mat3.e[1][0] * mat3.e[2][2] - mat3.e[1][2] * mat3.e[2][0]) +
           mat3.e[0][2] * (mat3.e[1][0] * mat3.e[2][1] - mat3.e[1][1] * mat3.e[2][0]);
}

inline Mat4x4 Inverse(Mat4x4 mat4)
{
    f32 cofactm00 = Cofactor(Mat3x3{ mat4.e[1][1], mat4.e[1][2], mat4.e[1][3], mat4.e[2][1], mat4.e[2][2], mat4.e[2][3],
                                     mat4.e[3][1], mat4.e[3][2], mat4.e[3][3] });
    f32 cofactm10 = Cofactor(Mat3x3{ mat4.e[1][0], mat4.e[1][2], mat4.e[1][3], mat4.e[2][0], mat4.e[2][2], mat4.e[2][3],
                                     mat4.e[3][0], mat4.e[3][2], mat4.e[3][3] });
    f32 cofactm20 = Cofactor(Mat3x3{ mat4.e[1][0], mat4.e[1][1], mat4.e[1][3], mat4.e[2][0], mat4.e[2][1], mat4.e[2][3],
                                     mat4.e[3][0], mat4.e[3][1], mat4.e[3][3] });

    f32 cofactm30 = Cofactor(Mat3x3{ mat4.e[1][0], mat4.e[1][1], mat4.e[1][2], mat4.e[2][0], mat4.e[2][1], mat4.e[2][2],
                                     mat4.e[3][0], mat4.e[3][1], mat4.e[3][2] });

    // Determinant
    f32 determinant =
        mat4.e[0][0] * cofactm00 - mat4.e[0][1] * cofactm10 + mat4.e[0][2] * cofactm20 - mat4.e[0][3] * cofactm30;
    if (fabsf(determinant) <= 0.00001f)
        return Mat4x4{ 1.0f };

    // Remaining cofactors for adj(M)
    f32 cofactm01 = Cofactor(Mat3x3{ mat4.e[0][1], mat4.e[0][2], mat4.e[0][3], mat4.e[2][1], mat4.e[2][2], mat4.e[2][3],
                                     mat4.e[3][1], mat4.e[3][2], mat4.e[3][3] });
    f32 cofactm11 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][2], mat4.e[0][3], mat4.e[2][0], mat4.e[2][2], mat4.e[2][3],
                                     mat4.e[3][0], mat4.e[3][2], mat4.e[3][3] });
    f32 cofactm21 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][1], mat4.e[0][3], mat4.e[2][0], mat4.e[2][1], mat4.e[2][3],
                                     mat4.e[3][0], mat4.e[3][1], mat4.e[3][3] });
    f32 cofactm31 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][1], mat4.e[0][2], mat4.e[2][0], mat4.e[2][1], mat4.e[2][2],
                                     mat4.e[3][0], mat4.e[3][1], mat4.e[3][2] });

    f32 cofactm02 = Cofactor(Mat3x3{ mat4.e[0][1], mat4.e[0][2], mat4.e[0][3], mat4.e[1][1], mat4.e[1][2], mat4.e[1][3],
                                     mat4.e[3][1], mat4.e[3][2], mat4.e[3][3] });
    f32 cofactm12 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][2], mat4.e[0][3], mat4.e[1][0], mat4.e[1][2], mat4.e[1][3],
                                     mat4.e[3][0], mat4.e[3][2], mat4.e[3][3] });
    f32 cofactm22 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][1], mat4.e[0][3], mat4.e[1][0], mat4.e[1][1], mat4.e[1][3],
                                     mat4.e[3][0], mat4.e[3][1], mat4.e[3][3] });
    f32 cofactm32 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][1], mat4.e[0][2], mat4.e[1][0], mat4.e[1][1], mat4.e[1][2],
                                     mat4.e[3][0], mat4.e[3][1], mat4.e[3][2] });

    f32 cofactm03 = Cofactor(Mat3x3{ mat4.e[0][1], mat4.e[0][2], mat4.e[0][3], mat4.e[1][1], mat4.e[1][2], mat4.e[1][3],
                                     mat4.e[2][1], mat4.e[2][2], mat4.e[2][3] });
    f32 cofactm13 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][2], mat4.e[0][3], mat4.e[1][0], mat4.e[1][2], mat4.e[1][3],
                                     mat4.e[2][0], mat4.e[2][2], mat4.e[2][3] });
    f32 cofactm23 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][1], mat4.e[0][3], mat4.e[1][0], mat4.e[1][1], mat4.e[1][3],
                                     mat4.e[2][0], mat4.e[2][1], mat4.e[2][3] });
    f32 cofactm33 = Cofactor(Mat3x3{ mat4.e[0][0], mat4.e[0][1], mat4.e[0][2], mat4.e[1][0], mat4.e[1][1], mat4.e[1][2],
                                     mat4.e[2][0], mat4.e[2][1], mat4.e[2][2] });

    Mat4x4 inverse{};
    f32    invDet = 1.0f / determinant;

    inverse.e[0][0] = invDet * cofactm00;
    inverse.e[0][1] = -invDet * cofactm01;
    inverse.e[0][2] = invDet * cofactm02;
    inverse.e[0][3] = -invDet * cofactm03;

    inverse.e[1][0] = -invDet * cofactm10;
    inverse.e[1][1] = invDet * cofactm11;
    inverse.e[1][2] = -invDet * cofactm12;
    inverse.e[1][3] = invDet * cofactm13;

    inverse.e[2][0] = invDet * cofactm20;
    inverse.e[2][1] = -invDet * cofactm21;
    inverse.e[2][2] = invDet * cofactm22;
    inverse.e[2][3] = -invDet * cofactm23;

    inverse.e[3][0] = -invDet * cofactm30;
    inverse.e[3][1] = invDet * cofactm31;
    inverse.e[3][2] = -invDet * cofactm32;
    inverse.e[3][3] = invDet * cofactm33;

    return inverse;
}

inline f32 Clamp(f32 value, f32 min, f32 max)
{
    if (value < min)
    {
        return min;
    }
    else if (value > max)
    {
        return max;
    }
    return value;
}

inline f32 Max(f32 a, f32 b) { return a > b ? a : b; }