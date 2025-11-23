#include <ft2build.h>
#include FT_FREETYPE_H

#define MAX_RECT_COUNT        100
#define MAX_RECT_VERTEX_COUNT MAX_RECT_COUNT * 4
#define MAX_RECT_INDEX_COUNT  MAX_RECT_COUNT * 6

#define MAX_PLANE_COUNT        100
#define MAX_PLANE_VERTEX_COUNT MAX_PLANE_COUNT * 4
#define MAX_PLANE_INDEX_COUNT  MAX_PLANE_COUNT * 6

#define MAX_TEXTURES 8
#define MAX_LIGHTS   4

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
    DRAW_PASS_PICKING,
    DRAW_PASS_SHADOW,
    DRAW_PASS_RENDER
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
    GLuint        VAO;
    GLuint        VBO;
    Rect2DVertex* vertexBuffer;
    Rect2DVertex* vertexBufferPtr;
    GLuint        program;
    GLint         viewProjUniform;
};

chess_internal void Rect2DBatchInit(Rect2DBatch* batch, GLuint quadIBO);
chess_internal void Rect2DBatchAddRect(Rect2DBatch* batch, Rect rect, Vec4 color, Mat4x4 viewProj, Texture* texture,
                                       Rect textureRect);
chess_internal void Rect2DBatchFlush(Rect2DBatch* batch, Mat4x4 viewProj);
chess_internal void Rect2DBatchClear(Rect2DBatch* batch);

struct RenderData
{
    GLuint        quadIBO;
    Rect2DBatch   rect2DBatch;
    GLuint        planeVAO;
    GLuint        planeVBO;
    PlaneVertex*  planeVertexBuffer;
    PlaneVertex*  planeVertexBufferPtr;
    u32           planeCount;
    GLuint        planeProgram;
    Camera3D*     camera3D;
    Camera2D*     camera2D;
    u32           renderPass;
    GLuint        mousePickingProgram;
    GLuint        mousePickingFBO;
    GLuint        mousePickingColorTexture;
    GLuint        mousePickingDepthTexture;
    GLuint        fontAtlasTexture;
    Vec2U         fontAtlasDimension;
    FontCharacter fontChars[128];
    Vec2U         viewportDimension;
    GLuint        textures[MAX_TEXTURES];
    GLuint        blinnPhongProgram;
    Light         lights[MAX_LIGHTS];
    u32           lightCount;
    GLuint        shadowProgram;
    GLuint        shadowFBO;
    GLuint        shadowDepthTexture;
    u32           shadowMapWidth;
    u32           shadowMapHeight;
    Mat4x4        shadowLightMatrix;
};

chess_internal RenderData gRenderData;

chess_internal void   FlushPlanes();
chess_internal void   FreeTypeInit();
chess_internal void   UpdateMousePickingFBO();
chess_internal u32    PushTexture(Texture* texture);
chess_internal void   BindActiveTextures(GLint samplerArrayLocation);
chess_internal GLuint ProgramBuild(const char* vertexSource, const char* fragmentSource);
chess_internal GLuint CompileShader(GLenum type, const char* src);

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

    gRenderData.planeProgram = ProgramBuild(planeVertexShader, planeFragmentShader);

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

    gRenderData.mousePickingProgram = ProgramBuild(mousePickingVertexSource, mousePickingFragmentSource);

    UpdateMousePickingFBO();

    FreeTypeInit();
    glClearColor(0.125f, 0.125f, 0.125f, 1.0f);

    // ----------------------------------------------------------------------------
    // Textures
    u32 whitePixel = 0xFFFFFFFF;
    glGenTextures(1, &gRenderData.textures[0]);
    glBindTexture(GL_TEXTURE_2D, gRenderData.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &whitePixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D);

    for (GLuint textureIndex = 1; textureIndex < ARRAY_COUNT(gRenderData.textures); textureIndex++)
    {
        gRenderData.textures[textureIndex] = -1;
    }
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Phong
    const char* blinnPhongVertexSource   = R"(
        #version 330
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aUV;
        layout(location = 2) in vec3 aNormal;
        layout(location = 3) in vec3 aTangent;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 lightMatrix;

        out VsOut
        {
            vec2 uv;
            vec3 fragPos;
            vec3 normal;
            vec4 fragPosLightSpace;
            mat3 TBN;
        } vsOut;

        void main()
        {
            vsOut.uv                = aUV;
            vsOut.fragPos           = vec3(model * vec4(aPos, 1.0));
            vsOut.normal            = mat3(transpose(inverse(model))) * aNormal;
            vsOut.fragPosLightSpace = lightMatrix * vec4(vsOut.fragPos, 1.0);

            vec3 T = normalize(vec3(model * vec4(aTangent, 0.0)));
            vec3 N = normalize(vec3(model * vec4(aNormal, 0.0)));
            vec3 B = cross(N, T);
            vsOut.TBN = mat3(T, B, N);

            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
		)";
    const char* blinnPhongFragmentSource = R"(
        #version 330
        #define MAX_LIGHTS 4

        struct Light
        {
            vec4 position;
            vec3 ambient;
            vec3 diffuse;
            vec3 specular;

            float constant;
            float linear;
            float quadratic;
        };

        struct Material
        {
            sampler2D albedo;
            sampler2D normalMap;
            vec3      specular;
            float     shininess;
        };

        out vec4 FragColor;

        uniform vec3      viewPos;
        uniform Light     lights[MAX_LIGHTS];
        uniform int       lightCount;
        uniform Material  material;
        uniform sampler2D shadowMap;

        in VsOut
        {
            vec2 uv;
            vec3 fragPos;
            vec3 normal;
            vec4 fragPosLightSpace;
            mat3 TBN;
        } fsIn;

        float ShadowCalc(vec3 lightDir, vec3 normal)
        {
            float shadow = 0.0;

            // perform perspective divide
            vec3 projCoords = fsIn.fragPosLightSpace.xyz / fsIn.fragPosLightSpace.w;
            // transform to [0,1] range
            projCoords = projCoords * 0.5 + 0.5;

           float closestDepth = texture(shadowMap, projCoords.xy).r;
           float currentDepth = projCoords.z;

            // Shadow acne fixing
            float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
            // PCF
            vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

            for(int x = -1; x <= 1; ++x)
            {
                for(int y = -1; y <= 1; ++y)
                {
                    float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                    shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
                }
            }
            shadow /= 9.0;

            return shadow;
		}

        vec3 BlinnPhong(Light light, vec3 normal, vec3 viewDir)
        {
            vec3 lightDir;
            float attenuation;
            vec3 lightPos = vec3(light.position);
            float shadow;

            if (light.position.w == 0.0) // Directional
            {
                lightDir    = normalize(-lightPos);
                attenuation = 1.0;
                shadow = ShadowCalc(lightDir, normal);
            }
            else if (light.position.w == 1.0) // Point
            {
                lightDir       = normalize(lightPos - fsIn.fragPos);
                float distance = length(lightPos - fsIn.fragPos);
                attenuation    = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
                shadow         = 1.0;
            }

            vec3 halfwayDir = normalize(lightDir + viewDir);

            vec3 albedo   = texture(material.albedo, fsIn.uv).rgb;
            float diff    = max(dot(normal, lightDir), 0.0);
            float spec    = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
            vec3 ambient  = light.ambient * albedo;
            vec3 diffuse  = light.diffuse * diff * albedo;
            vec3 specular = light.specular * spec * material.specular;
            
            ambient  *= attenuation;
            diffuse  *= attenuation;
            specular *= attenuation;

            return ambient + (1.0 - shadow) * (diffuse + specular);
		}

        void main()
        {
            vec3 normal = texture(material.normalMap, fsIn.uv).rgb;
            normal = normal * 2.0 - 1.0;
            normal = normalize(fsIn.TBN * normal);

            vec3 viewDir = normalize(viewPos - fsIn.fragPos);
            vec3 color   = vec3(0.0);

            for (int i = 0; i < lightCount; i++)
            {
                color += BlinnPhong(lights[i], normal, viewDir);
            }

            FragColor = vec4(color, 1.0);
        }
		)";

    gRenderData.blinnPhongProgram = ProgramBuild(blinnPhongVertexSource, blinnPhongFragmentSource);
    // ----------------------------------------------------------------------------

    // ----------------------------------------------------------------------------
    // Shadow mapping
    glGenFramebuffers(1, &gRenderData.shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.shadowFBO);

    // TODO: Make shadow mapping configurable
    gRenderData.shadowMapWidth  = 2048;
    gRenderData.shadowMapHeight = 2048;

    glGenTextures(1, &gRenderData.shadowDepthTexture);
    glBindTexture(GL_TEXTURE_2D, gRenderData.shadowDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, gRenderData.shadowMapWidth, gRenderData.shadowMapHeight, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gRenderData.shadowDepthTexture, 0);

    GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        CHESS_LOG("OpenGL Framebuffer error, status: 0x%x", framebufferStatus);
        CHESS_ASSERT(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const char* shadowVertexSource   = R"(
	#version 330 core
	layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aUV;
    layout(location = 2) in vec3 aNormal;
    layout(location = 3) in vec3 aTangent;

	uniform mat4 lightMatrix;
	uniform mat4 model;

	void main()
	{
		gl_Position = lightMatrix * model * vec4(aPos, 1.0);
	}
	)";
    const char* shadowFragmentSource = R"(
	#version 330 core
	void main() {}
	)";

    gRenderData.shadowProgram = ProgramBuild(shadowVertexSource, shadowFragmentSource);
    // ----------------------------------------------------------------------------

    glEnable(GL_MULTISAMPLE);
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

DRAW_LIGHT_ADD(DrawLightAddProcedure)
{
    CHESS_ASSERT(gRenderData.lightCount < MAX_LIGHTS);
    gRenderData.lights[gRenderData.lightCount++] = light;
}

DRAW_MESH(DrawMeshProcedure)
{
    CHESS_ASSERT(gRenderData.camera3D);

    if (gRenderData.renderPass == DRAW_PASS_RENDER)
    {
        GLint modelLoc        = glGetUniformLocation(gRenderData.blinnPhongProgram, "model");
        GLint matAlbedoLoc    = glGetUniformLocation(gRenderData.blinnPhongProgram, "material.albedo");
        GLint matNormalMapLoc = glGetUniformLocation(gRenderData.blinnPhongProgram, "material.normalMap");
        GLint matSpecLoc      = glGetUniformLocation(gRenderData.blinnPhongProgram, "material.specular");
        GLint matShininessLoc = glGetUniformLocation(gRenderData.blinnPhongProgram, "material.shininess");

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, material.albedo.id);
        glUniform1i(matAlbedoLoc, 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, material.normalMap.id);
        glUniform1i(matNormalMapLoc, 2);

        glUniform3fv(matSpecLoc, 1, &material.specular.x);
        glUniform1fv(matShininessLoc, 1, &material.shininess);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model.e[0][0]);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else if (gRenderData.renderPass == DRAW_PASS_PICKING)
    {
        GLint modelLoc    = glGetUniformLocation(gRenderData.mousePickingProgram, "model");
        GLint objectIdLoc = glGetUniformLocation(gRenderData.mousePickingProgram, "objectId");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model.e[0][0]);
        glUniform1ui(objectIdLoc, objectId + 1);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else if (gRenderData.renderPass == DRAW_PASS_SHADOW)
    {
        GLint modelLoc = glGetUniformLocation(gRenderData.shadowProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model.e[0][0]);

        glBindVertexArray(mesh->VAO);
        glDrawElements(GL_TRIANGLES, mesh->indicesCount, GL_UNSIGNED_INT, 0);
    }
    else
    {
        CHESS_ASSERT(0);
    }
}

DRAW_MESH_GPU_UPLOAD(DrawMeshGPUUploadProcedure)
{
    CHESS_ASSERT(mesh);

    glGenVertexArrays(1, &mesh->VAO);
    glGenBuffers(1, &mesh->VBO);
    glGenBuffers(1, &mesh->IBO);

    glBindVertexArray(mesh->VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(MeshVertex) * mesh->vertexCount, vertexs, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * mesh->indicesCount, indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, tangent));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
}

DRAW_BEGIN_PASS_PICKING(DrawBeginPassPickingProcedure)
{
    gRenderData.renderPass = DRAW_PASS_PICKING;
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.mousePickingFBO);
    glDisable(GL_DITHER);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gRenderData.mousePickingProgram);

    GLint viewLoc       = glGetUniformLocation(gRenderData.mousePickingProgram, "view");
    GLint projectionLoc = glGetUniformLocation(gRenderData.mousePickingProgram, "projection");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &gRenderData.camera3D->view.e[0][0]);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &gRenderData.camera3D->projection.e[0][0]);
}

DRAW_END_PASS_PICKING(DrawEndPassPickingProcedure)
{
    gRenderData.renderPass = DRAW_PASS_RENDER;
    glEnable(GL_DITHER);
    glUseProgram(0);
}

DRAW_BEGIN_PASS_SHADOW(DrawBeginPassShadowProcedure)
{
    gRenderData.renderPass        = DRAW_PASS_SHADOW;
    gRenderData.shadowLightMatrix = lightProj * lightView;

    glViewport(0, 0, gRenderData.shadowMapWidth, gRenderData.shadowMapHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, gRenderData.shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);

    glUseProgram(gRenderData.shadowProgram);

    GLint lightMatrixLoc = glGetUniformLocation(gRenderData.shadowProgram, "lightMatrix");
    glUniformMatrix4fv(lightMatrixLoc, 1, GL_FALSE, &gRenderData.shadowLightMatrix.e[0][0]);
}

DRAW_END_PASS_SHADOW(DrawEndPassShadowProcedure)
{
    glViewport(0, 0, (GLsizei)gRenderData.viewportDimension.w, (GLsizei)gRenderData.viewportDimension.h);
    glCullFace(GL_BACK);
    glUseProgram(0);
}

DRAW_BEGIN_PASS_RENDER(DrawBeginPassRenderProcedure)
{
    gRenderData.renderPass = DRAW_PASS_RENDER;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint program = gRenderData.blinnPhongProgram;
    glUseProgram(program);

    GLint viewLoc        = glGetUniformLocation(program, "view");
    GLint projectionLoc  = glGetUniformLocation(program, "projection");
    GLint lightMatrixLoc = glGetUniformLocation(program, "lightMatrix");
    GLint viewPosLoc     = glGetUniformLocation(program, "viewPos");
    GLint lightCountLoc  = glGetUniformLocation(program, "lightCount");
    GLint shadowMapLoc   = glGetUniformLocation(program, "shadowMap");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gRenderData.shadowDepthTexture);
    glUniform1i(shadowMapLoc, 0);

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &gRenderData.camera3D->view.e[0][0]);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &gRenderData.camera3D->projection.e[0][0]);
    glUniformMatrix4fv(lightMatrixLoc, 1, GL_FALSE, &gRenderData.shadowLightMatrix.e[0][0]);
    glUniform3fv(viewPosLoc, 1, &gRenderData.camera3D->position.x);

    glUniform1i(lightCountLoc, (GLint)gRenderData.lightCount);
    for (u32 i = 0; i < gRenderData.lightCount; i++)
    {
        char position[64];
        char ambient[64];
        char specular[64];
        char diffuse[64];
        char constant[64];
        char linear[64];
        char quadratic[64];
        sprintf(position, "lights[%d].position", i);
        sprintf(ambient, "lights[%d].ambient", i);
        sprintf(specular, "lights[%d].specular", i);
        sprintf(diffuse, "lights[%d].diffuse", i);
        sprintf(constant, "lights[%d].constant", i);
        sprintf(linear, "lights[%d].linear", i);
        sprintf(quadratic, "lights[%d].quadratic", i);

        GLint lightPosLoc       = glGetUniformLocation(program, position);
        GLint lightAmbientLoc   = glGetUniformLocation(program, ambient);
        GLint lightDiffuseLoc   = glGetUniformLocation(program, diffuse);
        GLint lightSpecularLoc  = glGetUniformLocation(program, specular);
        GLint lightConstantLoc  = glGetUniformLocation(program, constant);
        GLint lightLinearLoc    = glGetUniformLocation(program, linear);
        GLint lightQuadraticLoc = glGetUniformLocation(program, quadratic);

        glUniform4fv(lightPosLoc, 1, &gRenderData.lights[i].position.x);
        glUniform3fv(lightAmbientLoc, 1, &gRenderData.lights[i].ambient.x);
        glUniform3fv(lightDiffuseLoc, 1, &gRenderData.lights[i].diffuse.x);
        glUniform3fv(lightSpecularLoc, 1, &gRenderData.lights[i].specular.x);
        glUniform1fv(lightConstantLoc, 1, &gRenderData.lights[i].constant);
        glUniform1fv(lightLinearLoc, 1, &gRenderData.lights[i].linear);
        glUniform1fv(lightQuadraticLoc, 1, &gRenderData.lights[i].quadratic);
    }
}

DRAW_END_PASS_RENDER(DrawEndPassRenderProcedure) { glUseProgram(0); }

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
        size.w += fontChar.advanceX + fontChar.left;
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

DRAW_TEXTURE_CREATE(TextureCreateProcedure)
{
    CHESS_ASSERT(pixels);

    Texture result;
    result.width  = width;
    result.height = height;

    GLenum format;
    GLenum internalFormat;
    switch (bytesPerPixel)
    {
    case 3:
    {
        internalFormat = format = GL_RGB;
        break;
    }
    case 4:
    {
        internalFormat = format = GL_RGBA;
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

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, (GLsizei)width, (GLsizei)height, 0, format, GL_UNSIGNED_BYTE,
                 (GLvoid*)pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    return result;
}

DrawAPI DrawApiCreate()
{
    DrawAPI result;

    result.Init             = DrawInitProcedure;
    result.Destroy          = DrawDestroyProcedure;
    result.Begin            = DrawBeginProcedure;
    result.End              = DrawEndProcedure;
    result.Begin3D          = DrawBegin3D;
    result.End3D            = DrawEnd3D;
    result.Plane3D          = DrawPlane3DProcedure;
    result.PlaneTexture3D   = DrawPlaneTexture3DProcedure;
    result.Mesh             = DrawMeshProcedure;
    result.MeshGPUUpload    = DrawMeshGPUUploadProcedure;
    result.GetObjectAtPixel = DrawGetObjectAtPixelProcedure;
    result.Text             = DrawTextProcedure;
    result.TextGetSize      = DrawTextGetSizeProcedure;
    result.Begin2D          = DrawBegin2DProcedure;
    result.End2D            = DrawEnd2DProcedure;
    result.Rect             = DrawRectProcedure;
    result.RectTexture      = DrawRectTextureProcedure;
    result.TextureCreate    = TextureCreateProcedure;
    result.LightAdd         = DrawLightAddProcedure;
    result.BeginPassPicking = DrawBeginPassPickingProcedure;
    result.EndPassPicking   = DrawEndPassPickingProcedure;
    result.BeginPassShadow  = DrawBeginPassShadowProcedure;
    result.EndPassShadow    = DrawEndPassShadowProcedure;
    result.BeginPassRender  = DrawBeginPassRenderProcedure;
    result.EndPassRender    = DrawEndPassRenderProcedure;

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

        glUseProgram(gRenderData.planeProgram);

        GLint samplerArrayLoc = glGetUniformLocation(gRenderData.planeProgram, "textures");
        CHESS_ASSERT(samplerArrayLoc != -1);
        BindActiveTextures(samplerArrayLoc);

        glUniformMatrix4fv(glGetUniformLocation(gRenderData.planeProgram, "view"), 1, GL_FALSE,
                           &gRenderData.camera3D->view.e[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(gRenderData.planeProgram, "projection"), 1, GL_FALSE,
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

chess_internal GLuint ProgramBuild(const char* vertexSource, const char* fragmentSource)
{
    GLuint result = glCreateProgram();

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

    glAttachShader(result, vs);
    glAttachShader(result, fs);
    glLinkProgram(result);

    GLint ok;
    glGetProgramiv(result, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char infoBuffer[512];
        glGetProgramInfoLog(result, sizeof(infoBuffer), NULL, infoBuffer);
        CHESS_LOG("OpenGL linking program: '%s'", infoBuffer);
        CHESS_ASSERT(0);
    }

    return result;
}

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

chess_internal void Rect2DBatchInit(Rect2DBatch* batch, GLuint quadIBO)
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

    batch->program         = ProgramBuild(vertexSource, fragmentSource);
    batch->viewProjUniform = glGetUniformLocation(batch->program, "viewProj");
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

        glUseProgram(batch->program);

        GLint samplerArrayLoc = glGetUniformLocation(batch->program, "textures");
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