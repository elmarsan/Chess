#include "chess.h"
#include "chess_math.h"

#include <stddef.h>

// TODO: Free gpu resources

inline bool ButtonIsPressed(GameButtonState gameButtonState)
{
    return gameButtonState.isDown && !gameButtonState.wasDown;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    PlatformAPI          platform           = memory->platform;
    GfxAPI               g                  = memory->gfx;
    GameInputController* keyboardController = &memory->input.controllers[GAME_INPUT_CONTROLLER_KEYBOARD_0];
    GameInputController* gamepadController0 = &memory->input.controllers[GAME_INPUT_CONTROLLER_GAMEPAD_0];

    // Init
    if (!memory->isInitialized)
    {
        memory->isInitialized = true;

        // TODO: Get proper path to assets folder
        // Sound loading
        memory->sounds[GAME_SOUND_MOVE]    = platform.SoundLoad("../data/move.wav");
        memory->sounds[GAME_SOUND_ILLEGAL] = platform.SoundLoad("../data/illegal.wav");
        memory->sounds[GAME_SOUND_CHECK]   = platform.SoundLoad("../data/check.wav");
        // Texture loading
        Image containerImage = platform.ImageLoad("../data/chessset_gltf/textures/Chess01Shader_BaseColor.jpeg");
        memory->textures[0]  = g.GfxTexture2DCreate(containerImage.width, containerImage.height,
                                                    containerImage.bytesPerPixel, containerImage.pixels);
        platform.ImageDestroy(&containerImage);        

        const char* vertexSource   = R"(
			#version 330
			layout(location = 0) in vec3 aPos;
			layout(location = 1) in vec2 aTexCoords;
			
			out vec2 TexCoords;
			
			uniform mat4 model;
			uniform mat4 view;
			uniform mat4 projection;
			void main() 
			{
				TexCoords = aTexCoords;
				gl_Position = projection * view * model * vec4(aPos, 1.0); 
			}
			)";
        const char* fragmentSource = R"(
			#version 330
			out vec4 FragColor;
			in vec2 TexCoords;
			uniform sampler2D texture1;			
			void main() 
			{ 
				FragColor = texture(texture1, TexCoords);				
			}
			)";

        // clang-format off
		f32 vertices[] = {
			-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
			0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
			-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

			-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
			0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
			0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
			0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
			-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
			-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

			-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
			0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
			-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
			-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
		};
        // clang-format on

        GfxPipelineDesc desc = {};
        desc.vertexShader    = vertexSource;
        desc.FragmentShader  = fragmentSource;
        memory->pipeline     = g.GfxPipelineCreate(&desc);

        struct Vertex
        {
            Vec3 position;
            Vec2 textCoords;
        };

        memory->vao = g.GfxVertexArrayCreate();
        g.GfxVertexArrayBind(memory->vao);
        memory->vbo = g.GfxVertexBufferCreate(vertices, sizeof(vertices), false);

        g.GfxVertexArrayAttribPointer(0, 3, sizeof(Vertex), (void*)offsetof(Vertex, position));
        g.GfxVertexArrayAttribPointer(1, 2, sizeof(Vertex), (void*)offsetof(Vertex, textCoords));

        g.GfxEnableDepthTest();
    }

    // Update
    if (ButtonIsPressed(keyboardController->buttonAction))
    {
        g.GfxClearColor(0, 1, 0, 1);
    }
    if (ButtonIsPressed(keyboardController->buttonCancel))
    {
        g.GfxClearColor(0, 0, 1, 1);
    }
    if (gamepadController0->isEnabled)
    {
        if (ButtonIsPressed(gamepadController0->buttonAction))
        {
            g.GfxClearColor(1, 0, 0, 1);
            platform.SoundPlay(&memory->sounds[GAME_SOUND_CHECK]);
        }
        if (ButtonIsPressed(gamepadController0->buttonCancel))
        {
            platform.SoundPlay(&memory->sounds[GAME_SOUND_MOVE]);
        }
        if (ButtonIsPressed(gamepadController0->buttonStart))
        {
            platform.SoundPlay(&memory->sounds[GAME_SOUND_ILLEGAL]);
        }
    }

    // Draw
    g.GfxClear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);

    g.GfxPipelineBind(memory->pipeline);

    g.GfxTextureBind(memory->textures[0], 0);
    g.GfxUniformSetInt("texture1", 0);

    Mat4x4 model      = Identity();
    Mat4x4 view       = Identity();
    Mat4x4 projection = Identity();

    model = Translate(model, Vec3{ 0.0f, 0.0f, -5.0f });
    model = Rotate(model, (f32)platform.TimerGetTicks(), Vec3{ 0.5f, 1.0f, 0.0f });
    view  = Translate(view, Vec3{ 0.0f, 0.0f, 2.0f });

    Vec2U windowDimension = platform.WindowGetDimension();
    float aspect          = (float)windowDimension.w / (float)windowDimension.h;

    projection = Perspective(DEGTORAD(45.0f), aspect, 0.1f, 100.0f);

    g.GfxUniformSetMat4("model", &model.e[0][0]);
    g.GfxUniformSetMat4("view", &view.e[0][0]);
    g.GfxUniformSetMat4("projection", &projection.e[0][0]);

    g.GfxVertexArrayBind(memory->vao);
    g.GfxDrawArrays(36);
}