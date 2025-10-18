struct PlaneVertex
{
    Vec3 position;
    Vec4 color;
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

    const char* vertexShader   = R"(
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
    const char* fragmentShader = R"(
		#version 330 core
		in vec4 outColor;
		out vec4 FragColor;
		void main()
		{
			FragColor = outColor;
		}
		)";

    gRenderData.planeProgram = OpenGLProgramBuild(vertexShader, fragmentShader);
}

DRAW_DESTROY(DrawDestroyProcedure) { delete[] gRenderData.planeVertexBuffer; }

DRAW_BEGIN(DrawBeginProcedure)
{
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

DrawAPI DrawApiCreate()
{
    DrawAPI result;

    result.Init    = DrawInitProcedure;
    result.Destroy = DrawDestroyProcedure;
    result.Begin   = DrawBeginProcedure;
    result.End     = DrawEndProcedure;
    result.Begin3D = DrawBegin3D;
    result.End3D   = DrawEnd3D;
    result.Plane3D = DrawPlane3DProcedure;

    return result;
}