// ----------------------------------------------------------------------------
// Internal state
internal GLuint activePipeline = 0;
internal GLuint activeVAO      = 0;
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Buffers
GfxVertexBuffer GfxVertexBufferCreate(void* data, u32 size, bool dynamic)
{
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    return (GfxVertexBuffer)vbo;
}

void GfxVertexBufferSetData(GfxVertexBuffer buf, void* data, u32 size)
{
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

GfxIndexBuffer GfxIndexBufferCreate(u32* data, u32 size, bool dynamic)
{
    GLuint ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    return (GfxIndexBuffer)ibo;
}

void GfxVertexBufferBind(GfxVertexBuffer buf) { glBindBuffer(GL_ARRAY_BUFFER, buf); }

void GfxBufferDestroy(GfxVertexBuffer buf)
{
    GLuint id = (GLuint)buf;
    glDeleteBuffers(1, &id);
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Vertex Array
GfxVertexArray GfxVertexArrayCreate()
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    return (GfxVertexArray)vao;
}

void GfxVertexArrayBind(GfxVertexArray vao)
{
    activeVAO = (GLuint)vao;
    glBindVertexArray(activeVAO);
}

void GfxVertexArrayAttribPointer(u32 index, u32 size, u32 stride, void* offset)
{
    glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, offset);
    glEnableVertexAttribArray(index);
}

void GfxVertexArrayDestroy(GfxVertexArray vao) { glDeleteVertexArrays(1, &vao); }
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Textures
GfxTexture GfxTexture2DCreate(u32 width, u32 height, u32 bytesPerPixel, void* pixels)
{
    CHESS_ASSERT(pixels);

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
        CHESS_LOG("[OpenGL] unsupported texture format, bytesPerPixel: '%d'", bytesPerPixel);
        CHESS_ASSERT(0);
        break;
    }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, (GLsizei)width, (GLsizei)height, 0, format, GL_UNSIGNED_BYTE,
                 (GLvoid*)pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    return (GfxTexture)tex;
}

void GfxTextureBind(GfxTexture tex, u32 slot)
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, (GLuint)tex);
}
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Pipelines
internal GLuint CompileShader(GLenum type, const char* src)
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
        CHESS_LOG("[OpenGL] compiling shader: '%s'", infoBuffer);
        CHESS_ASSERT(0);
    }

    return shader;
}

GfxPipeline GfxPipelineCreate(GfxPipelineDesc* desc)
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, desc->vertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, desc->FragmentShader);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char infoBuffer[512];
        glGetProgramInfoLog(prog, sizeof(infoBuffer), NULL, infoBuffer);
        CHESS_LOG("[OpenGL] linking program: '%s'", infoBuffer);
        CHESS_ASSERT(0);
    }

    return (GfxPipeline)prog;
}

void GfxPipelineBind(GfxPipeline pipeline)
{
    activePipeline = (GLuint)pipeline;
    glUseProgram(activePipeline);
}

// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Uniforms
void GfxUniformSetMat4(const char* name, float* mat)
{
    GLint location = glGetUniformLocation(activePipeline, name);
    if (location != -1)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, mat);
    }
}

void GfxUniformSetInt(const char* name, int value)
{
    GLint location = glGetUniformLocation(activePipeline, name);
    if (location != -1)
    {
        glUniform1i(location, value);
    }
}
//  ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Draw Calls
void GfxDrawArrays(u32 vertex_count) { glDrawArrays(GL_TRIANGLES, 0, vertex_count); }

void GfxDrawIndexed(u32 index_count) { glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0); }
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Miscellaneous
void GfxClearColor(f32 r, f32 g, f32 b, f32 a) { glClearColor(r, g, b, a); }

void GfxClear(u32 mask) { glClear(mask); }

void GfxEnableDepthTest() { glEnable(GL_DEPTH_TEST); }
// ----------------------------------------------------------------------------

GfxAPI GfxGetApi()
{
    GfxAPI api = {};
#define GFX_FN(fn) api.fn = fn;
    GFX_API
#undef GFX_FN
    return api;
}