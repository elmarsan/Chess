#pragma once

#include <gl/glcorearb.h>

struct Program
{
    u32 id;
};

struct Texture
{
    u32 id;
};

void RendererInit();
void RendererDestroy();
void RendererSetViewport(u32 x, u32 y, u32 width, u32 height);

#define PROGRAM_BUILD(name) Program name(const char* vertexSource, const char* fragmentSource)
typedef PROGRAM_BUILD(ProgramBuildFunc);

#define TEXTURE_CREATE(name) Texture name(u32 width, u32 height, u32 bytesPerPixel, void* pixels)
typedef TEXTURE_CREATE(TextureCreateFunc);

struct OpenGL
{
    ProgramBuildFunc*  ProgramBuild;
    TextureCreateFunc* TextureCreate;

    PFNGLCREATEPROGRAMPROC           glCreateProgram;
    PFNGLCREATESHADERPROC            glCreateShader;
    PFNGLATTACHSHADERPROC            glAttachShader;
    PFNGLDELETESHADERPROC            glDeleteShader;
    PFNGLLINKPROGRAMPROC             glLinkProgram;
    PFNGLDELETEPROGRAMPROC           glDeleteProgram;
    PFNGLSHADERSOURCEPROC            glShaderSource;
    PFNGLUSEPROGRAMPROC              glUseProgram;
    PFNGLGETSHADERIVPROC             glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
    PFNGLCOMPILESHADERPROC           glCompileShader;
    PFNGLGETPROGRAMIVPROC            glGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog;
    PFNGLGENBUFFERSPROC              glGenBuffers;
    PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays;
    PFNGLBINDBUFFERPROC              glBindBuffer;
    PFNGLBINDVERTEXARRAYPROC         glBindVertexArray;
    PFNGLBUFFERDATAPROC              glBufferData;
    PFNGLBUFFERSUBDATAPROC           glBufferSubData;
    PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
    PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays;
    PFNGLBINDTEXTUREPROC             glBindTexture;
    PFNGLACTIVETEXTUREPROC           glActiveTexture;
    PFNGLGENERATEMIPMAPPROC          glGenerateMipmap;
    PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
    PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
    PFNGLUNIFORM1IPROC               glUniform1i;
    PFNGLUNIFORM1UIPROC              glUniform1ui;
    PFNGLDEBUGMESSAGECALLBACKPROC    glDebugMessageCallback;
    PFNGLDEBUGMESSAGECONTROLPROC     glDebugMessageControl;
    PFNGLDRAWELEMENTSPROC            glDrawElements;
    PFNGLDRAWARRAYSPROC              glDrawArrays;
    PFNGLCLEARPROC                   glClear;
    PFNGLENABLEPROC                  glEnable;
    PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus;
    PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;
    PFNGLREADBUFFERPROC              glReadBuffer;
    PFNGLREADPIXELSPROC              glReadPixels;
};

OpenGL GetOpenGL();