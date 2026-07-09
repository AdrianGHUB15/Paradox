#pragma once
#include "types.h"
#include <cstdint>

typedef uint64_t Bitboard;

// --------------------------------------------
// Portable bit operations
// --------------------------------------------
#ifdef _MSC_VER
#include <intrin.h>

inline int lsb_index(uint64_t b) {
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return int(idx);
}

inline int popcount64(uint64_t b) {
    return int(__popcnt64(b));
}

inline int msb(uint64_t b) {
    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return int(idx);
}

inline Bitboard isolate_lsb(Bitboard b) {
    return b & (~b + 1ULL);
}

inline int pop_lsb_index(Bitboard& b) {
    Bitboard l = isolate_lsb(b);
    int sq = lsb_index(l);
    b ^= l;
    return sq;
}

inline int pop_lsb(Bitboard& b) { return pop_lsb_index(b); }
inline int lsb(Bitboard b) { return lsb_index(b); }

#else

inline int lsb(uint64_t b) {
    return int(__builtin_ctzll(b));
}

inline int pop_lsb(Bitboard& b) {
    Bitboard l = b & -b;
    int sq = lsb(l);
    b ^= l;
    return sq;
}

inline int popcount64(uint64_t b) {
    return int(__builtin_popcountll(b));
}

inline int msb(uint64_t b) {
    return 63 - int(__builtin_clzll(b));
}

#endif

inline int popcount(uint64_t b) { return popcount64(b); }

// --------------------------------------------
// Non‑sliding attack tables
// --------------------------------------------
extern Bitboard KNIGHT_ATTACKS[64];
extern Bitboard KING_ATTACKS[64];
extern Bitboard PAWN_ATTACKS[2][64];

inline Bitboard attacks_knight(int sq) { return KNIGHT_ATTACKS[sq]; }
inline Bitboard attacks_king(int sq) { return KING_ATTACKS[sq]; }
inline Bitboard attacks_pawn(Color c, int sq) { return PAWN_ATTACKS[c][sq]; }

// --------------------------------------------
// File/rank/passed pawn masks
// --------------------------------------------
extern Bitboard file_mask[8];
extern Bitboard rank_mask[8];
extern Bitboard adjacent_files_mask[8];
extern Bitboard passed_mask[2][64];

// --------------------------------------------
// Magic bitboard infrastructure
// --------------------------------------------
extern Bitboard bishopMasks[64];
extern Bitboard rookMasks[64];

extern Bitboard bishopMagics[64];
extern Bitboard rookMagics[64];

extern int bishopShifts[64];
extern int rookShifts[64];

extern Bitboard bishopAttacks[64][512];   // 2^9 max
extern Bitboard rookAttacks[64][4096];    // 2^12 max

// --------------------------------------------
// Pawn pushes
// --------------------------------------------
extern Bitboard PAWN_PUSH[2][64];
extern Bitboard PAWN_PUSH2[2][64];

// --------------------------------------------
// Magic lookup functions
// --------------------------------------------
inline Bitboard bishop_attack(int sq, Bitboard occ) {
    Bitboard o = occ & bishopMasks[sq];
    o *= bishopMagics[sq];
    o >>= bishopShifts[sq];
    return bishopAttacks[sq][o];
}

inline Bitboard rook_attack(int sq, Bitboard occ) {
    Bitboard o = occ & rookMasks[sq];
    o *= rookMagics[sq];
    o >>= rookShifts[sq];
    return rookAttacks[sq][o];
}

inline Bitboard queen_attack(int sq, Bitboard occ) {
    return bishop_attack(sq, occ) | rook_attack(sq, occ);
}

// --------------------------------------------
// Initialization
// --------------------------------------------
void init_bitboards();   // knight/king/pawn + masks + pushes
void init_magics();      // magic tables + sliding attacks
