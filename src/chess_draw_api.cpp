#include <ft2build.h>
#include FT_FREETYPE_H

struct PlaneVertex
{
    Vec3 position;
    Vec4 color;
};

struct TextVertex
{
    Vec2 position;
    Vec2 uv;
    Vec4 color;
};

struct RectVertex
{
    Vec2 position;
    Vec4 color;
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

struct RenderData
{
    u32          planeVAO;
    u32          planeVBO;
    u32          planeIBO;
    PlaneVertex* planeVertexBuffer;
    PlaneVertex* planeVertexBufferPtr;
    u32          planeCount;
    Program      planeProgram;
    Camera3D*    camera3D;
    Camera2D*    camera2D;
    Program      meshProgram;
    u32          renderPhase;
    Program      mousePickingProgram;
    u32          mousePickingFBO;
    u32          mousePickingColorTexture;
    u32          mousePickingDepthTexture;
    //
    u32           fontAtlasTexture;
    Vec2U         atlasDimension;
    FontCharacter fontChars[128];
    u32           fontVAO;
    u32           fontVBO;
    u32           fontIBO;
    TextVertex*   fontVertexBuffer;
    TextVertex*   fontVertexBufferPtr;
    u32           fontCount;
    Program       fontProgram;
    //
    u32         rectCount;
    u32         rectVAO;
    u32         rectVBO;
    u32         rectIBO;
    RectVertex* rectVertexBuffer;
    RectVertex* rectVertexBufferPtr;
    Program     rectProgram;
};

#define MAX_PLANE_COUNT        100
#define MAX_PLANE_VERTEX_COUNT MAX_PLANE_COUNT * 4
#define MAX_PLANE_INDEX_COUNT  MAX_PLANE_COUNT * 6

#define MAX_FONT_CHAR_COUNT        500
#define MAX_FONT_CHAR_VERTEX_COUNT MAX_FONT_CHAR_COUNT * 4
#define MAX_FONT_CHAR_INDEX_COUNT  MAX_FONT_CHAR_COUNT * 6

#define MAX_RECT_COUNT        100
#define MAX_RECT_VERTEX_COUNT MAX_RECT_COUNT * 4
#define MAX_RECT_INDEX_COUNT  MAX_RECT_COUNT * 6

chess_internal RenderData gRenderData;

chess_internal void FlushPlanes();
chess_internal void FlushRectangles();
chess_internal void FlushTextBuffer();
chess_internal void FreeTypeInit();

DRAW_INIT(DrawInitProcedure)
{
    CHESS_LOG("Renderer api: InitDrawing");

    FreeTypeInit();

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

    glGenVertexArrays(1, &gRenderData.planeVAO);
    glGenBuffers(1, &gRenderData.planeVBO);
    glGenBuffers(1, &gRenderData.planeIBO);

    glBindVertexArray(gRenderData.planeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gRenderData.planeVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_PLANE_VERTEX_COUNT * sizeof(PlaneVertex), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gRenderData.planeIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, position));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, color));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    gRenderData.planeVertexBuffer    = new PlaneVertex[MAX_PLANE_VERTEX_COUNT];
    gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;

    const char* planeVertexShader   = R"(
		#version 330 core
		layout(location = 0) in vec3 aPos;
		layout(location = 1) in vec4 aColor;
		
		out vec4 outColor;
				
		uniform mat4 view;
		uniform mat4 projection;
		void main()
		{
			outColor = aColor;
			gl_Position = projection * view * vec4(aPos, 1.0);
		}
		)";
    const char* planeFragmentShader = R"(
		#version 330 core
		in vec4 outColor;
		out vec4 FragColor;
		void main()
		{
			FragColor = outColor;
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

    glGenFramebuffers(1, &gRenderData.mousePickingFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.mousePickingFBO);

    glGenTextures(1, &gRenderData.mousePickingColorTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.mousePickingColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, windowWidth, windowHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gRenderData.mousePickingColorTexture,
                           0);

    glGenTextures(1, &gRenderData.mousePickingDepthTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.mousePickingDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRenderData.mousePickingDepthTexture, 0);

    GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        CHESS_LOG("OpenGL Framebuffer error, status: 0x%x", framebufferStatus);
        CHESS_ASSERT(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Rectangles
    glGenVertexArrays(1, &gRenderData.rectVAO);
    glGenBuffers(1, &gRenderData.rectVBO);
    glGenBuffers(1, &gRenderData.rectIBO);

    glBindVertexArray(gRenderData.rectVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gRenderData.rectVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_RECT_VERTEX_COUNT * sizeof(RectVertex), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gRenderData.rectIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(RectVertex), (void*)offsetof(RectVertex, position));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(RectVertex), (void*)offsetof(RectVertex, color));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    gRenderData.rectVertexBuffer    = new RectVertex[MAX_RECT_VERTEX_COUNT];
    gRenderData.rectVertexBufferPtr = gRenderData.rectVertexBuffer;

    const char* rectVertexSource = R"(
		#version 330
		layout (location = 0) in vec2 aPos;
		layout (location = 1) in vec4 aColor;
		
		out vec4 Color;

		uniform mat4 viewProj;

		void main()
		{
			Color = aColor;
			gl_Position = viewProj * vec4(aPos, 0.0, 1.0);
		}
		)";

    const char* rectFragmentSource = R"(
		#version 330
		
		in vec4 Color;
		out vec4 FragColor;

		void main()
		{
			FragColor = Color;
		}
		)";

    gRenderData.rectProgram = OpenGLProgramBuild(rectVertexSource, rectFragmentSource);
    // ----------------------------------------------------------------------------
}

DRAW_DESTROY(DrawDestroyProcedure) { delete[] gRenderData.planeVertexBuffer; }

DRAW_BEGIN(DrawBeginProcedure)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;
    gRenderData.planeCount           = 0;

    gRenderData.rectVertexBufferPtr = gRenderData.rectVertexBuffer;
    gRenderData.rectCount           = 0;

#if 1
    // Bind font atlas texture to a some texture unit so it can be inspected using RenderDoc
    glActiveTexture(GL_TEXTURE31);
    glBindTexture(GL_TEXTURE_2D, gRenderData.fontAtlasTexture);
#endif
}

DRAW_END(DrawEndProcedure)
{
    FlushPlanes();
    FlushRectangles();
    FlushTextBuffer();
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

        gRenderData.planeVertexBufferPtr->position = worldPos3;
        gRenderData.planeVertexBufferPtr->color    = color;
        gRenderData.planeVertexBufferPtr++;
    }

    gRenderData.planeCount++;
}

chess_internal void FlushPlanes()
{
    if (gRenderData.planeCount > 0)
    {
        u32 vertexCount = gRenderData.planeCount * 4;
        u32 indexCount  = gRenderData.planeCount * 6;

        glUseProgram(gRenderData.planeProgram.id);

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
    }
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

    // f32 xOffset = 0;
    for (size_t i = 0; i < strlen(text); i++)
    {
        FontCharacter fontChar = gRenderData.fontChars[text[i]];

        f32 xpos = x + fontChar.left;
        // f32 ypos = y - (fontChar.rows - fontChar.top);
		f32 ypos = y - fontChar.top;
        f32 w    = fontChar.width;
        f32 h    = fontChar.rows;

        f32 u0 = (f32)fontChar.textureXOffset / (f32)gRenderData.atlasDimension.w;
        f32 v0 = 0.0f;
        f32 u1 = (f32)(fontChar.textureXOffset + fontChar.width) / (f32)gRenderData.atlasDimension.w;
        f32 v1 = (f32)fontChar.rows / (f32)gRenderData.atlasDimension.h;

  //      Vec2 textureSrc;
  //      Vec2 textureSize;

  //      textureSrc.x = { fontChar.textureXOffset / (f32)gRenderData.atlasDimension.w };
  //      textureSrc.y = 0.0f;

  //      textureSize.w = (fontChar.textureXOffset + (f32)fontChar.width) / (f32)gRenderData.atlasDimension.w;
  //      textureSize.h = (f32)gRenderData.atlasDimension.y;

  //      Vec2 textureCoords[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

  //      // Top-right
  //      gRenderData.fontVertexBufferPtr->position = { xpos + w, ypos };
  //      // gRenderData.fontVertexBufferPtr->uv       = { textureSize.x, textureSrc.y };
  //      gRenderData.fontVertexBufferPtr->uv    = { 1.0f, 1.0f };
  //      gRenderData.fontVertexBufferPtr->color = color;
  //      gRenderData.fontVertexBufferPtr++;

  //      // Top-left
  //      gRenderData.fontVertexBufferPtr->position = { xpos, ypos };
  //      // gRenderData.fontVertexBufferPtr->uv       = { textureSrc.x, textureSrc.y };
  //      gRenderData.fontVertexBufferPtr->uv    = { 0.0f, 1.0f };
  //      gRenderData.fontVertexBufferPtr->color = color;
  //      gRenderData.fontVertexBufferPtr++;

  //      // Bottom-left
  //      gRenderData.fontVertexBufferPtr->position = { xpos, ypos + h };
  //      // gRenderData.fontVertexBufferPtr->uv       = { textureSrc.x, textureSize.y };
  //      gRenderData.fontVertexBufferPtr->uv    = { 0.0f, 0.0f };
  //      gRenderData.fontVertexBufferPtr->color = color;
  //      gRenderData.fontVertexBufferPtr++;

  //      // Bottom-right
  //      gRenderData.fontVertexBufferPtr->position = { xpos + w, ypos + h };
  //      // gRenderData.fontVertexBufferPtr->uv       = { textureSize.x, textureSize.y };
  //      gRenderData.fontVertexBufferPtr->uv    = { 1.0f, 0.0f };
  //      gRenderData.fontVertexBufferPtr->color = color;
  //      gRenderData.fontVertexBufferPtr++;

              Vec2 positions[4] = {
                  { xpos + w, ypos },     // Top-right
                  { xpos, ypos },         // Top-left
                  { xpos, ypos + h },     // Bottom-left
                  { xpos + w, ypos + h }, // Bottom-right
              };
              Vec2 uvs[4] = {
                  { u1, v0 }, // Top-right
                  { u0, v0 }, // Top-left
                  { u0, v1 }, // Bottom-left
                  { u1, v1 }, // Bottom-right
              };

         Vec2 textureCoords[4] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

              for (u32 i = 0; i < 4; i++)
              {
                  gRenderData.fontVertexBufferPtr->position = positions[i];
                  gRenderData.fontVertexBufferPtr->uv       = uvs[i];
        	//gRenderData.fontVertexBufferPtr->uv       = textureCoords[i];
                  gRenderData.fontVertexBufferPtr->color    = color;
                  gRenderData.fontVertexBufferPtr++;
              }

        gRenderData.fontCount++;

        x += fontChar.advanceX;
        //y += fontChar.advanceY;
    }
}

DRAW_RECT(DrawRectProcedure)
{
    CHESS_ASSERT(gRenderData.camera2D);

    u32 rectVertexCount = gRenderData.rectCount * 4;
    if (rectVertexCount >= MAX_RECT_VERTEX_COUNT)
    {
        FlushRectangles();
    }

    // Top-right
    gRenderData.rectVertexBufferPtr->position = { rect.x + rect.w, rect.y };
    gRenderData.rectVertexBufferPtr->color    = color;
    gRenderData.rectVertexBufferPtr++;

    // Top-left
    gRenderData.rectVertexBufferPtr->position = { rect.x, rect.y };
    gRenderData.rectVertexBufferPtr->color    = color;
    gRenderData.rectVertexBufferPtr++;

    // Bottom-left
    gRenderData.rectVertexBufferPtr->position = { rect.x, rect.y + rect.h };
    gRenderData.rectVertexBufferPtr->color    = color;
    gRenderData.rectVertexBufferPtr++;

    // Bottom-right
    gRenderData.rectVertexBufferPtr->position = { rect.x + rect.w, rect.y + rect.h };
    gRenderData.rectVertexBufferPtr->color    = color;
    gRenderData.rectVertexBufferPtr++;

    gRenderData.rectCount++;
}

chess_internal void FlushRectangles()
{
    if (gRenderData.rectCount > 0)
    {
         glEnable(GL_BLEND);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        u32 vertexCount = gRenderData.rectCount * 4;
        u32 indexCount  = gRenderData.rectCount * 6;

        glUseProgram(gRenderData.rectProgram.id);

        glUniformMatrix4fv(glGetUniformLocation(gRenderData.rectProgram.id, "viewProj"), 1, GL_FALSE,
                           &gRenderData.camera2D->projection.e[0][0]);

        glBindBuffer(GL_ARRAY_BUFFER, gRenderData.rectVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(RectVertex), gRenderData.rectVertexBuffer);
        glBindVertexArray(gRenderData.rectVAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        gRenderData.rectVertexBufferPtr = gRenderData.rectVertexBuffer;
        gRenderData.rectCount           = 0;
         glDisable(GL_BLEND);
    }
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
    result.Mesh              = DrawMeshProcedure;
    result.BeginMousePicking = DrawBeginMousePickingProcedure;
    result.EndMousePicking   = DrawEndMousePickingProcedure;
    result.GetObjectAtPixel  = DrawGetObjectAtPixelProcedure;
    result.Text              = DrawTextProcedure;
    result.Begin2D           = DrawBegin2DProcedure;
    result.End2D             = DrawEnd2DProcedure;
    result.Rect              = DrawRectProcedure;

    return result;
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

    FT_Set_Pixel_Sizes(face, 0, 48);

    for (u32 charIndex = 32; charIndex < 128; charIndex++)
    {
        if (FT_Load_Char(face, charIndex, FT_LOAD_RENDER))
        {
            CHESS_LOG("[FreeType] failed to load glyph for ASCII: %d", charIndex);
            continue;
        }

        FT_GlyphSlot glyph = face->glyph;

        gRenderData.atlasDimension.w += glyph->bitmap.width;
        gRenderData.atlasDimension.h = Max(gRenderData.atlasDimension.h, glyph->bitmap.rows);
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &gRenderData.fontAtlasTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.fontAtlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gRenderData.atlasDimension.w, gRenderData.atlasDimension.h, 0, GL_RED,
                 GL_UNSIGNED_BYTE, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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

    u32 indices[MAX_FONT_CHAR_INDEX_COUNT]{};
    u32 offset = 0;
    for (u32 i = 0; i < MAX_FONT_CHAR_INDEX_COUNT; i += 6)
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

    glGenVertexArrays(1, &gRenderData.fontVAO);
    glGenBuffers(1, &gRenderData.fontVBO);
    glGenBuffers(1, &gRenderData.fontIBO);

    glBindVertexArray(gRenderData.fontVAO);

    glBindBuffer(GL_ARRAY_BUFFER, gRenderData.fontVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_FONT_CHAR_VERTEX_COUNT * sizeof(TextVertex), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gRenderData.fontIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (void*)offsetof(TextVertex, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (void*)offsetof(TextVertex, uv));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(TextVertex), (void*)offsetof(TextVertex, color));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    gRenderData.fontVertexBuffer    = new TextVertex[MAX_FONT_CHAR_VERTEX_COUNT];
    gRenderData.fontVertexBufferPtr = gRenderData.fontVertexBuffer;

    const char* fontVertexShader = R"(
		#version 330 core
		layout (location = 0) in vec2 aPosition;
		layout (location = 1) in vec2 aUV;
		layout (location = 2) in vec4 aColor;

		out vec2 UV;
		out vec4 color;

		uniform mat4 viewProj;

		void main()
		{
			UV = aUV;
			color = aColor;
			gl_Position = viewProj * vec4(aPosition.xy, 0.0, 1.0);
		}
		)";

    const char* fontFragmentShader = R"(
		#version 330 core
		in vec2 UV;
		in vec4 color;

		out vec4 FragColor;

		uniform sampler2D uTexture;

		void main()
		{
			float r = texture(uTexture, UV).r;
			FragColor = vec4(r, r, r, 1.0) * color;
		}
		)";

    gRenderData.fontProgram = OpenGLProgramBuild(fontVertexShader, fontFragmentShader);
}

chess_internal void FlushTextBuffer()
{
    CHESS_ASSERT(gRenderData.camera2D);

    if (gRenderData.fontCount > 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gRenderData.fontAtlasTexture);

        u32 vertexCount = gRenderData.fontCount * 4;
        u32 indexCount  = gRenderData.fontCount * 6;

        glUseProgram(gRenderData.fontProgram.id);
        glUniform1i(glGetUniformLocation(gRenderData.fontProgram.id, "uTexture"), 0);

        glUniformMatrix4fv(glGetUniformLocation(gRenderData.fontProgram.id, "viewProj"), 1, GL_FALSE,
                           &gRenderData.camera2D->projection.e[0][0]);

        glBindBuffer(GL_ARRAY_BUFFER, gRenderData.fontVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(TextVertex), gRenderData.fontVertexBuffer);
        glBindVertexArray(gRenderData.fontVAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        gRenderData.fontVertexBufferPtr = gRenderData.fontVertexBuffer;
        gRenderData.fontCount           = 0;
    }
}