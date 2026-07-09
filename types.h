#pragma once
#include <cstdint>

enum Color : int {
    WHITE = 0,
    BLACK = 1,
    NO_COLOR = 2
};

enum Piece : int {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NO_PIECE = 6
};
enum CastlingRights {
    CASTLE_WK = 1,
    CASTLE_WQ = 2,
    CASTLE_BK = 4,
    CASTLE_BQ = 8
};

using Bitboard = uint64_t;

inline Color operator^(Color c, int x) {
    return Color(int(c) ^ x);
}
