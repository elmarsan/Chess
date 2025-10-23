#if CHESS_PLATFORM_WINDOWS
#include "win32_chess.h"

#include <windows.h>
#include <gl/GL.h>
#include <gl/wglext.h>

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
PFNGLUNIFORM1UIPROC              glUniform1ui;
PFNGLUNIFORM4FVPROC              glUniform4fv;
PFNGLDEBUGMESSAGECALLBACKPROC    glDebugMessageCallback;
PFNGLDEBUGMESSAGECONTROLPROC     glDebugMessageControl;
PFNGLGENFRAMEBUFFERSPROC         glGenFramebuffers;
PFNGLBINDFRAMEBUFFERPROC         glBindFramebuffer;
PFNGLCHECKFRAMEBUFFERSTATUSPROC  glCheckFramebufferStatus;
PFNGLFRAMEBUFFERTEXTURE2DPROC    glFramebufferTexture2D;

void APIENTRY OpenGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                                  const GLchar* message, const void* userParam)
{
    if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        CHESS_LOG("------------------------------------------------------------");
        CHESS_LOG("OpenGL debug message: %s", message);
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
    CHESS_LOG("OpenGL version %d.%d", major, minor);

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
    GL_PROC_ADDRESS(glUniform1ui);
    GL_PROC_ADDRESS(glUniform4fv);
    GL_PROC_ADDRESS(glGenFramebuffers);
    GL_PROC_ADDRESS(glBindFramebuffer);
    GL_PROC_ADDRESS(glCheckFramebufferStatus);
    GL_PROC_ADDRESS(glFramebufferTexture2D);	
#endif

    s32 contextFlags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &contextFlags);
    if (contextFlags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
#if CHESS_BUILD_RELEASE
        CHESS_LOG("OpenGL release build must not use opengl debug capabilities");
        CHESS_ASSERT(0);
#endif

        GL_PROC_ADDRESS(glDebugMessageCallback);
        GL_PROC_ADDRESS(glDebugMessageControl);

        CHESS_LOG("OpenGL debug context created");
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

chess_internal GLuint CompileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char infoBuffer[512];
        glGetShaderInfoLog(shader, sizeof(infoBuffer), NULL, infoBuffer);
        CHESS_LOG("OpenGL compiling shader: '%s'", infoBuffer);
        CHESS_ASSERT(0);
    }

    return shader;
}

PROGRAM_BUILD(OpenGLProgramBuild)
{
    Program result;

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

    result.id = glCreateProgram();
    glAttachShader(result.id, vs);
    glAttachShader(result.id, fs);
    glLinkProgram(result.id);

    GLint ok;
    glGetProgramiv(result.id, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char infoBuffer[512];
        glGetProgramInfoLog(result.id, sizeof(infoBuffer), NULL, infoBuffer);
        CHESS_LOG("OpenGL linking program: '%s'", infoBuffer);
        CHESS_ASSERT(0);
    }

    return result;
}

TEXTURE_CREATE(OpenGLTextureCreate)
{
    CHESS_ASSERT(pixels);

    Texture result;

    GLenum format;
    switch (bytesPerPixel)
    {
    case 3:
    {
        format = GL_RGB;
        break;
    }
    case 4:
    {
        format = GL_RGBA;
        break;
    }
    default:
    {
        CHESS_LOG("OpenGL unsupported texture format, bytesPerPixel: '%d'", bytesPerPixel);
        CHESS_ASSERT(0);
        break;
    }
    }

    glGenTextures(1, &result.id);
    glBindTexture(GL_TEXTURE_2D, result.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, (GLsizei)width, (GLsizei)height, 0, format, GL_UNSIGNED_BYTE,
                 (GLvoid*)pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    return result;
}

OpenGL GetOpenGL()
{
    OpenGL result;

    result.glCreateProgram           = glCreateProgram;
    result.glCreateShader            = glCreateShader;
    result.glAttachShader            = glAttachShader;
    result.glDeleteShader            = glDeleteShader;
    result.glLinkProgram             = glLinkProgram;
    result.glDeleteProgram           = glDeleteProgram;
    result.glShaderSource            = glShaderSource;
    result.glUseProgram              = glUseProgram;
    result.glGetShaderiv             = glGetShaderiv;
    result.glGetShaderInfoLog        = glGetShaderInfoLog;
    result.glCompileShader           = glCompileShader;
    result.glGetProgramiv            = glGetProgramiv;
    result.glGetProgramInfoLog       = glGetProgramInfoLog;
    result.glGenBuffers              = glGenBuffers;
    result.glGenVertexArrays         = glGenVertexArrays;
    result.glBindBuffer              = glBindBuffer;
    result.glBindVertexArray         = glBindVertexArray;
    result.glBufferData              = glBufferData;
    result.glBufferSubData           = glBufferSubData;
    result.glDeleteBuffers           = glDeleteBuffers;
    result.glEnableVertexAttribArray = glEnableVertexAttribArray;
    result.glVertexAttribPointer     = glVertexAttribPointer;
    result.glDeleteVertexArrays      = glDeleteVertexArrays;
    result.glBindTexture             = glBindTexture;
    result.glActiveTexture           = glActiveTexture;
    result.glGenerateMipmap          = glGenerateMipmap;
    result.glGetUniformLocation      = glGetUniformLocation;
    result.glUniformMatrix4fv        = glUniformMatrix4fv;
    result.glUniform1i               = glUniform1i;
    result.glUniform1ui              = glUniform1ui;
    result.glUniform4fv              = glUniform4fv;
    result.glDebugMessageCallback    = glDebugMessageCallback;
    result.glDebugMessageControl     = glDebugMessageControl;
    result.glDrawElements            = glDrawElements;
    result.glDrawArrays              = glDrawArrays;
    result.glClear                   = glClear;
    result.glEnable                  = glEnable;
    result.glEnable                  = glEnable;
    result.glGenFramebuffers         = glGenFramebuffers;
    result.glBindFramebuffer         = glBindFramebuffer;
    result.glCheckFramebufferStatus  = glCheckFramebufferStatus;
    result.glFramebufferTexture2D    = glFramebufferTexture2D;
    result.glReadBuffer              = glReadBuffer;
    result.glReadPixels              = glReadPixels;

    result.ProgramBuild  = OpenGLProgramBuild;
    result.TextureCreate = OpenGLTextureCreate;

    return result;
}