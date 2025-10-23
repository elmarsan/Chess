#pragma once

struct Camera3D
{
    Mat4x4 view;
    Mat4x4 projection;
    Vec3   target;
    Vec3   position;
    Vec3   up;
    f32    distance;
    f32    fov;
    f32    pitch;
    f32    yaw;
};

Camera3D Camera3DInit(Vec3 position, Vec3 front, Vec3 up, f32 pitch, f32 yaw, f32 fov, f32 distance);
void     Camera3DUpdateProjection(Camera3D* camera, u32 width, u32 height, f32 zNear, f32 zFar);
void     Camera3DSetPitch(Camera3D* camera, f32 pitch);
void     Camera3DSetYaw(Camera3D* camera, f32 yaw);

struct Camera2D
{
    Mat4x4 projection;
    f32    left;
    f32    right;
    f32    bottom;
    f32    top;
};

Camera2D Camera2DInit(u32 viewportWidth, u32 viewportHeight);
void     Camera2DUpdateProjection(Camera2D* camera, u32 viewportWidth, u32 viewportHeight);