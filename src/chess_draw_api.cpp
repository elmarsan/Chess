#include "chess_draw_api.h"
#include "chess_gfx.h"

struct PlaneVertex
{
    Vec3 position;
    Vec4 color;
};

struct RenderData
{
    GfxVertexArray  planeVAO;
    GfxVertexBuffer planeVBO;
    GfxIndexBuffer  planeIBO;
    PlaneVertex*    planeVertexBuffer;
    PlaneVertex*    planeVertexBufferPtr;
    u32             planeCount;
    GfxPipeline     planePipeline;
    Camera*         camera3D;
};

#define MAX_PLANE_COUNT        100
#define MAX_PLANE_VERTEX_COUNT MAX_PLANE_COUNT * 4
#define MAX_PLANE_INDEX_COUNT  MAX_PLANE_COUNT * 6

internal RenderData gRenderData;

internal void FlushPlanes();

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

    gRenderData.planeVAO = GfxVertexArrayCreate();
    GfxVertexArrayBind(gRenderData.planeVAO);

    gRenderData.planeVBO = GfxVertexBufferCreate(nullptr, MAX_PLANE_VERTEX_COUNT * sizeof(PlaneVertex), true);
    gRenderData.planeIBO = GfxIndexBufferCreate(indices, sizeof(indices), false);

    GfxVertexArrayAttribPointer(0, 3, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, position));
    GfxVertexArrayAttribPointer(1, 4, sizeof(PlaneVertex), (void*)offsetof(PlaneVertex, color));

    gRenderData.planeVertexBuffer    = new PlaneVertex[MAX_PLANE_VERTEX_COUNT];
    gRenderData.planeVertexBufferPtr = gRenderData.planeVertexBuffer;

    GfxPipelineDesc planePipelineDesc;
    planePipelineDesc.vertexShader   = R"(
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
    planePipelineDesc.FragmentShader = R"(
		#version 330 core
		in vec4 outColor;
		out vec4 FragColor;
		void main()
		{
			FragColor = outColor;
		}
		)";

    gRenderData.planePipeline = GfxPipelineCreate(&planePipelineDesc);
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
        Vec3 worldPos;
        worldPos.x = position.x + planeVertices[i].x * size.x;
        worldPos.y = position.y + planeVertices[i].y;
        worldPos.z = position.z + planeVertices[i].z * size.y;

        gRenderData.planeVertexBufferPtr->position = worldPos;
        gRenderData.planeVertexBufferPtr->color    = color;
        gRenderData.planeVertexBufferPtr++;
    }

    gRenderData.planeCount++;
}

internal void FlushPlanes()
{
    if (gRenderData.planeCount > 0)
    {
        u32 vertexCount = gRenderData.planeCount * 4;
        u32 indexCount  = gRenderData.planeCount * 6;

        GfxPipelineBind(gRenderData.planePipeline);

        GfxUniformSetMat4("view", &gRenderData.camera3D->view.e[0][0]);
        GfxUniformSetMat4("projection", &gRenderData.camera3D->projection.e[0][0]);

        GfxVertexBufferSetData(gRenderData.planeVBO, gRenderData.planeVertexBuffer, vertexCount * sizeof(PlaneVertex));
        GfxVertexArrayBind(gRenderData.planeVAO);
        GfxDrawIndexed(indexCount);

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