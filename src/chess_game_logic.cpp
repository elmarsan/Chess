#include <Disservin/chess.hpp>

#define CELL_INDEX(row, col)       (col + row * 8)
#define CELL_ROW(index)            (index / 8)
#define CELL_COL(index)            (index % 8)
#define VALIDATE_CELL_INDEX(index) CHESS_ASSERT(index >= 0 && index <= 63)

chess_internal inline u32                 GetInternalMoveType(chess::Board& _board, chess::Move& _move);
chess_internal inline chess::PieceGenType GetExternalPieceGenType(chess::PieceType _pieceType);
chess_internal inline chess::Board        GetExternalBoard(Board* board);

Board BoardCreate(const char* fen)
{
    Board board;
    strncpy(board.fen, fen, strlen(fen) + 1);
    return board;
}

Piece BoardGetPiece(Board* board, u32 cellIndex)
{
    CHESS_ASSERT(board);
    VALIDATE_CELL_INDEX(cellIndex);

    Piece result;
    result.cellIndex = cellIndex;

    chess::Board _board = GetExternalBoard(board);
    chess::Piece _piece = _board.at(cellIndex);

    switch (_piece.type().internal())
    {
    case chess::PieceType::PAWN:
    {
        result.type      = PIECE_TYPE_PAWN;
        result.meshIndex = MESH_PAWN;
        break;
    }
    case chess::PieceType::BISHOP:
    {
        result.type      = PIECE_TYPE_BISHOP;
        result.meshIndex = MESH_BISHOP;
        break;
    }
    case chess::PieceType::KNIGHT:
    {
        result.type      = PIECE_TYPE_KNIGHT;
        result.meshIndex = MESH_KNIGHT;
        break;
    }
    case chess::PieceType::ROOK:
    {
        result.type      = PIECE_TYPE_ROOK;
        result.meshIndex = MESH_ROOK;
        break;
    }

    case chess::PieceType::KING:
    {
        result.type      = PIECE_TYPE_KING;
        result.meshIndex = MESH_KING;
        break;
    }
    case chess::PieceType::QUEEN:
    {
        result.type      = PIECE_TYPE_QUEEN;
        result.meshIndex = MESH_QUEEN;
        break;
    }
    case chess::PieceType::NONE:
    {
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

Move* BoardGetPieceMoveList(Board* board, u32 cellIndex, u32* moveCount)
{
    CHESS_ASSERT(board);
    CHESS_ASSERT(moveCount);
    VALIDATE_CELL_INDEX(cellIndex);

    Move* result = nullptr;
    *moveCount   = 0;

    chess::Board     _board     = GetExternalBoard(board);
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

            std::string uciStr = chess::uci::moveToUci(_move);
            strncpy(move->uci, uciStr.c_str(), sizeof(move->uci) - 1);
            move->uci[sizeof(move->uci) - 1] = '\0';
        }
    }

    return result;
}

void FreePieceMoveList(Move* movelist) { delete[] movelist; }

// TODO: Validate all move types (promotion, castling, etc)
void BoardDoMove(Board* board, Move* move)
{
    CHESS_ASSERT(board);
    CHESS_ASSERT(move);

    chess::Board _board = GetExternalBoard(board);
    chess::Move  _move  = chess::uci::uciToMove(_board, move->uci);

    _board.makeMove(_move);
    const char* fenStr = _board.getFen().c_str();
    strncpy(board->fen, fenStr, strlen(fenStr) + 1);
}