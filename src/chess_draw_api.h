#pragma once

#define DRAW_INIT(name) void name()
typedef DRAW_INIT(DrawInitFunc);

#define DRAW_DESTROY(name) void name()
typedef DRAW_DESTROY(DrawDestroyFunc);

#define DRAW_BEGIN(name) void name()
typedef DRAW_BEGIN(DrawBeginFunc);

#define DRAW_END(name) void name()
typedef DRAW_END(DrawEndFunc);

#define DRAW_BEGIN_3D(name) void name(Camera* camera)
typedef DRAW_BEGIN_3D(DrawBegin3DFunc);

#define DRAW_END_3D(name) void name()
typedef DRAW_END_3D(DrawEnd3DFunc);

#define DRAW_PLANE_3D(name) void name(Mat4x4 model, Vec4 color)
typedef DRAW_PLANE_3D(DrawPlane3DFunc);

struct DrawAPI
{
    DrawInitFunc*      Init;
    DrawDestroyFunc*   Destroy;
    DrawBeginFunc*     Begin;
    DrawEndFunc*       End;
    DrawBegin3DFunc*   Begin3D;
    DrawEnd3DFunc*     End3D;
    DrawPlane3DFunc*   Plane3D; 
};

DrawAPI DrawApiCreate();