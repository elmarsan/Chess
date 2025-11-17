#include "chess_camera.h"

chess_internal void Camera3DUpdateVectors(Camera3D* camera)
{
    camera->target.x = cosf(DEGTORAD(camera->yaw)) * cosf(DEGTORAD(camera->pitch));
    camera->target.y = sinf(DEGTORAD(camera->pitch));
    camera->target.z = sinf(DEGTORAD(camera->yaw)) * cosf(DEGTORAD(camera->pitch));
    camera->target   = Norm(camera->target);

    Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
    Vec3 right   = Norm(Cross(camera->target, worldUp));
    Vec3 up      = Norm(Cross(right, camera->target));

    camera->up   = up;
    camera->view = LookAt(camera->position, camera->position + camera->target, camera->up);
}

void Camera3DSetPitch(Camera3D* camera, f32 pitch)
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
    Camera3DUpdateVectors(camera);
}

void Camera3DSetYaw(Camera3D* camera, f32 yaw)
{
    CHESS_ASSERT(camera);
    camera->yaw = yaw;
    Camera3DUpdateVectors(camera);
}

Camera3D Camera3DInit(Vec3 position, Vec3 target, Vec3 up, f32 pitch, f32 yaw, f32 fov)
{
    Camera3D result;
    result.position = position;
    result.target   = target;
    result.pitch    = pitch;
    result.yaw      = yaw;
    result.fov      = fov;

    Camera3DUpdateVectors(&result);

    return result;
}

void Camera3DUpdateProjection(Camera3D* camera, u32 width, u32 height, f32 zNear = 0.1, f32 zFar = 100.0f)
{
    CHESS_ASSERT(camera);

    f32 aspectRatio    = (f32)width / (f32)height;
    camera->projection = Identity();
    camera->projection = Perspective(DEGTORAD(camera->fov), aspectRatio, zNear, zFar);
}

Camera2D Camera2DInit(u32 viewportWidth, u32 viewportHeight)
{
    Camera2D result;
    result.left   = 0.0f;
    result.right  = (f32)viewportWidth;
    result.bottom = (f32)viewportHeight;
    result.top    = 0.0f;

    return result;
}

void Camera2DUpdateProjection(Camera2D* camera, u32 viewportWidth, u32 viewportHeight)
{
    CHESS_ASSERT(camera);

    camera->right  = (f32)viewportWidth;
    camera->bottom = (f32)viewportHeight;

    camera->projection = Orthographic(camera->left, camera->right, camera->bottom, camera->top);
}