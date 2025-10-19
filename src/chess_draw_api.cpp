struct PlaneVertex
{
    Vec3 position;
    Vec4 color;
};

enum
{
    RENDER_PHASE_MOUSE_PICKING,
    RENDER_PHASE_DRAW
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
    Camera*      camera3D;
    Program      meshProgram;
    u32          renderPhase;
    Program      mousePickingProgram;
    u32          mousePickingFBO;
    u32          mousePickingColorTexture;
    u32          mousePickingDepthTexture;
};

#define MAX_PLANE_COUNT        100
#define MAX_PLANE_VERTEX_COUNT MAX_PLANE_COUNT * 4
#define MAX_PLANE_INDEX_COUNT  MAX_PLANE_COUNT * 6

chess_internal RenderData gRenderData;

chess_internal void FlushPlanes();

DRAW_INIT(DrawInitProcedure)
{
    CHESS_LOG("Renderer api: InitDrawing");

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
    glBufferData(GL_ARRAY_BUFFER, MAX_PLANE_VERTEX_COUNT * sizeof(PlaneVertex), 0, GL_STATIC_DRAW);

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
}

DRAW_DESTROY(DrawDestroyProcedure) { delete[] gRenderData.planeVertexBuffer; }

DRAW_BEGIN(DrawBeginProcedure)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;
    gRenderData.planeCount           = 0;
}

DRAW_END(DrawEndProcedure) { FlushPlanes(); }

DRAW_BEGIN_3D(DrawBegin3D)
{
    CHESS_ASSERT(camera);
    gRenderData.camera3D = camera;
}

DRAW_END_3D(DrawEnd3D)
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

    return result;
}