#include <Disservin/chess.hpp>

#define CELL_INDEX(row, col)       (col + row * 8)
#define CELL_ROW(index)            (index / 8)
#define CELL_COL(index)            (index % 8)
#define VALIDATE_CELL_INDEX(index) CHESS_ASSERT(index >= 0 && index <= 63)

#define DEFAULT_FEN_STRING "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

chess_internal inline u32                 GetInternalMoveType(chess::Board& _board, chess::Move& _move);
chess_internal inline chess::PieceGenType GetExternalPieceGenType(chess::PieceType _pieceType);
chess_internal inline chess::Board        GetExternalBoard(Board* board);

Board BoardCreate(const char* fen)
{
    Board        board;
    Board::Delta state{ Move{}, fen };
    board.deltas.push(state);
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
    bool         validFen = _board.setFen(board->deltas.peek().fen);
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

void FreePieceMoveList(Move* movelist)
{
    if (movelist)
    {
        delete[] movelist;
    }
}

void BoardMoveDo(Board* board, Move* move)
{
    CHESS_ASSERT(board);
    CHESS_ASSERT(move);

    chess::Board _board = GetExternalBoard(board);
    chess::Move  _move  = chess::uci::uciToMove(_board, move->uci);

    _board.makeMove(_move);
    board->deltas.push(Board::Delta{ *move, _board.getFen() });
}

void BoardMoveUndo(Board* board)
{
    CHESS_ASSERT(board);
    CHESS_ASSERT(BoardMoveCanUndo(board));

    board->deltas.pop();
}

bool BoardMoveCanUndo(Board* board)
{
    CHESS_ASSERT(board);
    return board->deltas.top >= 2;
}

Move BoardMoveGetLast(Board* board)
{
    CHESS_ASSERT(board);
    return board->deltas.peek().lastMove;
}

u32 BoardGetTurn(Board* board)
{
    CHESS_ASSERT(board);

    chess::Board _board = GetExternalBoard(board);
    chess::Color _color = _board.sideToMove();

    u32 color;

    switch (_color)
    {
    case chess::Color::WHITE:
    {
        color = PIECE_COLOR_WHITE;
        break;
    }
    case chess::Color::BLACK:
    {
        color = PIECE_COLOR_BLACK;
        break;
    }
    case chess::Color::NONE:
    {
        color = PIECE_COLOR_NONE;
        break;
    }
    default:
    {
        CHESS_ASSERT(0);
    }
    }

    return color;
}

u32 BoardGetGameResult(Board* board)
{
    CHESS_ASSERT(board);

    chess::Board                                          _board      = GetExternalBoard(board);
    std::pair<chess::GameResultReason, chess::GameResult> _result     = _board.isGameOver();
    chess::GameResult                                     _gameResult = std::get<1>(_result);
    chess::Color                                          _color      = _board.sideToMove();

    u32 result;

    switch (_gameResult)
    {
    case chess::GameResult::WIN:
    {
        if (_color == chess::Color::WHITE)
        {
            result = BOARD_GAME_RESULT_WIN;
        }
        else
        {
            result = BOARD_GAME_RESULT_LOSE;
        }
        break;
    }
    case chess::GameResult::LOSE:
    {
        if (_color == chess::Color::WHITE)
        {
            result = BOARD_GAME_RESULT_LOSE;
        }
        else
        {
            result = BOARD_GAME_RESULT_WIN;
        }
        break;
    }
    case chess::GameResult::DRAW:
    {
        result = BOARD_GAME_RESULT_DRAW;
        break;
    }
    case chess::GameResult::NONE:
    {
        result = BOARD_GAME_RESULT_NONE;
        break;
    }
    default:
    {
        CHESS_ASSERT(0);
    }
    }

    return result;
}

bool BoardGameStarted(Board* board)
{
    CHESS_ASSERT(board);
    return board->deltas.peek().fen != DEFAULT_FEN_STRING;
}

bool BoardInCheck(Board* board)
{
    CHESS_ASSERT(board);

    chess::Board _board = GetExternalBoard(board);
    return _board.inCheck();
}

u32 BoardGetKingCell(Board* board)
{
    CHESS_ASSERT(board);
    chess::Board _board = GetExternalBoard(board);
    return (u32)_board.kingSq(_board.sideToMove()).index();
}