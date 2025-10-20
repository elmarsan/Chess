#include <Disservin/chess.hpp>

#define VALIDATE_CELL(index) CHESS_ASSERT(index >= 0 && index <= CELL_COUNT)
#define CELL_INDEX(row, col) (col + (row * 8))

chess_internal inline u32                 GetInternalMoveType(chess::Board& _board, chess::Move& _move);
chess_internal inline chess::PieceGenType GetExternalPieceGenType(chess::PieceType _pieceType);
chess_internal inline chess::Board        GetExternalBoard(Board* board);

Board BoardCreate(const char* fen)
{
    Board board;
    board.fen = fen;
    return board;
}

Piece BoardGetPiece(Board* board, u32 row, u32 col)
{
    CHESS_ASSERT(board);
    Piece result;
    result.row = row;
    result.col = col;

    chess::Board _board = GetExternalBoard(board);
    chess::Piece _piece = _board.at(CELL_INDEX(row, col));

    switch (_piece.type().internal())
    {
    case chess::PieceType::PAWN:
    {
        // printf(result.name, sizeof(result.name), "%s", "Pawn");
        result.type      = PIECE_TYPE_PAWN;
        result.meshIndex = MESH_PAWN;
        break;
    }
    case chess::PieceType::BISHOP:
    {
        // printf(result.name, sizeof(result.name), "%s", "Bishop");
        result.type      = PIECE_TYPE_BISHOP;
        result.meshIndex = MESH_BISHOP;
        break;
    }
    case chess::PieceType::KNIGHT:
    {
        // printf(result.name, sizeof(result.name), "%s", "Bishop");
        result.type      = PIECE_TYPE_KNIGHT;
        result.meshIndex = MESH_KNIGHT;
        break;
    }
    case chess::PieceType::ROOK:
    {
        // printf(result.name, sizeof(result.name), "%s", "Rook");
        result.type      = PIECE_TYPE_ROOK;
        result.meshIndex = MESH_ROOK;
        break;
    }

    case chess::PieceType::KING:
    {
        // printf(result.name, sizeof(result.name), "%s", "King");
        result.type      = PIECE_TYPE_KING;
        result.meshIndex = MESH_KING;
        break;
    }
    case chess::PieceType::QUEEN:
    {
        // printf(result.name, sizeof(result.name), "%s", "Queen");
        result.type      = PIECE_TYPE_QUEEN;
        result.meshIndex = MESH_QUEEN;
        break;
    }
    case chess::PieceType::NONE:
    {
        // printf(result.name, sizeof(result.name), "%s", "-");
        result.type = PIECE_TYPE_NONE;
        break;
    }
    default:
    {
        CHESS_ASSERT(0);
    }
    }

    if (_piece.color() == chess::Color::WHITE)
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

chess_internal inline u32 GetInternalMoveType(chess::Board& _board, chess::Move& _move)
{
    u32 result;

    switch (_move.typeOf())
    {
    case chess::Move::ENPASSANT:
    {
        result = MOVE_TYPE_ENPASSANT;
        break;
    }
    case chess::Move::PROMOTION:
    {
        result = MOVE_TYPE_PROMOTION;
        break;
    }
    case chess::Move::CASTLING:
    {
        result = MOVE_TYPE_CASTLING;
        break;
    }
    default:
    {
        chess::PieceType _toPiece = _board.at(_move.to()).type();

        // CHECK
        if (_board.givesCheck(_move) != chess::CheckType::NO_CHECK)
        {
            result = MOVE_TYPE_CHECK;
        }
        // CAPTURE
        else if (_toPiece != chess::PieceType::NONE)
        {
            result = MOVE_TYPE_CAPTURE;
        }
        else
        {
            result = MOVE_TYPE_NORMAL;
        }

        break;
    }
    }

    return result;
}

#pragma warning(push)
#pragma warning(disable : 4715)
chess_internal inline chess::PieceGenType GetExternalPieceGenType(chess::PieceType _pieceType)
{
    // clang-format off
	switch (_pieceType.internal())
	{
	case chess::PieceType::PAWN:   { return chess::PieceGenType::PAWN; }
	case chess::PieceType::BISHOP: { return chess::PieceGenType::BISHOP; }
	case chess::PieceType::KNIGHT: { return chess::PieceGenType::KNIGHT; }
	case chess::PieceType::QUEEN:  { return chess::PieceGenType::QUEEN; }
	case chess::PieceType::KING:   { return chess::PieceGenType::KING; }
	case chess::PieceType::ROOK:   { return chess::PieceGenType::ROOK; }
	default:                       { CHESS_ASSERT(0); }
	}
    // clang-format on
}
#pragma warning(pop)

chess_internal inline chess::Board GetExternalBoard(Board* board)
{
    chess::Board _board{};
    bool         validFen = _board.setFen(board->fen);
    CHESS_ASSERT(validFen);
    return _board;
}

Move* BoardGetPieceMoveList(Board* board, u32 row, u32 col, u32* moveCount)
{
    CHESS_ASSERT(board);
    CHESS_ASSERT(moveCount);

    Move* result = nullptr;
    *moveCount   = 0;

    chess::Board     _board     = GetExternalBoard(board);
    u32              cellIndex  = CELL_INDEX(row, col);
    chess::Piece     _piece     = _board.at(cellIndex);
    chess::PieceType _pieceType = _piece.type();

    if (_pieceType == chess::PieceType::NONE)
    {
        *moveCount = 0;
    }
    else
    {
        chess::Movelist     _rawMovelist{};
        chess::Movelist     _movelist;
        chess::PieceGenType _genType = GetExternalPieceGenType(_pieceType);
        chess::movegen::legalmoves(_rawMovelist, _board, _genType);

        for (const auto& _move : _rawMovelist)
        {
            if (_move.from().index() == cellIndex)
            {
                _movelist.add(_move);
            }
        }

        u32 movelistSize = _movelist.size();
        *moveCount       = movelistSize;
        result           = new Move[movelistSize];
        for (u32 i = 0; i < movelistSize; i++)
        {
            chess::Move& _move = _movelist[i];

            Move* move = &result[i];
            move->from = (u32)_move.from().index();
            move->to   = (u32)_move.to().index();
            move->type = GetInternalMoveType(_board, _move);
        }
    }

    return result;
}

void FreePieceMoveList(Move* movelist) { delete[] movelist; }