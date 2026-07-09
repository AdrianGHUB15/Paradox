#include "eval.h"
#include "board.h"
#include "bitboard.h"

// ============================================================
//  Helper: own pieces mask
// ============================================================
inline Bitboard own_pieces(const Board& pos, Color c) {
    return pos.pieceBB[c][PAWN] |
        pos.pieceBB[c][KNIGHT] |
        pos.pieceBB[c][BISHOP] |
        pos.pieceBB[c][ROOK] |
        pos.pieceBB[c][QUEEN] |
        pos.pieceBB[c][KING];
}

// ============================================================
//  Eval parameters (initializer)
// ============================================================

EvalParams evalParams = {
    // pieceMG
    {100, 320, 330, 500, 900, 0},
    // pieceEG
    {120, 300, 320, 500, 900, 0},

    // pstPawnMG
    {
      0,0,0,0,0,0,0,0,
      10,10,10,10,10,10,10,10,
      5,5,5,15,15,5,5,5,
      2,2,2,10,10,2,2,2,
      0,0,0,5,5,0,0,0,
      0,0,-10,0,0,-10,0,0,
      0,0,0,-20,-20,0,0,0,
      0,0,0,0,0,0,0,0
    },

    // pstPawnEG
    {
      0,0,0,0,0,0,0,0,
      5,5,5,5,5,5,5,5,
      5,10,10,15,15,10,10,5,
      5,10,15,20,20,15,10,5,
      5,10,15,20,20,15,10,5,
      5,10,10,15,15,10,10,5,
      5,5,5,5,5,5,5,5,
      0,0,0,0,0,0,0,0
    },

    // pstKnightMG
    {
      -30,-20,-10,-10,-10,-10,-20,-30,
      -20,-5,0,0,0,0,-5,-20,
      -10,0,10,15,15,10,0,-10,
      -10,5,15,20,20,15,5,-10,
      -10,0,15,20,20,15,0,-10,
      -10,5,10,15,15,10,5,-10,
      -20,-10,0,5,5,0,-10,-20,
      -30,-20,-10,-10,-10,-10,-20,-30
    },

    // pstKnightEG
    {
      -20,-10,-10,-10,-10,-10,-10,-20,
      -10,0,0,0,0,0,0,-10,
      -10,0,5,10,10,5,0,-10,
      -10,5,10,15,15,10,5,-10,
      -10,5,10,15,15,10,5,-10,
      -10,0,5,10,10,5,0,-10,
      -10,0,0,0,0,0,0,-10,
      -20,-10,-10,-10,-10,-10,-10,-20
    },

    // pstBishopMG
    {
      -20,-10,-10,-10,-10,-10,-10,-20,
      -10,5,0,0,0,0,5,-10,
      -10,10,10,10,10,10,10,-10,
      -10,0,10,15,15,10,0,-10,
      -10,5,10,15,15,10,5,-10,
      -10,10,10,10,10,10,10,-10,
      -10,5,0,0,0,0,5,-10,
      -20,-10,-10,-10,-10,-10,-10,-20
    },

    // pstBishopEG
    {
      -10,-5,-5,-5,-5,-5,-5,-10,
      -5,0,0,0,0,0,0,-5,
      -5,0,5,10,10,5,0,-5,
      -5,5,10,15,15,10,5,-5,
      -5,5,10,15,15,10,5,-5,
      -5,0,5,10,10,5,0,-5,
      -5,0,0,0,0,0,0,-5,
      -10,-5,-5,-5,-5,-5,-5,-10
    },

    // pstRookMG
    {
      0,0,5,10,10,5,0,0,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      5,10,10,10,10,10,10,5,
      0,0,0,0,0,0,0,0
    },

    // pstRookEG
    {
      0,0,5,10,10,5,0,0,
      0,0,0,5,5,0,0,0,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      -5,0,0,0,0,0,0,-5,
      0,0,0,5,5,0,0,0,
      0,0,5,10,10,5,0,0
    },

    // pstQueenMG
    {
      -20,-10,-10,-5,-5,-10,-10,-20,
      -10,0,0,0,0,0,0,-10,
      -10,0,5,5,5,5,0,-10,
      -5,0,5,5,5,5,0,-5,
      0,0,5,5,5,5,0,-5,
      -10,5,5,5,5,5,0,-10,
      -10,0,5,0,0,0,0,-10,
      -20,-10,-10,-5,-5,-10,-10,-20
    },

    // pstQueenEG
    {
      -10,-5,-5,-5,-5,-5,-5,-10,
      -5,0,0,0,0,0,0,-5,
      -5,0,5,5,5,5,0,-5,
      -5,5,5,10,10,5,5,-5,
      -5,5,5,10,10,5,5,-5,
      -5,0,5,5,5,5,0,-5,
      -5,0,0,0,0,0,0,-5,
      -10,-5,-5,-5,-5,-5,-5,-10
    },

    // pstKingMG
    {
      20,30,10,0,0,10,30,20,
      20,20,0,0,0,0,20,20,
      -10,-20,-20,-20,-20,-20,-20,-10,
      -20,-30,-30,-40,-40,-30,-30,-20,
      -30,-40,-40,-50,-50,-40,-40,-30,
      -30,-40,-40,-50,-50,-40,-40,-30,
      -30,-40,-40,-50,-50,-40,-40,-30,
      -30,-40,-40,-50,-50,-40,-40,-30
    },

    // pstKingEG
    {
      -30,-20,-10,0,0,-10,-20,-30,
      -20,-10,0,10,10,0,-10,-20,
      -10,0,10,20,20,10,0,-10,
      0,10,20,30,30,20,10,0,
      0,10,20,30,30,20,10,0,
      -10,0,10,20,20,10,0,-10,
      -20,-10,0,10,10,0,-10,-20,
      -30,-20,-10,0,0,-10,-20,-30
    },

    // Mobility
    3,3, 4,4, 2,2, 1,1,

    // Bishop pair
    30,20,

    // Rook activity
    15,
    8,

    // Passed pawns
    {0,10,20,30,40,60,80,0},
    {0,20,40,60,80,120,160,0},

    // King safety
    10,

    // Tempo
    10
};

// ============================================================
//  Game phase
// ============================================================

static inline int mirror_sq(int sq) { return sq ^ 56; }

static int game_phase(const Board& pos) {
    int phase = 0;

    phase += popcount(pos.pieceBB[WHITE][KNIGHT]);
    phase += popcount(pos.pieceBB[WHITE][BISHOP]);
    phase += 2 * popcount(pos.pieceBB[WHITE][ROOK]);
    phase += 4 * popcount(pos.pieceBB[WHITE][QUEEN]);

    phase += popcount(pos.pieceBB[BLACK][KNIGHT]);
    phase += popcount(pos.pieceBB[BLACK][BISHOP]);
    phase += 2 * popcount(pos.pieceBB[BLACK][ROOK]);
    phase += 4 * popcount(pos.pieceBB[BLACK][QUEEN]);

    return (phase > 24 ? 24 : phase);
}

// ============================================================
//  EVALUATION
// ============================================================

int evaluate(const Board& pos) {
    int mg = 0;
    int eg = 0;

    // ---------------- Material + PST ----------------
    for (int c = 0; c < 2; c++) {
        Color col = Color(c);
        int sign = (col == WHITE ? 1 : -1);

        for (int p = PAWN; p <= KING; p++) {
            Bitboard bb = pos.pieceBB[col][p];
            while (bb) {
                int sq = pop_lsb(bb);
                int m = (col == WHITE ? sq : mirror_sq(sq));

                mg += sign * evalParams.pieceMG[p];
                eg += sign * evalParams.pieceEG[p];

                switch (p) {
                case PAWN:
                    mg += sign * evalParams.pstPawnMG[m];
                    eg += sign * evalParams.pstPawnEG[m];
                    break;
                case KNIGHT:
                    mg += sign * evalParams.pstKnightMG[m];
                    eg += sign * evalParams.pstKnightEG[m];
                    break;
                case BISHOP:
                    mg += sign * evalParams.pstBishopMG[m];
                    eg += sign * evalParams.pstBishopEG[m];
                    break;
                case ROOK:
                    mg += sign * evalParams.pstRookMG[m];
                    eg += sign * evalParams.pstRookEG[m];
                    break;
                case QUEEN:
                    mg += sign * evalParams.pstQueenMG[m];
                    eg += sign * evalParams.pstQueenEG[m];
                    break;
                case KING:
                    mg += sign * evalParams.pstKingMG[m];
                    eg += sign * evalParams.pstKingEG[m];
                    break;
                }
            }
        }
    }

    // ---------------- Extra terms per side ----------------
    Bitboard occ = pos.occupiedBB;

    for (int c = 0; c < 2; c++) {
        Color col = Color(c);
        int sign = (col == WHITE ? 1 : -1);
        Bitboard own = own_pieces(pos, col);

        // Mobility
        {
            // Knights
            Bitboard n = pos.pieceBB[col][KNIGHT];
            while (n) {
                int sq = pop_lsb(n);
                int moves = popcount(attacks_knight(sq) & ~own);
                mg += sign * moves * evalParams.knightMobMG;
                eg += sign * moves * evalParams.knightMobEG;
            }

            // Bishops
            Bitboard b = pos.pieceBB[col][BISHOP];
            while (b) {
                int sq = pop_lsb(b);
                int moves = popcount(bishop_attack(sq, occ) & ~own);
                mg += sign * moves * evalParams.bishopMobMG;
                eg += sign * moves * evalParams.bishopMobEG;
            }

            // Rooks
            Bitboard r = pos.pieceBB[col][ROOK];
            while (r) {
                int sq = pop_lsb(r);
                int moves = popcount(rook_attack(sq, occ) & ~own);
                mg += sign * moves * evalParams.rookMobMG;
                eg += sign * moves * evalParams.rookMobEG;
            }

            // Queens
            Bitboard q = pos.pieceBB[col][QUEEN];
            while (q) {
                int sq = pop_lsb(q);
                int moves = popcount(queen_attack(sq, occ) & ~own);
                mg += sign * moves * evalParams.queenMobMG;
                eg += sign * moves * evalParams.queenMobEG;
            }
        }

        // Bishop pair
        if (popcount(pos.pieceBB[col][BISHOP]) >= 2) {
            mg += sign * evalParams.bishopPairMG;
            eg += sign * evalParams.bishopPairEG;
        }

        // Rook on open / semi-open file
        {
            Bitboard rooks = pos.pieceBB[col][ROOK];
            Bitboard pawnsUs = pos.pieceBB[col][PAWN];
            Bitboard pawnsThem = pos.pieceBB[!col][PAWN];

            while (rooks) {
                int sq = pop_lsb(rooks);
                int file = sq & 7;

                Bitboard mask = file_mask[file];

                bool usPawn = (pawnsUs & mask) != 0;
                bool themPawn = (pawnsThem & mask) != 0;

                if (!usPawn && !themPawn)
                    mg += sign * evalParams.rookOpenFile;
                else if (!usPawn)
                    mg += sign * evalParams.rookSemiOpenFile;
            }
        }

        // Passed pawns
        {
            Bitboard pawns = pos.pieceBB[col][PAWN];
            Bitboard enemyPawns = pos.pieceBB[!col][PAWN];

            while (pawns) {
                int sq = pop_lsb(pawns);
                int rank = (col == WHITE ? sq / 8 : 7 - (sq / 8));

                Bitboard mask = passed_mask[col][sq];

                if (!(enemyPawns & mask)) {
                    mg += sign * evalParams.passedPawnMG[rank];
                    eg += sign * evalParams.passedPawnEG[rank];
                }
            }
        }

        // King safety: pawn shield
        {
            int kingSq = pos.kingSq[col];
            int file = kingSq & 7;

            int missing = 0;

            for (int df = -1; df <= 1; df++) {
                int f = file + df;
                if (f < 0 || f > 7) continue;

                int sq = (col == WHITE ? 8 + f : 48 + f);

                if (!(pos.pieceBB[col][PAWN] & (1ULL << sq)))
                    missing++;
            }

            mg -= sign * missing * evalParams.pawnShieldPenalty;
        }
    }

    // Tempo
    mg += (pos.stm == WHITE ? evalParams.tempoBonus : -evalParams.tempoBonus);

    // ---------------- Tapered eval ----------------
    int phase = game_phase(pos);
    int score = (mg * phase + eg * (24 - phase)) / 24;

    return (pos.stm == WHITE ? score : -score);
}