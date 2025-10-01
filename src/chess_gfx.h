#pragma once

#include "chess_types.h"

typedef u32 GfxVertexBuffer;
typedef u32 GfxVertexArray;
typedef u32 GfxTexture;
typedef u32 GfxPipeline;

#define DEPTH_BUFFER_BIT   0x00000100
#define STENCIL_BUFFER_BIT 0x00000400
#define COLOR_BUFFER_BIT   0x00004000

struct GfxPipelineDesc
{
    const char* vertexShader;
    const char* FragmentShader;
};

GfxVertexBuffer GfxVertexBufferCreate(float* data, u32 size, bool dynamic);
GfxVertexBuffer GfxIndexBufferCreate(u32* data, u32 size, bool dynamic);
void            GfxVertexBufferBind(GfxVertexBuffer buf);
void            GfxBufferDestroy(GfxVertexBuffer buf);
GfxVertexArray  GfxVertexArrayCreate();
void            GfxVertexArrayBind(GfxVertexArray vao);
void            GfxVertexArrayAttribPointer(u32 index, u32 size, u32 stride, void* offset);
GfxPipeline     GfxPipelineCreate(GfxPipelineDesc* desc);
void            GfxPipelineBind(GfxPipeline pipeline);
GfxTexture      GfxTexture2DCreate(u32 width, u32 height, u32 bytesPerPixel, void* pixels);
void            GfxTextureBind(GfxTexture tex, u32 slot);
void            GfxUniformSetMat4(const char* name, float* mat);
void            GfxUniformSetInt(const char* name, int value);
void            GfxDrawArrays(u32 vertex_count);
void            GfxDrawIndexed(u32 index_count);
void            GfxClearColor(f32 r, f32 g, f32 b, f32 a);
void            GfxClear(u32 mask);
void            GfxEnableDepthTest();

#define GFX_API                                                                                                        \
    GFX_FN(GfxVertexBufferCreate)                                                                                      \
    GFX_FN(GfxIndexBufferCreate)                                                                                       \
    GFX_FN(GfxVertexBufferBind)                                                                                        \
    GFX_FN(GfxBufferDestroy)                                                                                           \
    GFX_FN(GfxVertexArrayCreate)                                                                                       \
    GFX_FN(GfxVertexArrayBind)                                                                                         \
    GFX_FN(GfxVertexArrayAttribPointer)                                                                                \
    GFX_FN(GfxPipelineCreate)                                                                                          \
    GFX_FN(GfxPipelineBind)                                                                                            \
    GFX_FN(GfxTexture2DCreate)                                                                                         \
    GFX_FN(GfxTextureBind)                                                                                             \
    GFX_FN(GfxUniformSetMat4)                                                                                          \
    GFX_FN(GfxUniformSetInt)                                                                                           \
    GFX_FN(GfxDrawArrays)                                                                                              \
    GFX_FN(GfxDrawIndexed)                                                                                             \
    GFX_FN(GfxClearColor)                                                                                              \
    GFX_FN(GfxClear)                                                                                                   \
    GFX_FN(GfxEnableDepthTest)

struct GfxAPI
{
#define GFX_FN(name) decltype(name)* name;
    GFX_API
#undef GFX_FN
};

GfxAPI GfxGetApi();