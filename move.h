#pragma once
#include <cstdint>
#include "types.h"

// ------------------------------------------------------------
// Move encoding (32-bit packed):
//
// bits  0–5   : from-square (0..63)
// bits  6–11  : to-square   (0..63)
// bits 12–15  : promotion piece (Promo enum)
// bits 16–19  : flags (MoveFlag enum)
// bits 20–31  : unused
// ------------------------------------------------------------

using Move = uint32_t;

// Extractors
inline int from_sq(Move m) {
    return m & 0x3F;
}

inline int to_sq(Move m) {
    return (m >> 6) & 0x3F;
}

inline int promo_of(Move m) {
    return (m >> 12) & 0xF;
}

inline int flags_of(Move m) {
    return (m >> 16) & 0xF;
}

// ------------------------------------------------------------
// Promotion types
// ------------------------------------------------------------
enum Promo {
    PROMO_NONE = 0,
    PROMO_N = 1,
    PROMO_B = 2,
    PROMO_R = 3,
    PROMO_Q = 4
};

// ------------------------------------------------------------
// Move flags
// ------------------------------------------------------------
enum MoveFlag {
    FLAG_NONE = 0,
    FLAG_CAPTURE = 1,
    FLAG_DBL_PUSH = 2,
    FLAG_ENPASSANT = 3,
    FLAG_CASTLING = 4
};

// ------------------------------------------------------------
// Move constructor
// ------------------------------------------------------------
inline Move make_move(int from, int to, int promo = PROMO_NONE, int flags = FLAG_NONE) {
    return  (from & 0x3F)
        | ((to & 0x3F) << 6)
        | ((promo & 0x0F) << 12)
        | ((flags & 0x0F) << 16);
}
inline int move_flags(Move m) {
    return (m >> 16) & 0xF;
}


inline bool is_capture(Move m) {
    int f = move_flags(m);
    return (f & FLAG_CAPTURE) || (f & FLAG_ENPASSANT);
}

inline bool is_promo(Move m) {
    int promo = (m >> 16) & 0xF;   // depends on your encoding
    return promo != PROMO_NONE;
}

// ------------------------------------------------------------
// UCI / debug formatting
// ------------------------------------------------------------
const char* move_to_string(Move m);
