#include "see.h"
#include "bitboard.h"
#include <algorithm>

static inline int see_value(Piece p) {
    static const int val[6] = { 100, 300, 300, 500, 900, 20000 };
    return val[p];
}

static Bitboard attackers_to(const Board& pos, int sq, Bitboard occ, Color side) {
    Bitboard atk = 0;

    atk |= attacks_pawn(Color(side ^ 1), sq) & pos.pieceBB[side][PAWN];
    atk |= attacks_knight(sq) & pos.pieceBB[side][KNIGHT];
    atk |= bishop_attack(sq, occ) & (pos.pieceBB[side][BISHOP] | pos.pieceBB[side][QUEEN]);
    atk |= rook_attack(sq, occ) & (pos.pieceBB[side][ROOK] | pos.pieceBB[side][QUEEN]);
    atk |= attacks_king(sq) & pos.pieceBB[side][KING];

    return atk;
}

int see(const Board& pos, Move m) {
    uint64_t t0 = now_ns();
    prof.see_calls++;

    // -------------------------
    // YOUR ORIGINAL SEE CODE
    // -------------------------

    int from = from_sq(m);
    int to = to_sq(m);
    int flag = flags_of(m);

    Color side = pos.color_at(from);
    if (side == NO_COLOR)
        return 0;

    Piece attacker = pos.piece_at(from);
    Piece victim;

    if (flag == FLAG_ENPASSANT) {
        int capSq = (side == WHITE ? to - 8 : to + 8);
        victim = pos.piece_at(capSq);
    }
    else {
        victim = pos.piece_at(to);
    }

    if (victim == NO_PIECE)
        return 0;

    int gain[32];
    int depth = 0;

    if (victim < PAWN || victim > KING)
        return 0;

    gain[0] = see_value(victim);

    Bitboard occ = pos.occupiedBB;
    occ &= ~(1ULL << from);

    Piece current = attacker;

    while (true) {
        depth++;
        side = Color(side ^ 1);

        Bitboard atk = attackers_to(pos, to, occ, side) & occ;
        if (!atk)
            break;

        int sq = lsb(atk);
        current = pos.piece_at(sq);

        gain[depth] = see_value(current) - gain[depth - 1];

        occ &= ~(1ULL << sq);

        if (gain[depth] < 0)
            break;
    }

    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    int result = gain[0];

    // -------------------------
    // END OF YOUR CODE
    // -------------------------

    prof.see_ns += now_ns() - t0;
    return result;
}

