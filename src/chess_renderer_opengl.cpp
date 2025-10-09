#include "chess_renderer.h"

#if CHESS_PLATFORM_WINDOWS
#include "win32_chess.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>
#include <gl/wglext.h>
#include <gl/glcorearb.h>

#define GL_PROC_ADDRESS(name) name = (decltype(name))wglGetProcAddress(#name)
#endif

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
PFNGLACTIVETEXTUREPROC           glActiveTexture;
PFNGLGENERATEMIPMAPPROC          glGenerateMipmap;
PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
PFNGLUNIFORM1IPROC               glUniform1i;
PFNGLDEBUGMESSAGECALLBACKPROC    glDebugMessageCallback;
PFNGLDEBUGMESSAGECONTROLPROC     glDebugMessageControl;

void APIENTRY OpenGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                  const GLchar* message, const void* userParam)
{
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        CHESS_LOG("------------------------------------------------------------");
        CHESS_LOG("[OpenGL] debug message: %s", message);
        // clang-format off
        switch (source)
        {
			case GL_DEBUG_SOURCE_API:             CHESS_LOG("Source: API"); break;
			case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   CHESS_LOG("Source: Window System"); break;
			case GL_DEBUG_SOURCE_SHADER_COMPILER: CHESS_LOG("Source: Shader Compiler"); break;
			case GL_DEBUG_SOURCE_THIRD_PARTY:     CHESS_LOG("Source: Third Party"); break;
			case GL_DEBUG_SOURCE_APPLICATION:     CHESS_LOG("Source: Application"); break;
			case GL_DEBUG_SOURCE_OTHER:           CHESS_LOG("Source: Other"); break;
        }        
        switch (type)
        {
			case GL_DEBUG_TYPE_ERROR:               CHESS_LOG("Type: Error"); break;
			case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: CHESS_LOG("Type: Deprecated Behaviour"); break;
			case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  CHESS_LOG("Type: Undefined Behaviour"); break;
			case GL_DEBUG_TYPE_PORTABILITY:         CHESS_LOG("Type: Portability"); break;
			case GL_DEBUG_TYPE_PERFORMANCE:         CHESS_LOG("Type: Performance"); break;
			case GL_DEBUG_TYPE_MARKER:              CHESS_LOG("Type: Marker"); break;
			case GL_DEBUG_TYPE_PUSH_GROUP:          CHESS_LOG("Type: Push Group"); break;
			case GL_DEBUG_TYPE_POP_GROUP:           CHESS_LOG("Type: Pop Group"); break;
			case GL_DEBUG_TYPE_OTHER:               CHESS_LOG("Type: Other"); break;
        }
        CHESS_LOG("------------------------------------------------------------");
        // clang-format on
    }
}

void RendererInit()
{
#if CHESS_PLATFORM_WINDOWS
    PFNWGLCHOOSEPIXELFORMATARBPROC    wglChoosePixelFormatARB;
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

    PIXELFORMATDESCRIPTOR pfd = { 0 };
    pfd.nSize                 = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion              = 1;
    pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType            = PFD_TYPE_RGBA;
    pfd.cColorBits            = 32;
    pfd.cRedBits              = 8;
    pfd.cGreenBits            = 8;
    pfd.cBlueBits             = 8;
    pfd.cAlphaBits            = 8;
    pfd.cDepthBits            = 24;
    pfd.cStencilBits          = 8;
    pfd.iLayerType            = PFD_MAIN_PLANE;

    {
        HWND dummyWND = CreateWindowA(win32State.classname, NULL, WS_POPUP, 0, 0, 0, 0, 0, 0, 0, 0);
        if (!dummyWND)
        {
            Win32LogLastError("CreateWindowA");
            CHESS_ASSERT(0);
        }
        HDC  dummyDC     = GetWindowDC(dummyWND);
        int  pixelFormat = ChoosePixelFormat(dummyDC, &pfd);
        bool result      = SetPixelFormat(dummyDC, pixelFormat, &pfd);
        if (!result)
        {
            Win32LogLastError("SetPixelFormat");
            CHESS_ASSERT(0);
        }

        HGLRC dummyglRC = wglCreateContext(dummyDC);
        wglMakeCurrent(dummyDC, dummyglRC);

        GL_PROC_ADDRESS(wglChoosePixelFormatARB);
        GL_PROC_ADDRESS(wglCreateContextAttribsARB);

        wglMakeCurrent(0, 0);
        ReleaseDC(dummyWND, dummyDC);
        wglDeleteContext(dummyglRC);
        DestroyWindow(dummyWND);
    }

    // clang-format off
    int pixelAttrs[] = 
    {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
        WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,     32,
        WGL_DEPTH_BITS_ARB,     24,
        WGL_STENCIL_BITS_ARB,   8,
        0,
    };
    int glContextAttrs[] = 
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#if CHESS_BUILD_DEBUG
        WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
        0,
    };
    // clang-format on

    int  pixelFormat;
    UINT numFormats;
    wglChoosePixelFormatARB(win32State.deviceContext, pixelAttrs, NULL, 1, &pixelFormat, &numFormats);
    bool result = SetPixelFormat(win32State.deviceContext, pixelFormat, &pfd);
    CHESS_ASSERT(result);

    win32State.glContext = wglCreateContextAttribsARB(win32State.deviceContext, 0, glContextAttrs);
    result               = wglMakeCurrent(win32State.deviceContext, win32State.glContext);
    CHESS_ASSERT(result);

    s32 major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    CHESS_LOG("[OpenGL] version %d.%d", major, minor);

    GL_PROC_ADDRESS(glCreateProgram);
    GL_PROC_ADDRESS(glCreateShader);
    GL_PROC_ADDRESS(glAttachShader);
    GL_PROC_ADDRESS(glDeleteShader);
    GL_PROC_ADDRESS(glLinkProgram);
    GL_PROC_ADDRESS(glDeleteProgram);
    GL_PROC_ADDRESS(glShaderSource);
    GL_PROC_ADDRESS(glUseProgram);
    GL_PROC_ADDRESS(glGetShaderiv);
    GL_PROC_ADDRESS(glGetShaderInfoLog);
    GL_PROC_ADDRESS(glCompileShader);
    GL_PROC_ADDRESS(glGetProgramiv);
    GL_PROC_ADDRESS(glGetProgramInfoLog);
    GL_PROC_ADDRESS(glGenBuffers);
    GL_PROC_ADDRESS(glGenVertexArrays);
    GL_PROC_ADDRESS(glBindBuffer);
    GL_PROC_ADDRESS(glBindVertexArray);
    GL_PROC_ADDRESS(glBufferData);
    GL_PROC_ADDRESS(glBufferSubData);
    GL_PROC_ADDRESS(glDeleteBuffers);
    GL_PROC_ADDRESS(glEnableVertexAttribArray);
    GL_PROC_ADDRESS(glVertexAttribPointer);
    GL_PROC_ADDRESS(glDeleteVertexArrays);
    GL_PROC_ADDRESS(glActiveTexture);
    GL_PROC_ADDRESS(glGenerateMipmap);
    GL_PROC_ADDRESS(glGetUniformLocation);
    GL_PROC_ADDRESS(glUniformMatrix4fv);
    GL_PROC_ADDRESS(glUniform1i);
#endif

    s32 contextFlags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);
    if (contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
#if CHESS_BUILD_RELEASE
        CHESS_LOG("[OpenGL] release build must not use opengl debug capabilities");
        CHESS_ASSERT(0);
#endif

        GL_PROC_ADDRESS(glDebugMessageCallback);
        GL_PROC_ADDRESS(glDebugMessageControl);

        CHESS_LOG("[OpenGL] debug context created");
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(OpenGLDebugCallback, 0);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
    }
}

void RendererDestroy()
{
#if CHESS_PLATFORM_WINDOWS
    wglDeleteContext(win32State.glContext);
    wglMakeCurrent(0, 0);
#endif
}

void RendererSetViewport(u32 x, u32 y, u32 width, u32 height) { glViewport(x, y, width, height); }