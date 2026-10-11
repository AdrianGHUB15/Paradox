#pragma once

#include <cstdint>
#include "types.h"

using Move = uint16_t;

// Legacy promotion values. Keep these stable for board.h.
enum Promo : int {
    PROMO_NONE = 0,
    PROMO_N = 1,
    PROMO_B = 2,
    PROMO_R = 3,
    PROMO_Q = 4
};

// Legacy flags. Keep these stable for movegen.cpp and board.h.
enum MoveFlag : int {
    FLAG_NONE = 0,
    FLAG_CAPTURE = 1,
    FLAG_DBL_PUSH = 2,
    FLAG_ENPASSANT = 3,
    FLAG_CASTLING = 4
};

// Four bits available in the packed move.
enum MoveType : int {
    QUIET = 0,
    DOUBLE_PUSH,
    KING_CASTLE,
    QUEEN_CASTLE,
    CAPTURE,
    ENPASSANT,
    MOVE_PROMO_N,
    MOVE_PROMO_B,
    MOVE_PROMO_R,
    MOVE_PROMO_Q,
    MOVE_PROMO_N_CAPTURE,
    MOVE_PROMO_B_CAPTURE,
    MOVE_PROMO_R_CAPTURE,
    MOVE_PROMO_Q_CAPTURE
};

inline Move make_move_type(int from, int to, int type)
{
    return Move(
        (from & 0x3F) |
        ((to & 0x3F) << 6) |
        ((type & 0x0F) << 12)
    );
}

// Backward-compatible API: make_move(from, to, promo, flags).
inline Move make_move(int from, int to,
    int promo = PROMO_NONE,
    int flags = FLAG_NONE)
{
    int type = QUIET;

    switch (flags) {
    case FLAG_CAPTURE:
        switch (promo) {
        case PROMO_N: type = MOVE_PROMO_N_CAPTURE; break;
        case PROMO_B: type = MOVE_PROMO_B_CAPTURE; break;
        case PROMO_R: type = MOVE_PROMO_R_CAPTURE; break;
        case PROMO_Q: type = MOVE_PROMO_Q_CAPTURE; break;
        default:      type = CAPTURE; break;
        }
        break;

    case FLAG_DBL_PUSH:
        type = DOUBLE_PUSH;
        break;

    case FLAG_ENPASSANT:
        type = ENPASSANT;
        break;

    case FLAG_CASTLING:
        // e1-g1, e1-c1, e8-g8, e8-c8
        type = ((to & 7) == 6) ? KING_CASTLE : QUEEN_CASTLE;
        break;

    default:
        switch (promo) {
        case PROMO_N: type = MOVE_PROMO_N; break;
        case PROMO_B: type = MOVE_PROMO_B; break;
        case PROMO_R: type = MOVE_PROMO_R; break;
        case PROMO_Q: type = MOVE_PROMO_Q; break;
        default:      type = QUIET; break;
        }
        break;
    }

    return make_move_type(from, to, type);
}

inline int from_sq(Move m)
{
    return m & 0x3F;
}

inline int to_sq(Move m)
{
    return (m >> 6) & 0x3F;
}

inline int move_type(Move m)
{
    return (m >> 12) & 0x0F;
}

inline int flags_of(Move m)
{
    switch (move_type(m)) {
    case CAPTURE:
    case MOVE_PROMO_N_CAPTURE:
    case MOVE_PROMO_B_CAPTURE:
    case MOVE_PROMO_R_CAPTURE:
    case MOVE_PROMO_Q_CAPTURE:
        return FLAG_CAPTURE;

    case DOUBLE_PUSH:
        return FLAG_DBL_PUSH;

    case ENPASSANT:
        return FLAG_ENPASSANT;

    case KING_CASTLE:
    case QUEEN_CASTLE:
        return FLAG_CASTLING;

    default:
        return FLAG_NONE;
    }
}

inline int move_flags(Move m)
{
    return flags_of(m);
}

inline int promo_of(Move m)
{
    switch (move_type(m)) {
    case MOVE_PROMO_N:
    case MOVE_PROMO_N_CAPTURE:
        return PROMO_N;

    case MOVE_PROMO_B:
    case MOVE_PROMO_B_CAPTURE:
        return PROMO_B;

    case MOVE_PROMO_R:
    case MOVE_PROMO_R_CAPTURE:
        return PROMO_R;

    case MOVE_PROMO_Q:
    case MOVE_PROMO_Q_CAPTURE:
        return PROMO_Q;

    default:
        return PROMO_NONE;
    }
}

inline bool is_capture(Move m)
{
    return flags_of(m) == FLAG_CAPTURE ||
        flags_of(m) == FLAG_ENPASSANT;
}

inline bool is_promo(Move m)
{
    return promo_of(m) != PROMO_NONE;
}

inline bool is_ep(Move m)
{
    return move_type(m) == ENPASSANT;
}

inline bool is_castle(Move m)
{
    int t = move_type(m);
    return t == KING_CASTLE || t == QUEEN_CASTLE;
}

inline bool is_double_push(Move m)
{
    return move_type(m) == DOUBLE_PUSH;
}

inline Piece promo_piece(Move m)
{
    switch (promo_of(m)) {
    case PROMO_N: return KNIGHT;
    case PROMO_B: return BISHOP;
    case PROMO_R: return ROOK;
    case PROMO_Q: return QUEEN;
    default:      return NO_PIECE;
    }
}

const char* move_to_string(Move m);