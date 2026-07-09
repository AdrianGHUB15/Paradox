#include "movegen.h"
#include "bitboard.h"
#include "move.h"
#include "board.h"

// Helpers
static inline int file_of(int sq) { return sq & 7; }
static inline int rank_of(int sq) { return sq >> 3; }
static inline bool on_board(int sq) { return sq >= 0 && sq < 64; }

// Directions for ray-walks
static const int DIRS_ORTHO[4] = { 8, -8, 1, -1 };
static const int DIRS_DIAG[4] = { 9, 7, -7, -9 };
static const int DIRS_ALL[8] = { 8, -8, 1, -1, 9, 7, -7, -9 };

// --------------------------------------------------------
// Capture-only generation
// --------------------------------------------------------
void generate_captures(const Board& pos, MoveList& list) {
    list.size = 0;

    Color us = pos.stm;
    Color them = Color(us ^ 1);

    Bitboard usBB = pos.colorBB[us];
    Bitboard themBB = pos.colorBB[them];
    Bitboard occ = pos.occupiedBB;

    // Pawns
    Bitboard pawns = pos.pieceBB[us][PAWN];
    int      push = (us == WHITE ? 8 : -8);
    int      promoRank = (us == WHITE ? 6 : 1);

    while (pawns) {
        int from = pop_lsb(pawns);
        int r = rank_of(from);
        int f = file_of(from);

        int capL = from + push - 1;
        int capR = from + push + 1;

        // Left capture
        if (f != 0 && on_board(capL)) {
            Bitboard bb = 1ULL << capL;
            if (themBB & bb) {
                if (r == promoRank) {
                    list.moves[list.size++] = make_move(from, capL, PROMO_Q, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capL, PROMO_R, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capL, PROMO_B, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capL, PROMO_N, FLAG_CAPTURE);
                }
                else {
                    list.moves[list.size++] = make_move(from, capL, PROMO_NONE, FLAG_CAPTURE);
                }
            }
            if (capL == pos.epSquare)
                list.moves[list.size++] = make_move(from, capL, PROMO_NONE, FLAG_ENPASSANT);
        }

        // Right capture
        if (f != 7 && on_board(capR)) {
            Bitboard bb = 1ULL << capR;
            if (themBB & bb) {
                if (r == promoRank) {
                    list.moves[list.size++] = make_move(from, capR, PROMO_Q, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capR, PROMO_R, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capR, PROMO_B, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capR, PROMO_N, FLAG_CAPTURE);
                }
                else {
                    list.moves[list.size++] = make_move(from, capR, PROMO_NONE, FLAG_CAPTURE);
                }
            }
            if (capR == pos.epSquare)
                list.moves[list.size++] = make_move(from, capR, PROMO_NONE, FLAG_ENPASSANT);
        }
    }

    // Knights
    Bitboard knights = pos.pieceBB[us][KNIGHT];
    while (knights) {
        int from = pop_lsb(knights);
        Bitboard moves = KNIGHT_ATTACKS[from] & themBB;
        while (moves) {
            int to = pop_lsb(moves);
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, FLAG_CAPTURE);
        }
    }

    // King
    int ksq = pos.kingSq[us];
    Bitboard km = KING_ATTACKS[ksq] & themBB;
    while (km) {
        int to = pop_lsb(km);
        list.moves[list.size++] = make_move(ksq, to, PROMO_NONE, FLAG_CAPTURE);
    }

    // Bishops
    Bitboard bishops = pos.pieceBB[us][BISHOP];
    while (bishops) {
        int from = pop_lsb(bishops);
        Bitboard moves = bishop_attack(from, occ) & themBB;
        while (moves) {
            int to = pop_lsb(moves);
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, FLAG_CAPTURE);
        }
    }

    // Rooks
    Bitboard rooks = pos.pieceBB[us][ROOK];
    while (rooks) {
        int from = pop_lsb(rooks);
        Bitboard moves = rook_attack(from, occ) & themBB;
        while (moves) {
            int to = pop_lsb(moves);
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, FLAG_CAPTURE);
        }
    }

    // Queens
    Bitboard queens = pos.pieceBB[us][QUEEN];
    while (queens) {
        int from = pop_lsb(queens);
        Bitboard moves = queen_attack(from, occ) & themBB;
        while (moves) {
            int to = pop_lsb(moves);
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, FLAG_CAPTURE);
        }
    }
}

// --------------------------------------------------------
// Pseudo-legal move generation
// --------------------------------------------------------
void generate_pseudo(const Board& pos, MoveList& list) {
    list.size = 0;

    Color us = pos.stm;
    Color them = Color(us ^ 1);

    Bitboard usBB = pos.colorBB[us];
    Bitboard themBB = pos.colorBB[them];
    Bitboard occ = pos.occupiedBB;

    // Pawns
    Bitboard pawns = pos.pieceBB[us][PAWN];
    int      push = (us == WHITE ? 8 : -8);
    int      promoRank = (us == WHITE ? 6 : 1);

    while (pawns) {
        int from = pop_lsb(pawns);
        int r = rank_of(from);
        int f = file_of(from);

        // Single push
        Bitboard single = PAWN_PUSH[us][from];
        if (single && !(occ & single)) {
            int to = from + push;

            if (r == promoRank) {
                list.moves[list.size++] = make_move(from, to, PROMO_Q, FLAG_NONE);
                list.moves[list.size++] = make_move(from, to, PROMO_R, FLAG_NONE);
                list.moves[list.size++] = make_move(from, to, PROMO_B, FLAG_NONE);
                list.moves[list.size++] = make_move(from, to, PROMO_N, FLAG_NONE);
            }
            else {
                list.moves[list.size++] = make_move(from, to, PROMO_NONE, FLAG_NONE);

                // Double push
                Bitboard dbl = PAWN_PUSH2[us][from];
                if (dbl && !(occ & dbl))
                    list.moves[list.size++] = make_move(from, to + push, PROMO_NONE, FLAG_DBL_PUSH);
            }
        }

        // Captures
        int capL = from + push - 1;
        int capR = from + push + 1;

        // Left capture
        if (f != 0 && on_board(capL)) {
            Bitboard bb = 1ULL << capL;
            if (themBB & bb) {
                if (r == promoRank) {
                    list.moves[list.size++] = make_move(from, capL, PROMO_Q, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capL, PROMO_R, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capL, PROMO_B, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capL, PROMO_N, FLAG_CAPTURE);
                }
                else {
                    list.moves[list.size++] = make_move(from, capL, PROMO_NONE, FLAG_CAPTURE);
                }
            }
            if (capL == pos.epSquare)
                list.moves[list.size++] = make_move(from, capL, PROMO_NONE, FLAG_ENPASSANT);
        }

        // Right capture
        if (f != 7 && on_board(capR)) {
            Bitboard bb = 1ULL << capR;
            if (themBB & bb) {
                if (r == promoRank) {
                    list.moves[list.size++] = make_move(from, capR, PROMO_Q, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capR, PROMO_R, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capR, PROMO_B, FLAG_CAPTURE);
                    list.moves[list.size++] = make_move(from, capR, PROMO_N, FLAG_CAPTURE);
                }
                else {
                    list.moves[list.size++] = make_move(from, capR, PROMO_NONE, FLAG_CAPTURE);
                }
            }
            if (capR == pos.epSquare)
                list.moves[list.size++] = make_move(from, capR, PROMO_NONE, FLAG_ENPASSANT);
        }
    }

    // Knights
    Bitboard knights = pos.pieceBB[us][KNIGHT];
    while (knights) {
        int from = pop_lsb(knights);
        Bitboard moves = KNIGHT_ATTACKS[from] & ~usBB;
        while (moves) {
            int to = pop_lsb(moves);
            int flags = (themBB & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, flags);
        }
    }

    // King
    int ksq = pos.kingSq[us];
    Bitboard km = KING_ATTACKS[ksq] & ~usBB;
    while (km) {
        int to = pop_lsb(km);
        int flags = (themBB & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
        list.moves[list.size++] = make_move(ksq, to, PROMO_NONE, flags);
    }

    // Castling (pseudo-legal: emptiness + rights; legality checked in generate_legal/make_move)
    if (us == WHITE) {
        if ((pos.castling & CASTLE_WK) &&
            !(occ & ((1ULL << 5) | (1ULL << 6))))
        {
            list.moves[list.size++] = make_move(4, 6, PROMO_NONE, FLAG_CASTLING);
        }

        if ((pos.castling & CASTLE_WQ) &&
            !(occ & ((1ULL << 1) | (1ULL << 2) | (1ULL << 3))))
        {
            list.moves[list.size++] = make_move(4, 2, PROMO_NONE, FLAG_CASTLING);
        }
    }
    else {
        if ((pos.castling & CASTLE_BK) &&
            !(occ & ((1ULL << 61) | (1ULL << 62))))
        {
            list.moves[list.size++] = make_move(60, 62, PROMO_NONE, FLAG_CASTLING);
        }

        if ((pos.castling & CASTLE_BQ) &&
            !(occ & ((1ULL << 57) | (1ULL << 58) | (1ULL << 59))))
        {
            list.moves[list.size++] = make_move(60, 58, PROMO_NONE, FLAG_CASTLING);
        }
    }

    // Bishops
    Bitboard bishops = pos.pieceBB[us][BISHOP];
    while (bishops) {
        int from = pop_lsb(bishops);
        Bitboard moves = bishop_attack(from, occ) & ~usBB;
        while (moves) {
            int to = pop_lsb(moves);
            int flags = (themBB & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, flags);
        }
    }

    // Rooks
    Bitboard rooks = pos.pieceBB[us][ROOK];
    while (rooks) {
        int from = pop_lsb(rooks);
        Bitboard moves = rook_attack(from, occ) & ~usBB;
        while (moves) {
            int to = pop_lsb(moves);
            int flags = (themBB & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, flags);
        }
    }

    // Queens
    Bitboard queens = pos.pieceBB[us][QUEEN];
    while (queens) {
        int from = pop_lsb(queens);
        Bitboard moves = queen_attack(from, occ) & ~usBB;
        while (moves) {
            int to = pop_lsb(moves);
            int flags = (themBB & (1ULL << to)) ? FLAG_CAPTURE : FLAG_NONE;
            list.moves[list.size++] = make_move(from, to, PROMO_NONE, flags);
        }
    }
}

// --------------------------------------------------------
// Pins + checkmask
// --------------------------------------------------------
static void compute_pins_and_checkmask(
    const Board& pos,
    Color us,
    Bitboard& pinned,
    Bitboard pinRay[64],
    Bitboard& checkmask,
    bool& inCheck,
    bool& doubleCheck)
{
    Color them = Color(us ^ 1);
    int ksq = pos.kingSq[us];

    Bitboard occ = pos.occupiedBB;
    Bitboard usBB = pos.colorBB[us];
    Bitboard themBB = pos.colorBB[them];

    pinned = 0;
    checkmask = ~0ULL;
    inCheck = false;
    doubleCheck = false;

    Bitboard checkers = 0;
    bool slidingChecker = false;

    // Pawn checks
    Bitboard enemyPawns = pos.pieceBB[them][PAWN];
    Bitboard pawnAtk = PAWN_ATTACKS[them ^ 1][ksq];
    Bitboard pawnChk = pawnAtk & enemyPawns;
    if (pawnChk)
        checkers |= pawnChk;

    // Knight checks
    Bitboard enemyKnights = pos.pieceBB[them][KNIGHT];
    Bitboard knightChk = KNIGHT_ATTACKS[ksq] & enemyKnights;
    if (knightChk)
        checkers |= knightChk;

    // King adjacency
    Bitboard enemyKing = pos.pieceBB[them][KING];
    Bitboard kingChk = KING_ATTACKS[ksq] & enemyKing;
    if (kingChk)
        checkers |= kingChk;

    // Sliding checks + pins
    for (int d = 0; d < 8; ++d) {
        int dir = DIRS_ALL[d];
        int sq = ksq + dir;

        Bitboard rayMask = 0;
        int firstFriend = -1;

        while (on_board(sq)) {
            int fDiff = file_of(sq) - file_of(sq - dir);
            if (fDiff > 1 || fDiff < -1) break;

            Bitboard bb = 1ULL << sq;

            if (usBB & bb) {
                if (firstFriend != -1) break;
                firstFriend = sq;
            }
            else if (themBB & bb) {
                Piece p = pos.piece_at(sq);
                bool diagDir = (d >= 4);
                bool orthoDir = (d < 4);

                bool slider =
                    (diagDir && (p == BISHOP || p == QUEEN)) ||
                    (orthoDir && (p == ROOK || p == QUEEN));

                if (!slider) break;

                if (firstFriend == -1) {
                    checkers |= bb;
                    checkmask &= (rayMask | bb);
                    slidingChecker = true;
                }
                else {
                    pinned |= (1ULL << firstFriend);
                    pinRay[firstFriend] = (rayMask | bb);
                }
                break;
            }

            rayMask |= bb;
            sq += dir;
        }
    }

    if (checkers) {
        inCheck = true;

        Bitboard tmp = checkers;
        int cnt = 0;
        while (tmp) {
            pop_lsb(tmp);
            ++cnt;
        }
        if (cnt > 1)
            doubleCheck = true;

        if (!slidingChecker)
            checkmask &= checkers;
    }
}

// --------------------------------------------------------
// Legal move generation
// --------------------------------------------------------
void generate_legal(Board& pos, MoveList& list) {
    MoveList pseudo;
    generate_pseudo(pos, pseudo);

    list.size = 0;

    Color us = pos.stm;
    Color them = Color(us ^ 1);

    Bitboard pinned = 0;
    Bitboard pinRay[64] = {};
    Bitboard checkmask;
    bool inCheck, doubleCheck;

    compute_pins_and_checkmask(pos, us, pinned, pinRay, checkmask, inCheck, doubleCheck);

    int ksq = pos.kingSq[us];

    for (int i = 0; i < pseudo.size; ++i) {
        Move m = pseudo.moves[i];
        int from = from_sq(m);
        int to = to_sq(m);
        int flags = flags_of(m);

        Bitboard fromBB = 1ULL << from;
        Bitboard toBB = 1ULL << to;

        Piece pc = pos.piece_at(from);

        // Double check: only king moves
        if (doubleCheck && pc != KING)
            continue;

        // King moves
        if (pc == KING) {
            if (flags == FLAG_CASTLING) {
                int mid = (to + ksq) / 2;
                if (pos.square_attacked(ksq, them)) continue;
                if (pos.square_attacked(mid, them)) continue;
                if (pos.square_attacked(to, them))  continue;
            }
            else {
                State st;
                pos.make_move(m, st);
                bool illegal = in_check(pos, us);
                pos.unmake_move(st);
                if (illegal)
                    continue;
            }

            list.moves[list.size++] = m;
            continue;
        }

        // If in check: non-king moves must land in checkmask
        if (inCheck && !(toBB & checkmask))
            continue;

        // Pinned piece: must stay on pin ray
        if (pinned & fromBB) {
            if (!(toBB & pinRay[from]))
                continue;
        }

        // En passant: special case — can expose rook/bishop check
        if (flags == FLAG_ENPASSANT) {
            State st;
            pos.make_move(m, st);
            if (in_check(pos, us)) {
                pos.unmake_move(st);
                continue;
            }
            pos.unmake_move(st);
            list.moves[list.size++] = m;
            continue;
        }

        // All other moves are legal
        list.moves[list.size++] = m;
    }
}
