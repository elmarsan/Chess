#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

chess_internal void ParseMeshGeometry(GameMemory* memory, Mesh* mesh, cgltf_node* cgltfNode, s32 parentIndex)
{
    DrawAPI draw = memory->draw;

    cgltf_primitive* cgltfPrimitive = cgltfNode->mesh->primitives;
    cgltf_attribute* position       = 0;
    cgltf_attribute* uv             = 0;
    cgltf_attribute* normal         = 0;
    cgltf_attribute* tangent        = 0;

    for (u32 i = 0; i < cgltfPrimitive->attributes_count; i++)
    {
        cgltf_attribute* attribute = &cgltfPrimitive->attributes[i];

        if (!strcmp(attribute->name, "POSITION"))
        {
            position = attribute;
        }
        if (!strcmp(attribute->name, "TEXCOORD_0"))
        {
            uv = attribute;
        }
        if (!strcmp(attribute->name, "NORMAL"))
        {
            normal = attribute;
        }
        if (!strcmp(attribute->name, "TANGENT"))
        {
            tangent = attribute;
        }
    }

    mesh->vertexCount   = (u32)position->data->count;
    mesh->indicesCount  = (u32)cgltfPrimitive->indices->count;
    MeshVertex* vertexs = new MeshVertex[mesh->vertexCount];
    u32*        indices = new u32[mesh->indicesCount];
    mesh->parentIndex   = parentIndex;

    for (u32 i = 0; i < mesh->vertexCount; i++)
    {
        MeshVertex* vertex = &vertexs[i];

        if (!cgltf_accessor_read_float(position->data, i, &vertex->position.e[0], sizeof(float)))
        {
        }
        if (!cgltf_accessor_read_float(uv->data, i, &vertex->uv.e[0], sizeof(float)))
        {
        }
        if (!cgltf_accessor_read_float(normal->data, i, &vertex->normal.e[0], sizeof(float)))
        {
        }
        float tangentOut[4];
        if (!cgltf_accessor_read_float(normal->data, i, tangentOut, sizeof(float)))
        {
            CHESS_ASSERT(0);
        }
        else
        {
            vertex->tangent.e[0] = tangentOut[0];
            vertex->tangent.e[1] = tangentOut[1];
            vertex->tangent.e[2] = tangentOut[2];
        }
    }

    for (u32 i = 0; i < mesh->indicesCount; i++)
    {
        indices[i] = (u32)cgltf_accessor_read_index(cgltfPrimitive->indices, i);
    }

    if (cgltfNode->has_translation)
    {
        mesh->translate.x = cgltfNode->translation[0];
        mesh->translate.y = cgltfNode->translation[1];
        mesh->translate.z = cgltfNode->translation[2];
    }
    else
    {
        mesh->translate = { 0.0f };
    }

    if (cgltfNode->has_scale)
    {
        mesh->scale.x = cgltfNode->scale[0];
        mesh->scale.y = cgltfNode->scale[1];
        mesh->scale.z = cgltfNode->scale[2];
    }
    else
    {
        mesh->scale = { 1.0f };
    }

    draw.MeshGPUUpload(mesh, vertexs, indices);

    delete[] vertexs;
    delete[] indices;
}

chess_internal void ExtractMeshDataFromGLTF(GameMemory* memory)
{
    CHESS_ASSERT(memory);
    GameState* state  = (GameState*)memory->permanentStorage;
    Assets*    assets = &state->assets;

    memory->platform.Log("GAME loading gltf model ...");
    FileReadResult binFile  = memory->platform.FileReadEntire("../data/chess_set_v2.bin");
    FileReadResult jsonFile = memory->platform.FileReadEntire("../data/chess_set_v2.gltf");

    // clang-format off
    u32 nodes[] = {
		MESH_BOARD,
		MESH_QUEEN,
		MESH_KING,
		MESH_KNIGHT,
		MESH_ROOK,
		MESH_PAWN,
  		MESH_BISHOP,
    };
    // clang-format on

    cgltf_options options     = { 0 };
    cgltf_data*   data        = 0;
    cgltf_result  parseResult = cgltf_parse(&options, jsonFile.content, jsonFile.contentSize, &data);
    if (parseResult == cgltf_result_success)
    {
        if (data->buffers_count > 0)
        {
            data->buffers[0].data             = binFile.content;
            data->buffers[0].data_free_method = cgltf_data_free_method_none;

            for (u32 nodeIndex = 0; nodeIndex < ARRAY_COUNT(nodes); nodeIndex++)
            {
                u32         meshType  = nodes[nodeIndex];
                Mesh*       mesh      = &assets->meshes[meshType];
                cgltf_node* cgltfNode = data->scenes->nodes[nodeIndex];

                CHESS_ASSERT(meshType != MESH_BOARD_GRID_SURFACE);
                s32 parentIndex = meshType == MESH_BOARD ? -1 : MESH_BOARD_GRID_SURFACE;

                memory->platform.Log("GAME loading node: '%s'...", cgltfNode->name);

                ParseMeshGeometry(memory, mesh, cgltfNode, parentIndex);

                if (meshType == MESH_BOARD)
                {
                    CHESS_ASSERT(cgltfNode->children_count == 1);

                    Mesh* gridSurface = &assets->meshes[MESH_BOARD_GRID_SURFACE];

                    cgltf_node* cgltfBoardGridNode = cgltfNode->children[0];
                    memory->platform.Log("GAME loading node: '%s'...", cgltfBoardGridNode->name);
                    ParseMeshGeometry(memory, gridSurface, cgltfBoardGridNode, MESH_BOARD);
                }
            }
        }
        else
        {
            CHESS_ASSERT(0);
        }
    }
    else
    {
        memory->platform.Log("GAME unable to load gltf model");
    }

    if (binFile.contentSize > 0)
    {
        memory->platform.FileFreeMemory(binFile.content);
    }
    if (jsonFile.contentSize > 0)
    {
        memory->platform.FileFreeMemory(jsonFile.content);
    }
}

void LoadGameAssets(GameMemory* memory)
{
    GameState*  state    = (GameState*)memory->permanentStorage;
    Assets*     assets   = &state->assets;
    PlatformAPI platform = memory->platform;
    DrawAPI     draw     = memory->draw;

    // Meshes
    ExtractMeshDataFromGLTF(memory);

    // Textures
    for (u32 textureIndex = 0; textureIndex < TEXTURE_COUNT; textureIndex++)
    {
        char texturePath[256];
        sprintf(texturePath, "../data/textures/%s", texturePaths[textureIndex]);
        platform.Log("GAME loading texture: '%s'...", texturePath);

        Image image = platform.ImageLoad(texturePath);

        assets->textures[textureIndex] = draw.TextureCreate(&image);

        platform.ImageDestroy(&image);
    }

    // Sounds
    for (u32 soundIndex = 0; soundIndex < GAME_SOUND_COUNT; soundIndex++)
    {
        char soundPath[256];
        sprintf(soundPath, "../data/sounds/%s", soundPaths[soundIndex]);
        platform.Log("GAME loading sound: '%s'...", soundPath);

        assets->sounds[soundIndex] = platform.SoundLoad(soundPath);
    }
}

Mat4x4 MeshComputeModelMatrix(Mesh* meshes, u32 index)
{
    CHESS_ASSERT(meshes);
    Mesh* mesh = &meshes[index];

    Mat4x4 model = Identity();
    model        = Translate(model, mesh->translate);
    model        = Scale(model, mesh->scale);

    if (mesh->parentIndex != -1)
    {
        Mat4x4 parentModel = MeshComputeModelMatrix(meshes, mesh->parentIndex);
        model              = parentModel * model;
    }

    return model;
}