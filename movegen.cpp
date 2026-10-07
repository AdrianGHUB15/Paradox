#include "movegen.h"
#include <cstdlib>

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

    pinned = 0;
    checkmask = ~0ULL;
    inCheck = false;
    doubleCheck = false;

    for (int i = 0; i < 64; ++i)
        pinRay[i] = 0;

    Bitboard checkers = 0;

    // Non-sliding checkers.
    checkers |= PAWN_ATTACKS[them ^ 1][ksq]
        & pos.pieceBB[them][PAWN];

    checkers |= KNIGHT_ATTACKS[ksq]
        & pos.pieceBB[them][KNIGHT];

    checkers |= KING_ATTACKS[ksq]
        & pos.pieceBB[them][KING];

    static const int dirs[8] = {
         8, -8,
         1, -1,
         9,  7,
        -7, -9
    };

    for (int d = 0; d < 8; ++d) {

        int dir = dirs[d];
        bool diagonal = (d >= 4);

        int sq = ksq;
        int firstFriend = -1;

        Bitboard ray = 0;

        while (true) {

            int next = sq + dir;

            if (next < 0 || next >= 64)
                break;

            int df = file_of(next) - file_of(sq);

            // Vertical.
            if (dir == 8 || dir == -8) {
                if (df != 0)
                    break;
            }
            // Horizontal / diagonal.
            else {
                if (std::abs(df) != 1)
                    break;
            }

            sq = next;

            Bitboard bb = 1ULL << sq;

            if (pos.colorBB[us] & bb) {

                if (firstFriend != -1)
                    break;

                firstFriend = sq;
                ray |= bb;
                continue;
            }

            if (pos.colorBB[them] & bb) {

                Piece p = pos.piece_at(sq);

                bool slider;

                if (diagonal)
                    slider = (p == BISHOP || p == QUEEN);
                else
                    slider = (p == ROOK || p == QUEEN);

                if (!slider)
                    break;

                if (firstFriend == -1) {
                    // Direct check.
                    checkers |= bb;
                }
                else {
                    // One friendly piece shields the king.
                    // Therefore it is pinned.
                    pinned |= 1ULL << firstFriend;

                    // Legal destinations for the pinned piece
                    // are on this line.
                    pinRay[firstFriend] = ray | bb;
                }

                break;
            }

            // Empty square.
            ray |= bb;
        }
    }

    if (!checkers)
        return;

    inCheck = true;

    int count = popcount(checkers);

    if (count >= 2) {
        doubleCheck = true;
        return;
    }

    // Exactly one checker.
    int checkerSq = lsb(checkers);

    // Pawn/knight/king: must capture checker.
    Piece checker = pos.piece_at(checkerSq);

    if (checker == PAWN ||
        checker == KNIGHT ||
        checker == KING)
    {
        checkmask = 1ULL << checkerSq;
        return;
    }

    // Sliding checker.
    //
    // Find the ray from king to checker.
    for (int d = 0; d < 8; ++d) {

        int dir = dirs[d];
        bool diagonal = (d >= 4);

        int sq = ksq;
        Bitboard ray = 0;

        while (true) {

            int next = sq + dir;

            if (next < 0 || next >= 64)
                break;

            int df = file_of(next) - file_of(sq);

            if (dir == 8 || dir == -8) {
                if (df != 0)
                    break;
            }
            else {
                if (std::abs(df) != 1)
                    break;
            }

            sq = next;
            ray |= 1ULL << sq;

            if (sq == checkerSq) {
                checkmask = ray;
                return;
            }

            if (pos.occupiedBB & (1ULL << sq))
                break;
        }
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

                if (pos.square_attacked(ksq, them))
                    continue;

                if (pos.square_attacked(mid, them))
                    continue;

                if (pos.square_attacked(to, them))
                    continue;
            }
            else {
                State st;

                if (!pos.make_move(m, st))
                    continue;

                bool illegal = in_check(pos, us);

                pos.unmake_move(st);

                if (illegal)
                    continue;
            }

            list.moves[list.size++] = m;
            continue;
        }

        // ----------------------------------------
        // EN PASSANT
        //
        // Do this BEFORE checkmask/pin tests.
        // EP changes three squares:
        //
        //   from  -> to
        //   captured pawn disappears
        //
        // Therefore ordinary pin/checkmask geometry
        // is not sufficient.
        // ----------------------------------------
        if (flags == FLAG_ENPASSANT) {
            State st;

            if (!pos.make_move(m, st))
                continue;

            bool illegal = in_check(pos, us);

            pos.unmake_move(st);

            if (illegal)
                continue;

            list.moves[list.size++] = m;
            continue;
        }

        // If in check, non-king moves must resolve the check.
        if (inCheck && !(toBB & checkmask))
            continue;

        // Pinned piece
        if (pinned & fromBB) {
            if (!(toBB & pinRay[from]))
                continue;
        }

        // Normal move
        list.moves[list.size++] = m;
    }
}