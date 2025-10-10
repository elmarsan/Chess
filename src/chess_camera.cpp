#include "chess_camera.h"

internal void CameraUpdateVectors(Camera* camera)
{
    camera->target.x = cosf(DEGTORAD(camera->yaw)) * cosf(DEGTORAD(camera->pitch));
    camera->target.y = sinf(DEGTORAD(camera->pitch));
    camera->target.z = sinf(DEGTORAD(camera->yaw)) * cosf(DEGTORAD(camera->pitch));
    camera->target   = Norm(camera->target);

    Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
    Vec3 right   = Norm(Cross(camera->target, worldUp));
    Vec3 up      = Norm(Cross(right, camera->target));

    camera->up = up;

    camera->view = LookAt(camera->position, camera->position + camera->target, camera->up);
}

void CameraMoveForward(Camera* camera, f32 delta, f32 velocity)
{
    CHESS_ASSERT(camera);
    f32 cameraSpeed = delta * velocity;
    camera->position += camera->target * cameraSpeed;
}
void CameraMoveBackward(Camera* camera, f32 delta, f32 velocity)
{
    CHESS_ASSERT(camera);
    f32 cameraSpeed = delta * velocity;
    camera->position -= camera->target * cameraSpeed;
}
void CameraMoveLeft(Camera* camera, f32 delta, f32 velocity)
{
    CHESS_ASSERT(camera);
    f32 cameraSpeed = delta * velocity;

    Vec3 right = Norm(Cross(camera->target, camera->up));
    camera->position -= right * cameraSpeed;
}
void CameraMoveRight(Camera* camera, f32 delta, f32 velocity)
{
    CHESS_ASSERT(camera);
    f32 cameraSpeed = delta * velocity;
    camera->position += Norm(Cross(camera->target, camera->up)) * cameraSpeed;

    Vec3 right = Norm(Cross(camera->target, camera->up));
    camera->position += right * cameraSpeed;
}

void CameraSetPitch(Camera* camera, f32 pitch)
{
    CHESS_ASSERT(camera);

    if (pitch > 89.0f)
    {
        pitch = 89.0f;
    }
    if (pitch < -89.0f)
    {
        pitch = -89.0f;
    }
    camera->pitch = pitch;
    CameraUpdateVectors(camera);
}

void CameraSetYaw(Camera* camera, f32 yaw)
{
    CHESS_ASSERT(camera);
    camera->yaw = yaw;
    CameraUpdateVectors(camera);
}

Camera CameraInit(Vec3 position, Vec3 front, Vec3 up, f32 pitch, f32 yaw, f32 fov, f32 distance)
{
    Camera result;
    result.position = position;
    result.target   = front;
    result.pitch    = pitch;
    result.yaw      = yaw;
    result.fov      = fov;
    result.distance = distance;

    CameraUpdateVectors(&result);

    return result;
}

void CameraUpdateProjection(Camera* camera, u32 width, u32 height, f32 zNear = 0.1, f32 zFar = 100.0f)
{
    CHESS_ASSERT(camera);

    f32 aspectRatio    = (f32)width / (f32)height;
    camera->projection = Identity();
    camera->projection = Perspective(DEGTORAD(camera->fov), aspectRatio, zNear, zFar);
}