#pragma once

#define UCI_STR_MAX_LENGTH 10
#define FEN_STR_MAX_LENGTH 92

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
    PIECE_COLOR_WHITE,
    PIECE_COLOR_NONE
};

struct Piece
{
    u32 type;
    u32 color;
    u32 cellIndex;
    u32 meshIndex;
    u32 textureIndex;
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
    u32  from;
    u32  to;
    u32  type;
    char uci[UCI_STR_MAX_LENGTH];
};

enum
{
    BOARD_GAME_RESULT_NONE,
    BOARD_GAME_RESULT_WIN,
    BOARD_GAME_RESULT_LOSE,
    BOARD_GAME_RESULT_DRAW
};

struct Board
{
    Stack<std::string> history;
};

Board BoardCreate(const char* fen);
Piece BoardGetPiece(Board* board, u32 cellIndex);
Move* BoardGetPieceMoveList(Board* board, u32 cellIndex, u32* moveCount);
void  FreePieceMoveList(Move* move);
void  BoardMoveDo(Board* board, Move* move);
void  BoardMoveUndo(Board* board);
bool  BoardMoveCanUndo(Board* board);
u32   BoardGetTurn(Board* board);
u32   BoardGetGameResult(Board* board);