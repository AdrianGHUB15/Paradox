#pragma once
#include "types.h"
#include "bitboard.h"
#include "move.h"
#include "zobrist.h"
#include <cstring>
#include <cstdint>
#include "profile.h"

struct State {
    uint8_t   castling;
    int       epSquare;
    int       halfmoveClock;
    int       kingSq[2];
    Bitboard  pieceBB[2][6];
    Bitboard  colorBB[2];
    Bitboard  occupiedBB;
    uint64_t  hash;
    Color     stm;
};

class Board {
public:
    Bitboard pieceBB[2][6];
    Bitboard colorBB[2];
    Bitboard occupiedBB;

    int   kingSq[2];
    Color stm;

    uint8_t  castling;
    int      epSquare;

    int      halfmoveClock;
    int      fullmoveNumber;

    uint64_t hash;

    // --- repetition history ---
    uint64_t repHistory[1024];
    int      repLen = 0;

public:
    Board();

    void set_fen(const char* fen);

    Piece piece_at(int sq) const;
    Color color_at(int sq) const;

    inline bool square_attacked(int sq, Color by) const;

    void make_null_move(State& st);
    void unmake_null_move(const State& st);

    int  non_pawn_material(Color c) const;

    bool sanity_check() const;

    bool make_move(Move m, State& st);
    void unmake_move(const State& st);

    inline bool is_repetition(int ply) const {
        int start = repLen - ply;
        if (start < 0) start = 0;

        uint64_t h = hash;

        // check same-side-to-move positions
        for (int i = repLen - 2; i >= start; i -= 2) {
            if (repHistory[i] == h)
                return true;
        }
        return false;
    }

    void print() const;
};

// --------------------------------------------------------
// in_check using magic sliding attacks
// --------------------------------------------------------
inline bool in_check(const Board& pos, Color side) {
    uint64_t t0 = now_ns();
    prof.in_check_calls++;

    Color them = Color(side ^ 1);
    int ksq = pos.kingSq[side];

    Bitboard pawns = pos.pieceBB[them][PAWN];
    Bitboard knights = pos.pieceBB[them][KNIGHT];
    Bitboard kings = pos.pieceBB[them][KING];
    Bitboard bishops = pos.pieceBB[them][BISHOP];
    Bitboard rooks = pos.pieceBB[them][ROOK];
    Bitboard queens = pos.pieceBB[them][QUEEN];
    Bitboard occ = pos.occupiedBB;

    // Pawn attacks
    if (PAWN_ATTACKS[them ^ 1][ksq] & pawns) {
        prof.in_check_ns += now_ns() - t0;
        return true;
    }

    // Knight
    if (KNIGHT_ATTACKS[ksq] & knights) {
        prof.in_check_ns += now_ns() - t0;
        return true;
    }

    // King
    if (KING_ATTACKS[ksq] & kings) {
        prof.in_check_ns += now_ns() - t0;
        return true;
    }

    // Bishop / queen diagonals
    if (bishop_attack(ksq, occ) & (bishops | queens)) {
        prof.in_check_ns += now_ns() - t0;
        return true;
    }

    // Rook / queen orthogonals
    if (rook_attack(ksq, occ) & (rooks | queens)) {
        prof.in_check_ns += now_ns() - t0;
        return true;
    }

    prof.in_check_ns += now_ns() - t0;
    return false;
}

// --------------------------------------------------------
// make_move / unmake_move
// --------------------------------------------------------
inline bool Board::make_move(Move m, State& st) {
    uint64_t t0 = now_ns();
    prof.make_move_calls++;

    // Save state
    st.castling = castling;
    st.epSquare = epSquare;
    st.halfmoveClock = halfmoveClock;
    st.kingSq[WHITE] = kingSq[WHITE];
    st.kingSq[BLACK] = kingSq[BLACK];
    std::memcpy(st.pieceBB, pieceBB, sizeof(pieceBB));
    std::memcpy(st.colorBB, colorBB, sizeof(colorBB));
    st.occupiedBB = occupiedBB;
    st.hash = hash;
    st.stm = stm;

    int from = from_sq(m);
    int to = to_sq(m);
    int promo = promo_of(m);
    int flag = flags_of(m);

    Color us = stm;
    Color them = Color(us ^ 1);

    Piece pc = piece_at(from);
    if (pc == NO_PIECE) {
        prof.make_move_ns += now_ns() - t0;
        return false;
    }

    Bitboard fromBB = 1ULL << from;
    Bitboard toBB = 1ULL << to;

    // --- VALIDATE CASTLING BEFORE MODIFYING BOARD ---
    if (flag == FLAG_CASTLING) {
        // White
        if (us == WHITE) {
            if (to == 6) { // e1->g1
                if (!(castling & CASTLE_WK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (in_check(*this, WHITE)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(5, BLACK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(6, BLACK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (!(pieceBB[WHITE][ROOK] & (1ULL << 7))) { prof.make_move_ns += now_ns() - t0; return false; }
            }
            else if (to == 2) { // e1->c1
                if (!(castling & CASTLE_WQ)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (in_check(*this, WHITE)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(3, BLACK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(2, BLACK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (!(pieceBB[WHITE][ROOK] & (1ULL << 0))) { prof.make_move_ns += now_ns() - t0; return false; }
            }
            else {
                prof.make_move_ns += now_ns() - t0;
                return false;
            }
        }
        // Black
        else {
            if (to == 62) { // e8->g8
                if (!(castling & CASTLE_BK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (in_check(*this, BLACK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(61, WHITE)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(62, WHITE)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (!(pieceBB[BLACK][ROOK] & (1ULL << 63))) { prof.make_move_ns += now_ns() - t0; return false; }
            }
            else if (to == 58) { // e8->c8
                if (!(castling & CASTLE_BQ)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (in_check(*this, BLACK)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(59, WHITE)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (square_attacked(58, WHITE)) { prof.make_move_ns += now_ns() - t0; return false; }
                if (!(pieceBB[BLACK][ROOK] & (1ULL << 56))) { prof.make_move_ns += now_ns() - t0; return false; }
            }
            else {
                prof.make_move_ns += now_ns() - t0;
                return false;
            }
        }
    }

    // remove piece from source
    pieceBB[us][pc] &= ~fromBB;
    colorBB[us] &= ~fromBB;
    occupiedBB &= ~fromBB;
    hash ^= ZP[us][pc][from];

    // captures
    if (flag == FLAG_CAPTURE || flag == FLAG_ENPASSANT) {
        int capSq = to;
        if (flag == FLAG_ENPASSANT)
            capSq = (us == WHITE ? to - 8 : to + 8);

        Piece cap = piece_at(capSq);
        if (cap != NO_PIECE) {
            Bitboard capBB = 1ULL << capSq;

            pieceBB[them][cap] &= ~capBB;
            colorBB[them] &= ~capBB;
            occupiedBB &= ~capBB;
            hash ^= ZP[them][cap][capSq];

            // captured rook → update castling rights
            if (capSq == 0)  castling &= ~2;
            if (capSq == 7)  castling &= ~1;
            if (capSq == 56) castling &= ~8;
            if (capSq == 63) castling &= ~4;
        }
    }

    // king or rook moved → remove castling rights
    if (pc == KING) {
        if (us == WHITE) castling &= ~(1 | 2);
        else             castling &= ~(4 | 8);
    }
    else if (pc == ROOK) {
        if (from == 0)  castling &= ~2;
        if (from == 7)  castling &= ~1;
        if (from == 56) castling &= ~8;
        if (from == 63) castling &= ~4;
    }

    // promotions
    Piece newPc = pc;
    if (promo != PROMO_NONE) {
        newPc = (promo == PROMO_N ? KNIGHT :
            promo == PROMO_B ? BISHOP :
            promo == PROMO_R ? ROOK :
            QUEEN);
    }

    // add piece to destination
    pieceBB[us][newPc] |= toBB;
    colorBB[us] |= toBB;
    occupiedBB |= toBB;
    hash ^= ZP[us][newPc][to];

    if (pc == KING)
        kingSq[us] = to;

    // castling rook move (board already validated above)
    if (flag == FLAG_CASTLING) {
        if (to == 6) { // white king side
            pieceBB[WHITE][ROOK] &= ~(1ULL << 7);
            pieceBB[WHITE][ROOK] |= (1ULL << 5);
            colorBB[WHITE] &= ~(1ULL << 7);
            colorBB[WHITE] |= (1ULL << 5);
            occupiedBB &= ~(1ULL << 7);
            occupiedBB |= (1ULL << 5);
            hash ^= ZP[WHITE][ROOK][7];
            hash ^= ZP[WHITE][ROOK][5];
        }
        else if (to == 2) { // white queen side
            pieceBB[WHITE][ROOK] &= ~(1ULL << 0);
            pieceBB[WHITE][ROOK] |= (1ULL << 3);
            colorBB[WHITE] &= ~(1ULL << 0);
            colorBB[WHITE] |= (1ULL << 3);
            occupiedBB &= ~(1ULL << 0);
            occupiedBB |= (1ULL << 3);
            hash ^= ZP[WHITE][ROOK][0];
            hash ^= ZP[WHITE][ROOK][3];
        }
        else if (to == 62) { // black king side
            pieceBB[BLACK][ROOK] &= ~(1ULL << 63);
            pieceBB[BLACK][ROOK] |= (1ULL << 61);
            colorBB[BLACK] &= ~(1ULL << 63);
            colorBB[BLACK] |= (1ULL << 61);
            occupiedBB &= ~(1ULL << 63);
            occupiedBB |= (1ULL << 61);
            hash ^= ZP[BLACK][ROOK][63];
            hash ^= ZP[BLACK][ROOK][61];
        }
        else if (to == 58) { // black queen side
            pieceBB[BLACK][ROOK] &= ~(1ULL << 56);
            pieceBB[BLACK][ROOK] |= (1ULL << 59);
            colorBB[BLACK] &= ~(1ULL << 56);
            colorBB[BLACK] |= (1ULL << 59);
            occupiedBB &= ~(1ULL << 56);
            occupiedBB |= (1ULL << 59);
            hash ^= ZP[BLACK][ROOK][56];
            hash ^= ZP[BLACK][ROOK][59];
        }
    }

    // EP hash out
    if (epSquare != -1)
        hash ^= Z_EP[epSquare & 7];

    // EP square
    if (flag == FLAG_DBL_PUSH)
        epSquare = (us == WHITE ? to - 8 : to + 8);
    else
        epSquare = -1;

    if (epSquare != -1)
        hash ^= Z_EP[epSquare & 7];

    // Castling hash
    hash ^= Z_CASTLE[st.castling];
    hash ^= Z_CASTLE[castling];

    // Side to move
    stm = them;
    hash ^= Z_STM;

    // repetition push
   // repHistory[repLen++] = hash;

    prof.make_move_ns += now_ns() - t0;
    return true;
}

inline void Board::unmake_move(const State& st) {
    uint64_t t0 = now_ns();
    prof.unmake_move_calls++;
    repLen--;

    castling = st.castling;
    epSquare = st.epSquare;
    halfmoveClock = st.halfmoveClock;
    kingSq[WHITE] = st.kingSq[WHITE];
    kingSq[BLACK] = st.kingSq[BLACK];
    std::memcpy(pieceBB, st.pieceBB, sizeof(pieceBB));
    std::memcpy(colorBB, st.colorBB, sizeof(colorBB));
    occupiedBB = st.occupiedBB;
    hash = st.hash;
    stm = st.stm;

    prof.unmake_move_ns += now_ns() - t0;
}

// --------------------------------------------------------
// square_attacked using magic sliding attacks
// --------------------------------------------------------
inline bool Board::square_attacked(int sq, Color by) const {
    uint64_t t0 = now_ns();
    prof.square_attacked_calls++;

    Bitboard occ = occupiedBB;
    Bitboard pawns = pieceBB[by][PAWN];
    Bitboard knights = pieceBB[by][KNIGHT];
    Bitboard kings = pieceBB[by][KING];
    Bitboard bishops = pieceBB[by][BISHOP];
    Bitboard rooks = pieceBB[by][ROOK];
    Bitboard queens = pieceBB[by][QUEEN];

    // Pawn attacks
    if (PAWN_ATTACKS[by ^ 1][sq] & pawns) {
        prof.square_attacked_ns += now_ns() - t0;
        return true;
    }

    // Knight
    if (KNIGHT_ATTACKS[sq] & knights) {
        prof.square_attacked_ns += now_ns() - t0;
        return true;
    }

    // King
    if (KING_ATTACKS[sq] & kings) {
        prof.square_attacked_ns += now_ns() - t0;
        return true;
    }

    // Bishop / queen diagonals
    if (bishop_attack(sq, occ) & (bishops | queens)) {
        prof.square_attacked_ns += now_ns() - t0;
        return true;
    }

    // Rook / queen orthogonals
    if (rook_attack(sq, occ) & (rooks | queens)) {
        prof.square_attacked_ns += now_ns() - t0;
        return true;
    }

    prof.square_attacked_ns += now_ns() - t0;
    return false;
}

// --------------------------------------------------------
// Null move
// --------------------------------------------------------
inline void Board::make_null_move(State& st) {
    uint64_t t0 = now_ns();
    prof.null_make_calls++;

    // Save state
    st.castling = castling;
    st.epSquare = epSquare;
    st.halfmoveClock = halfmoveClock;
    st.kingSq[WHITE] = kingSq[WHITE];
    st.kingSq[BLACK] = kingSq[BLACK];
    std::memcpy(st.pieceBB, pieceBB, sizeof(pieceBB));
    std::memcpy(st.colorBB, colorBB, sizeof(colorBB));
    st.occupiedBB = occupiedBB;
    st.hash = hash;
    st.stm = stm;

    // Remove EP hash if present
    if (epSquare != -1)
        hash ^= Z_EP[epSquare & 7];

    // Clear EP
    epSquare = -1;

    // Flip side
    stm = Color(stm ^ 1);
    hash ^= Z_STM;

//    repHistory[repLen++] = hash;

    prof.null_make_ns += now_ns() - t0;
}

inline void Board::unmake_null_move(const State& st) {
    uint64_t t0 = now_ns();
    prof.null_unmake_calls++;
    repLen--;

    castling = st.castling;
    epSquare = st.epSquare;
    halfmoveClock = st.halfmoveClock;
    kingSq[WHITE] = st.kingSq[WHITE];
    kingSq[BLACK] = st.kingSq[BLACK];
    std::memcpy(pieceBB, st.pieceBB, sizeof(pieceBB));
    std::memcpy(colorBB, st.colorBB, sizeof(colorBB));
    occupiedBB = st.occupiedBB;
    hash = st.hash;
    stm = st.stm;

    prof.null_unmake_ns += now_ns() - t0;
}

// --------------------------------------------------------
// Non-pawn material
// --------------------------------------------------------
inline int Board::non_pawn_material(Color c) const {
    int total = 0;

    total += popcount(pieceBB[c][KNIGHT]) * 300;
    total += popcount(pieceBB[c][BISHOP]) * 300;
    total += popcount(pieceBB[c][ROOK]) * 500;
    total += popcount(pieceBB[c][QUEEN]) * 900;

    return total;
}
inline bool Board::sanity_check() const {

    // 1. Side to move must be valid
    if (stm != WHITE && stm != BLACK)
        return false;

    // 2. King squares must be on board
    if (kingSq[WHITE] < 0 || kingSq[WHITE] >= 64)
        return false;
    if (kingSq[BLACK] < 0 || kingSq[BLACK] >= 64)
        return false;

    // 3. King bitboards must contain exactly one king
    Bitboard wk = pieceBB[WHITE][KING];
    Bitboard bk = pieceBB[BLACK][KING];
    if (wk == 0 || bk == 0) return false;
    if (wk & (wk - 1)) return false; // more than one white king
    if (bk & (bk - 1)) return false; // more than one black king

    // 4. colorBB must match pieceBB
    Bitboard cW = 0, cB = 0;
    for (int p = 0; p < 6; ++p) {
        cW |= pieceBB[WHITE][p];
        cB |= pieceBB[BLACK][p];
    }
    if (cW != colorBB[WHITE]) return false;
    if (cB != colorBB[BLACK]) return false;

    // 5. occupied must match union
    if (occupiedBB != (cW | cB)) return false;

    // 6. no overlap
    if (cW & cB) return false;

    // 7. king squares must match bitboards
    if (!(pieceBB[WHITE][KING] & (1ULL << kingSq[WHITE]))) return false;
    if (!(pieceBB[BLACK][KING] & (1ULL << kingSq[BLACK]))) return false;

    return true;
}

