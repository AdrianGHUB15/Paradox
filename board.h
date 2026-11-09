#pragma once

#include "bitboard.h"
#include "search.h"
#include "zobrist.h"

#include <cstring>

struct State {
    Move move;
    Piece capturedPiece;
    int capturedSquare;

    uint8_t castling;
    int epSquare;
    int halfmoveClock;
    int kingSq[2];

    uint64_t hash;
    Color stm;
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

    inline bool is_repetition() const {
        int count = 0;

        for (int i = repLen - 1; i >= 0; --i) {
            if (repHistory[i] == hash) {
                ++count;

                if (count >= 2)
                    return true;
            }
        }
        return false;
    }

    void print() const;
};

// in_check using magic sliding attacks
inline bool in_check(const Board& pos, Color side) {

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
    if (PAWN_ATTACKS[them ^ 1][ksq] & pawns)
        return true;

    // Knight
    if (KNIGHT_ATTACKS[ksq] & knights)
        return true;
    
    // King
    if (KING_ATTACKS[ksq] & kings) 
        return true;

    // Bishop / queen diagonals
    if (bishop_attack(ksq, occ) & (bishops | queens)) 
        return true;

    // Rook / queen orthogonals
    if (rook_attack(ksq, occ) & (rooks | queens))
        return true;
    
    return false;
}

// make_move / unmake_move
inline bool Board::make_move(Move m, State& st) {
    nodes++;

    int from = from_sq(m);
    int to = to_sq(m);
    int promo = promo_of(m);
    int flag = flags_of(m);

    Color us = stm;
    Color them = Color(us ^ 1);

    Piece pc = piece_at(from);
    if (pc == NO_PIECE)
        return false;

    // Save undo information
    st.move = m;
    st.capturedPiece = NO_PIECE;
    st.capturedSquare = -1;

    st.castling = castling;
    st.epSquare = epSquare;
    st.halfmoveClock = halfmoveClock;
    st.kingSq[WHITE] = kingSq[WHITE];
    st.kingSq[BLACK] = kingSq[BLACK];
    st.hash = hash;
    st.stm = stm;

    Bitboard fromBB = 1ULL << from;
    Bitboard toBB = 1ULL << to;

    // Validate castling before modifying the board
    if (flag == FLAG_CASTLING) {
        if (us == WHITE) {
            if (to == 6) {
                if (!(castling & CASTLE_WK)
                    || in_check(*this, WHITE)
                    || square_attacked(5, BLACK)
                    || square_attacked(6, BLACK)
                    || !(pieceBB[WHITE][ROOK] & (1ULL << 7)))
                    return false;
            }
            else if (to == 2) {
                if (!(castling & CASTLE_WQ)
                    || in_check(*this, WHITE)
                    || square_attacked(3, BLACK)
                    || square_attacked(2, BLACK)
                    || !(pieceBB[WHITE][ROOK] & (1ULL << 0)))
                    return false;
            }
            else return false;
        }
        else {
            if (to == 62) {
                if (!(castling & CASTLE_BK)
                    || in_check(*this, BLACK)
                    || square_attacked(61, WHITE)
                    || square_attacked(62, WHITE)
                    || !(pieceBB[BLACK][ROOK] & (1ULL << 63)))
                    return false;
            }
            else if (to == 58) {
                if (!(castling & CASTLE_BQ)
                    || in_check(*this, BLACK)
                    || square_attacked(59, WHITE)
                    || square_attacked(58, WHITE)
                    || !(pieceBB[BLACK][ROOK] & (1ULL << 56)))
                    return false;
            }
            else return false;
        }
    }

    // Find and save captured piece
    if (flag == FLAG_CAPTURE || flag == FLAG_ENPASSANT) {
        int capSq = (flag == FLAG_ENPASSANT)
            ? (us == WHITE ? to - 8 : to + 8)
            : to;

        Piece cap = piece_at(capSq);

        if (cap != NO_PIECE) {
            st.capturedPiece = cap;
            st.capturedSquare = capSq;

            Bitboard capBB = 1ULL << capSq;

            pieceBB[them][cap] &= ~capBB;
            colorBB[them] &= ~capBB;
            occupiedBB &= ~capBB;
            hash ^= ZP[them][cap][capSq];

            if (capSq == 0)  castling &= ~2;
            if (capSq == 7)  castling &= ~1;
            if (capSq == 56) castling &= ~8;
            if (capSq == 63) castling &= ~4;
        }
    }

    // Remove moving piece from source
    pieceBB[us][pc] &= ~fromBB;
    colorBB[us] &= ~fromBB;
    occupiedBB &= ~fromBB;
    hash ^= ZP[us][pc][from];

    // Update castling rights
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

    // Handle promotion
    Piece newPc = pc;

    if (promo != PROMO_NONE) {
        newPc = (promo == PROMO_N ? KNIGHT :
            promo == PROMO_B ? BISHOP :
            promo == PROMO_R ? ROOK :
            QUEEN);
    }

    // Place moving piece on destination
    pieceBB[us][newPc] |= toBB;
    colorBB[us] |= toBB;
    occupiedBB |= toBB;
    hash ^= ZP[us][newPc][to];

    if (pc == KING)
        kingSq[us] = to;

    // Move castling rook
    if (flag == FLAG_CASTLING) {
        int rookFrom, rookTo;

        if (to == 6) { rookFrom = 7;  rookTo = 5; }
        else if (to == 2) { rookFrom = 0;  rookTo = 3; }
        else if (to == 62) { rookFrom = 63; rookTo = 61; }
        else { rookFrom = 56; rookTo = 59; }

        Bitboard rookFromBB = 1ULL << rookFrom;
        Bitboard rookToBB = 1ULL << rookTo;

        pieceBB[us][ROOK] &= ~rookFromBB;
        pieceBB[us][ROOK] |= rookToBB;

        colorBB[us] &= ~rookFromBB;
        colorBB[us] |= rookToBB;

        occupiedBB &= ~rookFromBB;
        occupiedBB |= rookToBB;

        hash ^= ZP[us][ROOK][rookFrom];
        hash ^= ZP[us][ROOK][rookTo];
    }

    // En passant square
    if (epSquare != -1)
        hash ^= Z_EP[epSquare & 7];

    epSquare = (flag == FLAG_DBL_PUSH)
        ? (us == WHITE ? to - 8 : to + 8)
        : -1;

    if (epSquare != -1)
        hash ^= Z_EP[epSquare & 7];

    // Castling hash
    hash ^= Z_CASTLE[st.castling];
    hash ^= Z_CASTLE[castling];

    // Change side to move
    stm = them;
    hash ^= Z_STM;

    // Repetition history
    repHistory[repLen++] = hash;

    return true;
}

inline void Board::unmake_move(const State& st) {
    if (repLen > 0)
        repLen--;

    Move m = st.move;

    int from = from_sq(m);
    int to = to_sq(m);
    int promo = promo_of(m);
    int flag = flags_of(m);

    Color us = st.stm;
    Color them = Color(us ^ 1);

    Piece pc = piece_at(to);

    Bitboard fromBB = 1ULL << from;
    Bitboard toBB = 1ULL << to;

    // Remove the moved piece from destination
    pieceBB[us][pc] &= ~toBB;
    colorBB[us] &= ~toBB;
    occupiedBB &= ~toBB;

    // Restore original piece at source
    Piece originalPc = (promo != PROMO_NONE) ? PAWN : pc;

    pieceBB[us][originalPc] |= fromBB;
    colorBB[us] |= fromBB;
    occupiedBB |= fromBB;

    // Undo castling rook movement
    if (flag == FLAG_CASTLING) {
        int rookFrom, rookTo;

        if (to == 6) { rookFrom = 7;  rookTo = 5; }
        else if (to == 2) { rookFrom = 0;  rookTo = 3; }
        else if (to == 62) { rookFrom = 63; rookTo = 61; }
        else { rookFrom = 56; rookTo = 59; }

        Bitboard rookFromBB = 1ULL << rookFrom;
        Bitboard rookToBB = 1ULL << rookTo;

        pieceBB[us][ROOK] &= ~rookToBB;
        pieceBB[us][ROOK] |= rookFromBB;

        colorBB[us] &= ~rookToBB;
        colorBB[us] |= rookFromBB;

        occupiedBB &= ~rookToBB;
        occupiedBB |= rookFromBB;
    }

    // Restore captured piece
    if (st.capturedPiece != NO_PIECE) {
        Bitboard capBB = 1ULL << st.capturedSquare;

        pieceBB[them][st.capturedPiece] |= capBB;
        colorBB[them] |= capBB;
        occupiedBB |= capBB;
    }

    // Restore metadata
    castling = st.castling;
    epSquare = st.epSquare;
    halfmoveClock = st.halfmoveClock;

    kingSq[WHITE] = st.kingSq[WHITE];
    kingSq[BLACK] = st.kingSq[BLACK];

    hash = st.hash;
    stm = st.stm;
}

// square_attacked using magic sliding attacks
inline bool Board::square_attacked(int sq, Color by) const {

    Bitboard occ = occupiedBB;
    Bitboard pawns = pieceBB[by][PAWN];
    Bitboard knights = pieceBB[by][KNIGHT];
    Bitboard kings = pieceBB[by][KING];
    Bitboard bishops = pieceBB[by][BISHOP];
    Bitboard rooks = pieceBB[by][ROOK];
    Bitboard queens = pieceBB[by][QUEEN];

    // Pawn attacks
    if (PAWN_ATTACKS[by ^ 1][sq] & pawns) 
        return true;
    

    // Knight
    if (KNIGHT_ATTACKS[sq] & knights) 
        return true;
    

    // King
    if (KING_ATTACKS[sq] & kings) 
        return true;
    

    // Bishop / queen diagonals
    if (bishop_attack(sq, occ) & (bishops | queens)) 
        return true;
    

    // Rook / queen orthogonals
    if (rook_attack(sq, occ) & (rooks | queens)) 
        return true;
    

    return false;
}