#pragma once

#include "chess_gfx.h"
#include "chess_platform.h"
#include "chess_types.h"

struct GameMemory;

enum
{
    GAME_SOUND_MOVE,
    GAME_SOUND_ILLEGAL,
    GAME_SOUND_CHECK,

    GAME_SOUND_COUNT
};

// Board    vertex count: 344,    indices: 534
// Grid     vertex count: 4,      indices: 6
// Queen    vertex count: 1827,   indices: 8820
// King     vertex count: 2643,   indices: 12516
// Knigth   vertex count: 1705,   indices: 7896
// Rook     vertex count: 2016,   indices: 8928
// Pawn     vertex count: 1028,   indices: 4920
// Bishop   vertex count: 1738,   indices: 8334
enum
{
    MESH_BOARD,
    MESH_BOARD_GRID_SURFACE,
    MESH_KING,
    MESH_QUEEN,
    MESH_KNIGHT,
    MESH_BISHOP,
    MESH_ROOK,
    MESH_PAWN,
    MESH_COUNT
};

enum
{
    TEXTURE_BOARD_ALBEDO,
    // TEXTURE_BOARD_NORMAL,
    // TEXTURE_BOARD_METALLIC_ROUGHNESS,
    TEXTURE_WHITE_ALBEDO,
    // TEXTURE_WHITE_NORMAL,
    // TEXTURE_WHITE_METALLIC_ROUGHNESS,
    TEXTURE_BLACK_ALBEDO,
    // TEXTURE_BLACK_NORMAL,
    // TEXTURE_BLACK_METALLIC_ROUGHNESS,

    TEXTURE_COUNT
};

// clang-format off
const char* texturePaths [TEXTURE_COUNT] =
{
	"chess_set_board_diff.png",
	// "chess_set_board_nor_gl.png",
	// "chess_set_board_rough.png",
	"chess_set_pieces_white_diff.png",
	//"chess_set_pieces_white_nor_gl.png",
	//"chess_set_pieces_white_rough.png",
	"chess_set_pieces_black_diff.png",
	//"chess_set_pieces_black_nor_gl.png",
	//"chess_set_pieces_black_rough.png"
};
// clang-format on

struct MeshVertex
{
    Vec3 position;
    Vec2 uv;
    Vec3 normal;
};

struct Mesh
{
    u32             vertexCount;
    u32             indicesCount;
    s32             parentIndex;
    Vec3            translate;
    Vec3            rotate;
    Vec3            scale;    
    GfxVertexArray  vao;
    GfxVertexBuffer vbo;
    GfxIndexBuffer  ibo;
};

// TODO: Free asset resources on exit
struct Assets
{
    Mesh       meshes[MESH_COUNT];
    GfxTexture textures[TEXTURE_COUNT];
    Sound      sounds[GAME_SOUND_COUNT];
};

void LoadGameAssets(GameMemory* memory);