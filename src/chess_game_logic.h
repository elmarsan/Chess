#pragma once

#define CELL_COUNT 64

enum
{
    PIECE_TYPE_PAWN,
    PIECE_TYPE_BISHOP,
    PIECE_TYPE_KNIGHT,
    PIECE_TYPE_ROOK,
    PIECE_TYPE_KING,
    PIECE_TYPE_QUEEN,
    PIECE_TYPE_NONE,
    PIECE_TYPE_COUNT
};

enum
{
    PIECE_COLOR_BLACK,
    PIECE_COLOR_WHITE
};

struct Piece
{
    char name[32];
    u32  type;
    u32  color;
    u32  row;
    u32  col;
    u32  meshIndex;
    u32  textureIndex;
};

enum
{
    MOVE_TYPE_NORMAL,
    MOVE_TYPE_CAPTURE,
    MOVE_TYPE_CHECK,
    MOVE_TYPE_ENPASSANT,
    MOVE_TYPE_CASTLING,
    MOVE_TYPE_PROMOTION
};

struct Move
{
    u32 from;
    u32 to;
    u32 type;
};

struct Board
{
    const char* fen;
};

Board BoardCreate(const char* fen);
Piece BoardGetPiece(Board* board, u32 row, u32 col);
Move* BoardGetPieceMoveList(Board* board, u32 row, u32 col, u32* moveCount);
void  FreePieceMoveList(Move* move);