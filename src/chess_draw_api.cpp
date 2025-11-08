#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_RECT_COUNT        100
#define MAX_RECT_VERTEX_COUNT MAX_RECT_COUNT * 4
#define MAX_RECT_INDEX_COUNT  MAX_RECT_COUNT * 6

#define MAX_PLANE_COUNT        100
#define MAX_PLANE_VERTEX_COUNT MAX_PLANE_COUNT * 4
#define MAX_PLANE_INDEX_COUNT  MAX_PLANE_COUNT * 6

#define MAX_TEXTURES 8

struct PlaneVertex
{
    Vec3 position;
    Vec2 uv;
    Vec4 color;
    f32  textureIndex;
};

struct Rect2DVertex
{
    Vec2 position;
    Vec2 uv;
    Vec4 color;
    f32  textureIndex;
};

enum
{
    RENDER_PHASE_MOUSE_PICKING,
    RENDER_PHASE_DRAW
};

struct FontCharacter
{
    f32 advanceX;
    f32 advanceY;
    f32 width;
    f32 rows;
    f32 left;
    f32 top;
    f32 textureXOffset;
};

struct Rect2DBatch
{
    u32           count;
    u32           VAO;
    u32           VBO;
    Rect2DVertex* vertexBuffer;
    Rect2DVertex* vertexBufferPtr;
    Program       program;
    s32           viewProjUniform;
};

chess_internal void Rect2DBatchInit(Rect2DBatch* batch, u32 quadIBO);
chess_internal void Rect2DBatchAddRect(Rect2DBatch* batch, Rect rect, Vec4 color, Mat4x4 viewProj, Texture* texture,
                                       Rect textureRect);
chess_internal void Rect2DBatchFlush(Rect2DBatch* batch, Mat4x4 viewProj);
chess_internal void Rect2DBatchClear(Rect2DBatch* batch);

struct RenderData
{
    u32           quadIBO;
    Rect2DBatch   rect2DBatch;
    u32           planeVAO;
    u32           planeVBO;
    PlaneVertex*  planeVertexBuffer;
    PlaneVertex*  planeVertexBufferPtr;
    u32           planeCount;
    Program       planeProgram;
    Camera3D*     camera3D;
    Camera2D*     camera2D;
    Program       meshProgram;
    u32           renderPhase;
    Program       mousePickingProgram;
    u32           mousePickingFBO;
    u32           mousePickingColorTexture;
    u32           mousePickingDepthTexture;
    u32           fontAtlasTexture;
    Vec2U         fontAtlasDimension;
    FontCharacter fontChars[128];
    Vec2U         viewportDimension;
    u32           textures[MAX_TEXTURES];
};

chess_internal RenderData gRenderData;

chess_internal void FlushPlanes();
chess_internal void FreeTypeInit();
chess_internal void UpdateMousePickingFBO();
chess_internal u32  PushTexture(Texture* texture);
chess_internal void BindActiveTextures(GLint samplerArrayLocation);

DRAW_INIT(DrawInitProcedure)
{
    CHESS_LOG("Renderer api: InitDrawing");
    gRenderData.viewportDimension = { windowWidth, windowHeight };

    u32 indices[MAX_PLANE_INDEX_COUNT]{};
    u32 offset = 0;
    for (u32 i = 0; i < MAX_PLANE_INDEX_COUNT; i += 6)
    {
        // First triangle
        indices[i + 0] = 0 + offset;
        indices[i + 1] = 1 + offset;
        indices[i + 2] = 2 + offset;
        // Second triangle
        indices[i + 3] = 2 + offset;
        indices[i + 4] = 3 + offset;
        indices[i + 5] = 0 + offset;

        offset += 4;
    }
    glGenBuffers(1, &gRenderData.quadIBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gRenderData.quadIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    Rect2DBatchInit(&gRenderData.rect2DBatch, gRenderData.quadIBO);

    glGenVertexArrays(1, &gRenderData.planeVAO);
    glGenBuffers(1, &gRenderData.planeVBO);

    glBindVertexArray(gRenderData.planeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gRenderData.planeVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PLANE_VERTEX_COUNT * sizeof(PlaneVertex), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gRenderData.quadIBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, uv));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, color));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, textureIndex));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    gRenderData.planeVertexBuffer    = new PlaneVertex[MAX_PLANE_VERTEX_COUNT];
    gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;

    const char* planeVertexShader   = R"(
		#version 330 core
		layout(location = 0) in vec3  aPos;
		layout(location = 1) in vec2  aUV;
		layout(location = 2) in vec4  aColor;
		layout(location = 3) in float aTextureIndex;
		
		out vec4  color;
		out vec2  uv;
		flat out float textureIndex;

		uniform mat4 view;
		uniform mat4 projection;
		void main()
		{
			color        = aColor;
			uv           = aUV;
			textureIndex = aTextureIndex;
			gl_Position  = projection * view * vec4(aPos, 1.0);
		}
		)";
    const char* planeFragmentShader = R"(
		#version 330 core
		in   vec4  color;
		in   vec2  uv;
		flat in    float textureIndex;
		out  vec4  FragColor;

		uniform sampler2D textures[8];

		void main()
		{
			vec4 texColor = texture(textures[int(textureIndex)], uv);
			FragColor     = texColor * color;
		}
		)";

    gRenderData.planeProgram = OpenGLProgramBuild(planeVertexShader, planeFragmentShader);

    const char* meshVertexSource   = R"(
		#version 330
		layout(location = 0) in vec3 aPos;
		layout(location = 1) in vec2 aUV;
		layout(location = 2) in vec3 aNormal;
			
		out vec2 UV;
			
		uniform mat4 model;
		uniform mat4 view;
		uniform mat4 projection;
		void main()
		{
			UV = aUV;
			gl_Position = projection * view * model * vec4(aPos, 1.0);
		}
		)";
    const char* meshFragmentSource = R"(
		#version 330
		out vec4 FragColor;
		in vec2 UV;
		uniform sampler2D albedo;
		// TODO: Lightning and normal mapping
		// uniform sampler2D normal;
		uniform vec4 tintColor;

		void main()
		{
			FragColor = texture(albedo, UV) * tintColor;
		}
		)";

    gRenderData.meshProgram = OpenGLProgramBuild(meshVertexSource, meshFragmentSource);

    // ----------------------------------------------------------------------------
    // Mouse picking
    const char* mousePickingVertexSource = R"(
		#version 330
		layout (location = 0) in vec3 aPos;

		uniform mat4 model;
		uniform mat4 view;
		uniform mat4 projection;

		void main()
		{
			gl_Position = projection * view * model * vec4(aPos, 1.0);
		}
		)";

    const char* mousePickingFragmentSource = R"(
		#version 330
		uniform uint objectId;
		out uint FragColor;

		void main()
		{
			FragColor = objectId;
		}
		)";

    gRenderData.mousePickingProgram = OpenGLProgramBuild(mousePickingVertexSource, mousePickingFragmentSource);

    UpdateMousePickingFBO();

    FreeTypeInit();
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);

    u32 white = 0xFFFFFFFF;
    glGenTextures(1, &gRenderData.textures[0]);
    glBindTexture(GL_TEXTURE_2D, gRenderData.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    for (u32 textureIndex = 1; textureIndex < ARRAY_COUNT(gRenderData.textures); textureIndex++)
    {
        gRenderData.textures[textureIndex] = -1;
    }
}

DRAW_DESTROY(DrawDestroyProcedure) { delete[] gRenderData.planeVertexBuffer; }

DRAW_BEGIN(DrawBeginProcedure)
{
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (gRenderData.viewportDimension != Vec2U{ windowWidth, windowHeight })
    {
        gRenderData.viewportDimension = { windowWidth, windowHeight };
        UpdateMousePickingFBO();
    }

    gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;
    gRenderData.planeCount           = 0;

    Rect2DBatchClear(&gRenderData.rect2DBatch);

    for (u32 textureIndex = 1; textureIndex < ARRAY_COUNT(gRenderData.textures); textureIndex++)
    {
        gRenderData.textures[textureIndex] = -1;
    }
}

DRAW_END(DrawEndProcedure)
{
    FlushPlanes();
    Rect2DBatchFlush(&gRenderData.rect2DBatch, gRenderData.camera2D->projection);
}

DRAW_BEGIN_3D(DrawBegin3D)
{
    CHESS_ASSERT(camera);
    gRenderData.camera3D = camera;
}

DRAW_END_3D(DrawEnd3D)
{
    //
}

DRAW_BEGIN_2D(DrawBegin2DProcedure)
{
    CHESS_ASSERT(camera);
    gRenderData.camera2D = camera;
}

DRAW_END_2D(DrawEnd2DProcedure)
{
    //
}

DRAW_PLANE_3D(DrawPlane3DProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    u32 planeVertexCount = gRenderData.planeCount * 4;
    if (planeVertexCount >= MAX_PLANE_VERTEX_COUNT)
    {
        FlushPlanes();
    }

    Vec3 planeVertices[4] = {
        { 1.0f, 0.0f, 1.0f },   // top-right
        { -1.0f, 0.0f, 1.0f },  // top-left
        { -1.0f, 0.0f, -1.0f }, // bottom-left
        { 1.0f, 0.0f, -1.0f }   // bottom-right
    };

    for (u32 i = 0; i < 4; ++i)
    {
        // Transform local vertex to world space using the model matrix
        Vec4 localPos  = { planeVertices[i].x, planeVertices[i].y, planeVertices[i].z, 1.0f };
        Vec4 worldPos4 = model * localPos;
        Vec3 worldPos3 = { worldPos4.x, worldPos4.y, worldPos4.z };

        gRenderData.planeVertexBufferPtr->position     = worldPos3;
        gRenderData.planeVertexBufferPtr->color        = color;
        gRenderData.planeVertexBufferPtr->textureIndex = 0.0f;
        gRenderData.planeVertexBufferPtr++;
    }

    gRenderData.planeCount++;
}

DRAW_PLANE_TEXTURE_3D(DrawPlaneTexture3DProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    u32 planeVertexCount = gRenderData.planeCount * 4;
    if (planeVertexCount >= MAX_PLANE_VERTEX_COUNT)
    {
        FlushPlanes();
    }

    f32 textureIndex = PushTexture(&texture);

    Vec3 planeVertices[4] = {
        { 1.0f, 0.0f, 1.0f },   // top-right
        { -1.0f, 0.0f, 1.0f },  // top-left
        { -1.0f, 0.0f, -1.0f }, // bottom-left
        { 1.0f, 0.0f, -1.0f }   // bottom-right
    };

    f32 x = textureRect.x;
    f32 y = textureRect.y;
    f32 w = textureRect.x + textureRect.w;
    f32 h = textureRect.y + textureRect.h;

    textureRect.x = x / texture.width;
    textureRect.y = y / texture.height;
    textureRect.w = w / texture.width;
    textureRect.h = h / texture.height;

    Vec2 uvs[4] = {
        { textureRect.w, textureRect.h }, // bottom-right
        { textureRect.x, textureRect.h }, // bottom-left
        { textureRect.x, textureRect.y }, // top-left
        { textureRect.w, textureRect.y }  // top-right
    };

    for (u32 i = 0; i < 4; ++i)
    {
        // Transform local vertex to world space using the model matrix
        Vec4 localPos  = { planeVertices[i].x, planeVertices[i].y, planeVertices[i].z, 1.0f };
        Vec4 worldPos4 = model * localPos;
        Vec3 worldPos3 = { worldPos4.x, worldPos4.y, worldPos4.z };

        gRenderData.planeVertexBufferPtr->position     = worldPos3;
        gRenderData.planeVertexBufferPtr->color        = color;
        gRenderData.planeVertexBufferPtr->uv           = uvs[i];
        gRenderData.planeVertexBufferPtr->textureIndex = textureIndex;
        gRenderData.planeVertexBufferPtr++;
    }

    gRenderData.planeCount++;
}

DRAW_MESH(DrawMeshProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    if (gRenderData.renderPhase == RENDER_PHASE_DRAW)
    {
        glUseProgram(gRenderData.meshProgram.id);

        glUniformMatrix4fv(glGetUniformLocation(gRenderData.meshProgram.id, "model"), 1, GL_FALSE, &model.e[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(gRenderData.meshProgram.id, "view"), 1, GL_FALSE,
                           &gRenderData.camera3D->view.e[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(gRenderData.meshProgram.id, "projection"), 1, GL_FALSE,
                           &gRenderData.camera3D->projection.e[0][0]);

        glUniform4fv(glGetUniformLocation(gRenderData.meshProgram.id, "tintColor"), 1, &tintColor.x);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else if (gRenderData.renderPhase == RENDER_PHASE_MOUSE_PICKING)
    {
        glUseProgram(gRenderData.mousePickingProgram.id);

        glUniformMatrix4fv(glGetUniformLocation(gRenderData.mousePickingProgram.id, "model"), 1, GL_FALSE,
                           &model.e[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(gRenderData.mousePickingProgram.id, "view"), 1, GL_FALSE,
                           &gRenderData.camera3D->view.e[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(gRenderData.mousePickingProgram.id, "projection"), 1, GL_FALSE,
                           &gRenderData.camera3D->projection.e[0][0]);
        glUniform1ui(glGetUniformLocation(gRenderData.mousePickingProgram.id, "objectId"), objectId + 1);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else
    {
        CHESS_ASSERT(0);
    }
}

DRAW_BEGIN_MOUSE_PICKING(DrawBeginMousePickingProcedure)
{
    gRenderData.renderPhase = RENDER_PHASE_MOUSE_PICKING;
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.mousePickingFBO);
    glDisable(GL_DITHER);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

DRAW_END_MOUSE_PICKING(DrawEndMousePickingProcedure)
{
    gRenderData.renderPhase = RENDER_PHASE_DRAW;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DITHER);
}

DRAW_GET_OBJECT_AT_PIXEL(DrawGetObjectAtPixelProcedure)
{
    u32 objectId;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gRenderData.mousePickingFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &objectId);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    // Not found
    if (objectId != 0)
    {
        return objectId - 1;
    }

    return -1;
}

DRAW_TEXT(DrawTextProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    for (size_t i = 0; i < strlen(text); i++)
    {
        FontCharacter fontChar = gRenderData.fontChars[text[i]];

        Rect rect;
        rect.x = x + fontChar.left;
        rect.y = y - fontChar.top;
        rect.w = fontChar.width;
        rect.h = fontChar.rows;

        Rect textureRect;
        textureRect.x = (f32)fontChar.textureXOffset / (f32)gRenderData.fontAtlasDimension.w;
        textureRect.y = 0.0f;
        textureRect.w = (f32)(fontChar.textureXOffset + fontChar.width) / (f32)gRenderData.fontAtlasDimension.w;
        textureRect.h = (f32)fontChar.rows / (f32)gRenderData.fontAtlasDimension.h;

        Texture texture;
        texture.id = gRenderData.fontAtlasTexture;

        Mat4x4 viewProj = gRenderData.camera2D->projection;
        Rect2DBatchAddRect(&gRenderData.rect2DBatch, rect, color, viewProj, &texture, textureRect);

        x += fontChar.advanceX;
    }
}

DRAW_TEXT_GET_SIZE(DrawTextGetSizeProcedure)
{
    Vec2 size{ 0.0f };

    for (size_t i = 0; i < strlen(text); i++)
    {
        FontCharacter fontChar = gRenderData.fontChars[text[i]];
        size.w += fontChar.advanceX;
        size.h = Max(size.h, fontChar.rows);
    }

    return size;
}

DRAW_RECT(DrawRectProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    Mat4x4 viewProj = gRenderData.camera2D->projection;
    Rect2DBatchAddRect(&gRenderData.rect2DBatch, rect, color, viewProj, nullptr, Rect{});
}

DRAW_RECT_TEXTURE(DrawRectTextureProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    Mat4x4 viewProj = gRenderData.camera2D->projection;

    float x = textureRect.x;
    float y = textureRect.y;
    float w = textureRect.x + textureRect.w;
    float h = textureRect.y + textureRect.h;

    textureRect.x = x / texture.width;
    textureRect.y = y / texture.height;
    textureRect.w = w / texture.width;
    textureRect.h = h / texture.height;

    Rect2DBatchAddRect(&gRenderData.rect2DBatch, rect, tintColor, viewProj, &texture, textureRect);
}

DrawAPI DrawApiCreate()
{
    DrawAPI result;

    result.Init              = DrawInitProcedure;
    result.Destroy           = DrawDestroyProcedure;
    result.Begin             = DrawBeginProcedure;
    result.End               = DrawEndProcedure;
    result.Begin3D           = DrawBegin3D;
    result.End3D             = DrawEnd3D;
    result.Plane3D           = DrawPlane3DProcedure;
    result.PlaneTexture3D    = DrawPlaneTexture3DProcedure;
    result.Mesh              = DrawMeshProcedure;
    result.BeginMousePicking = DrawBeginMousePickingProcedure;
    result.EndMousePicking   = DrawEndMousePickingProcedure;
    result.GetObjectAtPixel  = DrawGetObjectAtPixelProcedure;
    result.Text              = DrawTextProcedure;
    result.TextGetSize       = DrawTextGetSizeProcedure;
    result.Begin2D           = DrawBegin2DProcedure;
    result.End2D             = DrawEnd2DProcedure;
    result.Rect              = DrawRectProcedure;
    result.RectTexture       = DrawRectTextureProcedure;

    return result;
}

chess_internal void UpdateMousePickingFBO()
{
    if (glIsFramebuffer(gRenderData.mousePickingFBO) == GL_TRUE)
    {
        CHESS_LOG("Updating mouse picking framebuffer...");

        GLuint textures[2] = { gRenderData.mousePickingColorTexture, gRenderData.mousePickingDepthTexture };
        glDeleteTextures(2, textures);
        glDeleteFramebuffers(1, &gRenderData.mousePickingFBO);
    }
    else
    {
        CHESS_LOG("Creating mouse picking framebuffer...");
    }

    u32 width  = gRenderData.viewportDimension.w;
    u32 height = gRenderData.viewportDimension.h;

    glGenFramebuffers(1, &gRenderData.mousePickingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.mousePickingFBO);

    glGenTextures(1, &gRenderData.mousePickingColorTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.mousePickingColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gRenderData.mousePickingColorTexture,
                           0);

    glGenTextures(1, &gRenderData.mousePickingDepthTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.mousePickingDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRenderData.mousePickingDepthTexture, 0);

    GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        CHESS_LOG("OpenGL Framebuffer error, status: 0x%x", framebufferStatus);
        CHESS_ASSERT(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

chess_internal void FlushPlanes()
{
    if (gRenderData.planeCount > 0)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        u32 vertexCount = gRenderData.planeCount * 4;
        u32 indexCount  = gRenderData.planeCount * 6;

        glUseProgram(gRenderData.planeProgram.id);

        GLint samplerArrayLoc = glGetUniformLocation(gRenderData.planeProgram.id, "textures");
        CHESS_ASSERT(samplerArrayLoc != -1);
        BindActiveTextures(samplerArrayLoc);

        glUniformMatrix4fv(glGetUniformLocation(gRenderData.planeProgram.id, "view"), 1, GL_FALSE,
                           &gRenderData.camera3D->view.e[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(gRenderData.planeProgram.id, "projection"), 1, GL_FALSE,
                           &gRenderData.camera3D->projection.e[0][0]);

        glBindBuffer(GL_ARRAY_BUFFER, gRenderData.planeVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(PlaneVertex), gRenderData.planeVertexBuffer);
        glBindVertexArray(gRenderData.planeVAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;
        gRenderData.planeCount           = 0;

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

chess_internal void FreeTypeInit()
{
    FT_Library library;
    FT_Face    face;

    if (FT_Init_FreeType(&library))
    {
        CHESS_LOG("[FreeType] error on FT_Init_FreeType");
        CHESS_ASSERT(0);
    }

    if (FT_New_Face(library, "../data/DroidSans.ttf", 0, &face))
    {
        CHESS_LOG("[FreeType] error loading font");
        CHESS_ASSERT(0);
    }

    FT_Set_Pixel_Sizes(face, 0, 24);

    for (u32 charIndex = 32; charIndex < 128; charIndex++)
    {
        if (FT_Load_Char(face, charIndex, FT_LOAD_RENDER))
        {
            CHESS_LOG("[FreeType] failed to load glyph for ASCII: %d", charIndex);
            continue;
        }

        FT_GlyphSlot glyph = face->glyph;

        gRenderData.fontAtlasDimension.w += glyph->bitmap.width;
        gRenderData.fontAtlasDimension.h = Max(gRenderData.fontAtlasDimension.h, glyph->bitmap.rows);
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &gRenderData.fontAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.fontAtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gRenderData.fontAtlasDimension.w, gRenderData.fontAtlasDimension.h, 0,
                 GL_RED, GL_UNSIGNED_BYTE, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLint swizzleMask[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    s32 xOffset = 0;
    for (u32 charIndex = 32; charIndex < 128; charIndex++)
    {
        if (FT_Load_Char(face, charIndex, FT_LOAD_RENDER))
        {
            continue;
        }

        FT_GlyphSlot glyph  = face->glyph;
        s32          width  = glyph->bitmap.width;
        s32          height = glyph->bitmap.rows;

        FontCharacter* c  = &gRenderData.fontChars[charIndex];
        c->advanceX       = glyph->advance.x >> 6;
        c->advanceY       = glyph->advance.y >> 6;
        c->width          = width;
        c->rows           = height;
        c->left           = glyph->bitmap_left;
        c->top            = glyph->bitmap_top;
        c->textureXOffset = xOffset;

        glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
        xOffset += width;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(library);
}

chess_internal u32 PushTexture(Texture* texture)
{
    CHESS_ASSERT(texture);

    u32 textureIndex = 0.0f;

    for (u32 i = 0; i < ARRAY_COUNT(gRenderData.textures); i++)
    {
        if (gRenderData.textures[i] == texture->id)
        {
            textureIndex = i;
            break;
        }
        // Empty slot
        if (gRenderData.textures[i] == -1)
        {
            gRenderData.textures[i] = texture->id;
            textureIndex            = i;
            break;
        }
    }

    return textureIndex;
}

chess_internal void BindActiveTextures(GLint samplerArrayLocation)
{
    for (int i = 0; i < ARRAY_COUNT(gRenderData.textures); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        if (gRenderData.textures[i] != -1)
        {
            glBindTexture(GL_TEXTURE_2D, gRenderData.textures[i]);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, gRenderData.textures[0]);
        }
    }

    constexpr int textureUnits[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    glUniform1iv(samplerArrayLocation, ARRAY_COUNT(gRenderData.textures), textureUnits);
}

chess_internal void Rect2DBatchInit(Rect2DBatch* batch, u32 quadIBO)
{
    glGenVertexArrays(1, &batch->VAO);
    glGenBuffers(1, &batch->VBO);

    glBindVertexArray(batch->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, batch->VBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_RECT_VERTEX_COUNT * sizeof(Rect2DVertex), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIBO);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Rect2DVertex), (void*)offsetof(Rect2DVertex, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Rect2DVertex), (void*)offsetof(Rect2DVertex, uv));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Rect2DVertex), (void*)offsetof(Rect2DVertex, color));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Rect2DVertex), (void*)offsetof(Rect2DVertex, textureIndex));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    batch->vertexBuffer    = new Rect2DVertex[MAX_RECT_VERTEX_COUNT];
    batch->vertexBufferPtr = batch->vertexBuffer;

    const char* vertexSource = R"(
		#version 330
		layout (location = 0) in vec2  aPos;
		layout (location = 1) in vec2  aUV;
		layout (location = 2) in vec4  aColor;
		layout (location = 3) in float aTextureIndex;
		
		out vec4  color;
		out vec2  uv;
		out float textureIndex;

		uniform mat4 viewProj;

		void main()
		{
			color        = aColor;
			uv           = aUV;
			textureIndex = aTextureIndex;
			gl_Position  = viewProj * vec4(aPos, 0.0, 1.0);
		}
		)";

    const char* fragmentSource = R"(
		#version 330
		
		in  vec4  color;
		in  vec2  uv;
		in  float textureIndex;
		out vec4  FragColor;

		uniform sampler2D textures[8];

		void main()
		{
			vec4 texColor = texture(textures[int(textureIndex)], uv);
			FragColor     = texColor * color;
		}
		)";

    batch->program         = OpenGLProgramBuild(vertexSource, fragmentSource);
    batch->viewProjUniform = glGetUniformLocation(batch->program.id, "viewProj");
}

chess_internal void Rect2DBatchAddRect(Rect2DBatch* batch, Rect rect, Vec4 color, Mat4x4 viewProj, Texture* texture,
                                       Rect textureRect)
{
    u32 vertexCount = batch->count * 4;
    if (vertexCount >= MAX_RECT_VERTEX_COUNT)
    {
        Rect2DBatchFlush(batch, viewProj);
    }

    f32 textureIndex = 0.0f;

    if (texture)
    {
        textureIndex = (f32)PushTexture(texture);
    }

    // Top-right
    batch->vertexBufferPtr->position     = { rect.x + rect.w, rect.y };
    batch->vertexBufferPtr->uv           = { textureRect.w, textureRect.y };
    batch->vertexBufferPtr->color        = color;
    batch->vertexBufferPtr->textureIndex = textureIndex;
    batch->vertexBufferPtr++;

    // Top-left
    batch->vertexBufferPtr->position     = { rect.x, rect.y };
    batch->vertexBufferPtr->uv           = { textureRect.x, textureRect.y };
    batch->vertexBufferPtr->color        = color;
    batch->vertexBufferPtr->textureIndex = textureIndex;
    batch->vertexBufferPtr++;

    // Bottom-left
    batch->vertexBufferPtr->position     = { rect.x, rect.y + rect.h };
    batch->vertexBufferPtr->uv           = { textureRect.x, textureRect.h };
    batch->vertexBufferPtr->color        = color;
    batch->vertexBufferPtr->textureIndex = textureIndex;
    batch->vertexBufferPtr++;

    // Bottom-right
    batch->vertexBufferPtr->position     = { rect.x + rect.w, rect.y + rect.h };
    batch->vertexBufferPtr->uv           = { textureRect.w, textureRect.h };
    batch->vertexBufferPtr->color        = color;
    batch->vertexBufferPtr->textureIndex = textureIndex;
    batch->vertexBufferPtr++;

    batch->count++;
}

chess_internal void Rect2DBatchFlush(Rect2DBatch* batch, Mat4x4 viewProj)
{
    if (batch->count > 0)
    {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(batch->program.id);

        GLint samplerArrayLoc = glGetUniformLocation(batch->program.id, "textures");
        CHESS_ASSERT(samplerArrayLoc != -1);
        BindActiveTextures(samplerArrayLoc);

        u32 vertexCount = batch->count * 4;
        u32 indexCount  = batch->count * 6;

        glUniformMatrix4fv(batch->viewProjUniform, 1, GL_FALSE, &viewProj.e[0][0]);

        glBindBuffer(GL_ARRAY_BUFFER, batch->VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Rect2DVertex), batch->vertexBuffer);
        glBindVertexArray(batch->VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        Rect2DBatchClear(batch);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }
}

chess_internal inline void Rect2DBatchClear(Rect2DBatch* batch)
{
    batch->vertexBufferPtr = batch->vertexBuffer;
    batch->count           = 0;
}