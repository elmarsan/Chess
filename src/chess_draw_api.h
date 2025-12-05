#pragma once

struct Image;

struct Texture
{
    u32 id;
    u32 width;
    u32 height;
};

struct MeshVertex
{
    Vec3 position;
    Vec2 uv;
    Vec3 normal;
    Vec3 tangent;
};

struct Mesh
{
    u32  vertexCount;
    u32  indicesCount;
    s32  parentIndex;
    Vec3 translate;
    Vec3 rotate;
    Vec3 scale;
    u32  VAO;
    u32  VBO;
    u32  IBO;
};

struct Material
{
    Texture albedo;
    Texture normalMap;
    Texture armMap; // r:ao g:roughness b:metallic
};

struct Light
{
    Vec4 position;
    Vec3 color;
};

#define DRAW_INIT(name) void name(u32 windowWidth, u32 windowHeight)
typedef DRAW_INIT(DrawInitFunc);

#define DRAW_DESTROY(name) void name()
typedef DRAW_DESTROY(DrawDestroyFunc);

#define DRAW_BEGIN(name) void name(u32 windowWidth, u32 windowHeight)
typedef DRAW_BEGIN(DrawBeginFunc);

#define DRAW_END(name) void name()
typedef DRAW_END(DrawEndFunc);

#define DRAW_BEGIN_3D(name) void name(Camera3D* camera)
typedef DRAW_BEGIN_3D(DrawBegin3DFunc);

#define DRAW_END_3D(name) void name()
typedef DRAW_END_3D(DrawEnd3DFunc);

#define DRAW_BEGIN_2D(name) void name(Camera2D* camera)
typedef DRAW_BEGIN_2D(DrawBegin2DFunc);

#define DRAW_END_2D(name) void name()
typedef DRAW_END_2D(DrawEnd2DFunc);

#define DRAW_PLANE_3D(name) void name(Mat4x4 model, Vec4 color)
typedef DRAW_PLANE_3D(DrawPlane3DFunc);

#define DRAW_PLANE_TEXTURE_3D(name) void name(Mat4x4 model, Vec4 color, Texture texture, Rect textureRect)
typedef DRAW_PLANE_TEXTURE_3D(DrawPlaneTexture3DFunc);

#define DRAW_MESH_GPU_UPLOAD(name) void name(Mesh* mesh, MeshVertex* vertexs, u32* indices)
typedef DRAW_MESH_GPU_UPLOAD(DrawMeshGPUUploadFunc);

#define DRAW_MESH(name) void name(Mesh* mesh, Mat4x4 model, u32 objectId, Material material)
typedef DRAW_MESH(DrawMeshFunc);

#define DRAW_GET_OBJECT_AT_PIXEL(name) s32 name(u32 x, u32 y)
typedef DRAW_GET_OBJECT_AT_PIXEL(DrawGetObjectAtPixelFunc);

#define DRAW_TEXT(name) void name(const char* text, f32 x, f32 y, Vec4 color)
typedef DRAW_TEXT(DrawTextFunc);

#define DRAW_TEXT_GET_SIZE(name) Vec2 name(const char* text)
typedef DRAW_TEXT_GET_SIZE(DrawTextGetSizeFunc);

#define DRAW_RECT(name) void name(Rect rect, Vec4 color)
typedef DRAW_RECT(DrawRectFunc);

#define DRAW_RECT_TEXTURE(name) void name(Rect rect, Rect textureRect, Texture texture, Vec4 tintColor)
typedef DRAW_RECT_TEXTURE(DrawRectTextureFunc);

#define DRAW_TEXTURE_CREATE(name) Texture name(Image* image)
typedef DRAW_TEXTURE_CREATE(DrawTextureCreateFunc);

#define DRAW_LIGHT_ADD(name) void name(Light light)
typedef DRAW_LIGHT_ADD(DrawLightAddFunc);

#define DRAW_BEGIN_PASS_PICKING(name) void name()
typedef DRAW_BEGIN_PASS_PICKING(DrawBeginPassPicking);

#define DRAW_END_PASS_PICKING(name) void name()
typedef DRAW_END_PASS_PICKING(DrawEndPassPicking);

#define DRAW_BEGIN_PASS_SHADOW(name) void name(Mat4x4 lightProj, Mat4x4 lightView)
typedef DRAW_BEGIN_PASS_SHADOW(DrawBeginPassShadowFunc);

#define DRAW_END_PASS_SHADOW(name) void name()
typedef DRAW_END_PASS_SHADOW(DrawEndPassShadowFunc);

#define DRAW_BEGIN_PASS_RENDER(name) void name()
typedef DRAW_BEGIN_PASS_RENDER(DrawBeginPassRenderFunc);

#define DRAW_END_PASS_RENDER(name) void name()
typedef DRAW_END_PASS_RENDER(DrawEndPassRenderFunc);

#define DRAW_ENVIRONMENT_SET_HDR_MAP(name) void name(Texture hdrTexture)
typedef DRAW_ENVIRONMENT_SET_HDR_MAP(DrawEnvironmentSetHDRMapFunc);

#define DRAW_VSYNC(name) void name(bool enabled)
typedef DRAW_VSYNC(DrawVsyncFunc);

struct DrawAPI
{
    DrawInitFunc*                 Init;
    DrawDestroyFunc*              Destroy;
    DrawBeginFunc*                Begin;
    DrawEndFunc*                  End;
    DrawBegin3DFunc*              Begin3D;
    DrawEnd3DFunc*                End3D;
    DrawBegin2DFunc*              Begin2D;
    DrawEnd2DFunc*                End2D;
    DrawPlane3DFunc*              Plane3D;
    DrawPlaneTexture3DFunc*       PlaneTexture3D;
    DrawMeshFunc*                 Mesh;
    DrawMeshGPUUploadFunc*        MeshGPUUpload;
    DrawGetObjectAtPixelFunc*     GetObjectAtPixel;
    DrawTextFunc*                 Text;
    DrawTextGetSizeFunc*          TextGetSize;
    DrawRectFunc*                 Rect;
    DrawRectTextureFunc*          RectTexture;
    DrawTextureCreateFunc*        TextureCreate;
    DrawLightAddFunc*             LightAdd;
    DrawBeginPassPicking*         BeginPassPicking;
    DrawEndPassPicking*           EndPassPicking;
    DrawBeginPassShadowFunc*      BeginPassShadow;
    DrawEndPassShadowFunc*        EndPassShadow;
    DrawBeginPassRenderFunc*      BeginPassRender;
    DrawEndPassRenderFunc*        EndPassRender;
    DrawEnvironmentSetHDRMapFunc* EnvironmentSetHDRMap;
    DrawVsyncFunc*                Vsync;
};

DrawAPI DrawApiCreate();