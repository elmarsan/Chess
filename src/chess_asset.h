#pragma once

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
    TEXTURE_BOARD_NORMAL,
    TEXTURE_BOARD_ARM,
    TEXTURE_WHITE_ALBEDO,
    TEXTURE_WHITE_NORMAL,
    TEXTURE_WHITE_ARM,
    TEXTURE_BLACK_ALBEDO,
    TEXTURE_BLACK_NORMAL,
    TEXTURE_BLACK_ARM,
    TEXTURE_2D_ATLAS,
    TEXTURE_HDR_SCENE,
    TEXTURE_COUNT
};

// clang-format off
const char* texturePaths [TEXTURE_COUNT] =
{
	"chess_set_board_diff.png",
	"chess_set_board_nor.png",
	"chess_set_board_arm.png",
	"chess_set_pieces_white_diff.png",
	"chess_set_pieces_white_nor.png",
	"chess_set_pieces_white_arm.png",
	"chess_set_pieces_black_diff.png",
	"chess_set_pieces_black_nor.png",
	"chess_set_pieces_black_arm.png",
	"atlas.png",
	"newport_loft.hdr"
};

const char* soundPaths[GAME_SOUND_COUNT] = 
{
	"move.wav",
	"illegal.wav",
	"check.wav",
};
// clang-format on

constexpr Rect textureCursorPointer{ 64.0f, 64.0f, 39.0f, 62.0f };
constexpr Rect textureCursorFinger{ 88.0f, 203.0f, 56.0f, 59.0f };
constexpr Rect textureCursorPick{ 369.0f, 228.0f, 47.0f, 50.0f };
constexpr Rect textureArrowLeft{ 443.0f, 63.0f, 32.0f, 64.0f };
constexpr Rect textureArrowRight{ 484.0f, 63.0f, 32.0f, 64.0f };
constexpr Rect textureArrowUndo{ 452.0f, 207.0f, 64.0f, 64.0f };
constexpr Rect textureCircle{ 632.0f, 71.0f, 63.0f, 64.0f };
constexpr Rect textureCircleOutlined{ 550.0f, 213.0f, 56.0f, 56.0f };
constexpr Rect textureStar{ 714.0f, 69.0f, 64.0f, 64.0f };
constexpr Rect textureStarAlt{ 632.0f, 205.0f, 64.0f, 64.0f };
constexpr Rect textureRectOutlined{ 717.0f, 203.0f, 69.0f, 69.0f };

Mat4x4 MeshComputeModelMatrix(Mesh* meshes, u32 index);

struct Assets
{
    Mesh    meshes[MESH_COUNT];
    Texture textures[TEXTURE_COUNT];
    Sound   sounds[GAME_SOUND_COUNT];
};

void LoadGameAssets(GameMemory* memory);