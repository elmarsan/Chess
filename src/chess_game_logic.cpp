// #include "chess_game_logic.h"

#include <Disservin/chess.hpp>

#define VALIDATE_CELL(index) CHESS_ASSERT(index >= 0 && index <= CELL_COUNT)

#define CELL_INDEX(row, col) (col + (row * 8))

// extern "C"
Board BoardCreate(const char* fen)
{
    Board board;
    board.fen = fen;
    return board;
}

// extern "C"
Piece BoardGetPiece(Board* board, u32 row, u32 col)
{
    CHESS_ASSERT(board);
    Piece result;
    result.row = row;
    result.col = col;

    chess::Board auxBoard{ board->fen };
    chess::Piece piece = auxBoard.at(CELL_INDEX(row, col));

    switch (piece.type().internal())
    {
    case chess::PieceType::PAWN:
    {
        //printf(result.name, sizeof(result.name), "%s", "Pawn");
        result.type      = PIECE_TYPE_PAWN;
        result.meshIndex = MESH_PAWN;
        break;
    }
    case chess::PieceType::BISHOP:
    {
        //printf(result.name, sizeof(result.name), "%s", "Bishop");
        result.type      = PIECE_TYPE_BISHOP;
        result.meshIndex = MESH_BISHOP;
        break;
    }
    case chess::PieceType::KNIGHT:
    {
        //printf(result.name, sizeof(result.name), "%s", "Bishop");
        result.type      = PIECE_TYPE_KNIGHT;
        result.meshIndex = MESH_KNIGHT;
        break;
    }
    case chess::PieceType::ROOK:
    {
        //printf(result.name, sizeof(result.name), "%s", "Rook");
        result.type      = PIECE_TYPE_ROOK;
        result.meshIndex = MESH_ROOK;
        break;
    }

    case chess::PieceType::KING:
    {
        //printf(result.name, sizeof(result.name), "%s", "King");
        result.type      = PIECE_TYPE_KING;
        result.meshIndex = MESH_KING;
        break;
    }
    case chess::PieceType::QUEEN:
    {
        //printf(result.name, sizeof(result.name), "%s", "Queen");
        result.type      = PIECE_TYPE_QUEEN;
        result.meshIndex = MESH_QUEEN;
        break;
    }
    case chess::PieceType::NONE:
    {
        //printf(result.name, sizeof(result.name), "%s", "-");
        result.type = PIECE_TYPE_NONE;
        break;
    }
    default:
    {
        CHESS_ASSERT(0);
    }
    }

    if (piece.color() == chess::Color::WHITE)
    {
        result.color        = PIECE_COLOR_WHITE;
        result.textureIndex = TEXTURE_WHITE_ALBEDO;
    }
    else
    {
        result.color        = PIECE_COLOR_BLACK;
        result.textureIndex = TEXTURE_BLACK_ALBEDO;
    }

    return result;
}

// Move GetMoveListAtCell(u32 cellIndex)
//{
//	VALIDATE_CELL(cellIndex);

//	chess::Square square{ (int)cell };
//}