#pragma once

#include <cstdint>
#include "types.h"

// ============================================================
// Move Encoding (16-bit)
//
// bits  0-5   : from square
// bits  6-11  : to square
// bits 12-15  : move type
// ============================================================

using Move = uint16_t;

// ============================================================
// Move Types
// ============================================================

enum MoveType : uint8_t {
    QUIET = 0,

    DOUBLE_PUSH,
    KING_CASTLE,
    QUEEN_CASTLE,

    CAPTURE,
    ENPASSANT,

    PROMO_N,
    PROMO_B,
    PROMO_R,
    PROMO_Q,

    PROMO_N_CAPTURE,
    PROMO_B_CAPTURE,
    PROMO_R_CAPTURE,
    PROMO_Q_CAPTURE
};

// ============================================================
// Legacy Compatibility
// ============================================================

constexpr int PROMO_NONE = 0;

constexpr int FLAG_NONE = 0;
constexpr int FLAG_CAPTURE = 1;
constexpr int FLAG_DBL_PUSH = 2;
constexpr int FLAG_ENPASSANT = 3;
constexpr int FLAG_CASTLING = 4;

// ============================================================
// Constructors
// ============================================================

inline Move make_move(int from, int to, int type = QUIET)
{
    return Move(
        (from & 0x3F) |
        ((to & 0x3F) << 6) |
        ((type & 0x0F) << 12)
    );
}

// Old API compatibility:
// make_move(from, to, promo, flags)
inline Move make_move(int from, int to, int promo, int flags)
{
    int type = QUIET;

    switch (flags)
    {
    case FLAG_CAPTURE:

        switch (promo)
        {
        case PROMO_N: type = PROMO_N_CAPTURE; break;
        case PROMO_B: type = PROMO_B_CAPTURE; break;
        case PROMO_R: type = PROMO_R_CAPTURE; break;
        case PROMO_Q: type = PROMO_Q_CAPTURE; break;
        default:      type = CAPTURE;         break;
        }

        break;

    case FLAG_ENPASSANT:
        type = ENPASSANT;
        break;

    case FLAG_DBL_PUSH:
        type = DOUBLE_PUSH;
        break;

    case FLAG_CASTLING:

        // temporary compatibility hack
        // both castles map here for now

        type = KING_CASTLE;
        break;

    default:

        switch (promo)
        {
        case PROMO_N: type = PROMO_N; break;
        case PROMO_B: type = PROMO_B; break;
        case PROMO_R: type = PROMO_R; break;
        case PROMO_Q: type = PROMO_Q; break;
        default:      type = QUIET;   break;
        }

        break;
    }

    return make_move(from, to, type);
}

// ============================================================
// Extractors
// ============================================================

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

// ============================================================
// Compatibility Accessors
// ============================================================

inline int flags_of(Move m)
{
    switch (move_type(m))
    {
    case CAPTURE:
    case PROMO_N_CAPTURE:
    case PROMO_B_CAPTURE:
    case PROMO_R_CAPTURE:
    case PROMO_Q_CAPTURE:
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

inline int promo_of(Move m)
{
    switch (move_type(m))
    {
    case PROMO_N:
    case PROMO_N_CAPTURE:
        return PROMO_N;

    case PROMO_B:
    case PROMO_B_CAPTURE:
        return PROMO_B;

    case PROMO_R:
    case PROMO_R_CAPTURE:
        return PROMO_R;

    case PROMO_Q:
    case PROMO_Q_CAPTURE:
        return PROMO_Q;

    default:
        return PROMO_NONE;
    }
}

// ============================================================
// Helpers
// ============================================================

inline bool is_capture(Move m)
{
    return flags_of(m) == FLAG_CAPTURE
        || flags_of(m) == FLAG_ENPASSANT;
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

    return t == KING_CASTLE
        || t == QUEEN_CASTLE;
}

inline bool is_double_push(Move m)
{
    return move_type(m) == DOUBLE_PUSH;
}

inline Piece promo_piece(Move m)
{
    switch (promo_of(m))
    {
    case PROMO_N: return KNIGHT;
    case PROMO_B: return BISHOP;
    case PROMO_R: return ROOK;
    case PROMO_Q: return QUEEN;
    default:      return NO_PIECE;
    }
}

// ============================================================
// UCI / debug
// ============================================================

const char* move_to_string(Move m);