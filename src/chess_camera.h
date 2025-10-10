#pragma once

struct Camera
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

Camera CameraInit(Vec3 position, Vec3 front, Vec3 up, f32 pitch, f32 yaw, f32 fov, f32 distance);

void CameraUpdateProjection(Camera* camera, u32 width, u32 height, f32 zNear, f32 zFar);

void CameraMoveForward(Camera* camera, f32 delta, f32 velocity);
void CameraMoveBackward(Camera* camera, f32 delta, f32 velocity);
void CameraMoveLeft(Camera* camera, f32 delta, f32 velocity);
void CameraMoveRight(Camera* camera, f32 delta, f32 velocity);

void CameraSetPitch(Camera* camera, f32 pitch);
void CameraSetYaw(Camera* camera, f32 yaw);