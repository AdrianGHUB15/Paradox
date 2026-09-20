#include "eval.h"

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
    {52, 357, 398, 482, 1081, 0},
    // pieceEG
    {109, 373, 404, 725, 1274, 0},

    // pstPawnMG
    {
    -73, -73, -73, -73, -73, -73, -73, -73,
        82, 51, 70, 108, 87, 13, -28, 33,
        6, 27, 40, 65, 60, 36, 44, 28,
        -6, 25, 24, 44, 53, 33, 31, -8,
        -18, 12, 12, 38, 31, 20, 23, -8,
        -15, 10, 12, 16, 22, 18, 46, -2,
        -31, 9, 0, -13, 0, 33, 44, -14,
        -73, -73, -73, -73, -73, -73, -73, -73,
    },

    // pstPawnEG
     {   // Pawn
            -176, -176, -176, -176, -176, -176, -176, -176,
            182, 202, 163, 107, 77, 141, 190, 183,
            104, 99, 73, 68, 57, 48, 85, 79,
            65, 50, 34, 18, 12, 18, 36, 51,
            45, 35, 26, 18, 14, 19, 38, 37,
            31, 37, 22, 24, 17, 22, 22, 26,
            42, 39, 33, 20, 35, 22, 23, 27,
            -176, -176, -176, -176, -176, -176, -176, -176,
        },

     // pstKnightMG
         {   // Knight
            -33, 6, 21, 28, -7, 133, 41, -153,
            -21, -8, -1, 62, 34, 11, 8, 3,
            -8, -3, 21, 39, 9, 10, 14, 10,
            -13, 24, 37, 34, 37, 44, 36, -24,
            -3, 21, 30, 41, 42, 33, 6, 18,
            -36, -3, 12, 19, 14, 14, 3, -18,
            -71, -12, -7, 14, 3, -9, -6, -47,
            -112, -44, -31, -41, -26, -27, -42, -127,
        },

        {   // Knight
            -33, 6, 21, 28, -7, 133, 41, -153,
            -21, -8, -1, 62, 34, 11, 8, 3,
            -8, -3, 21, 39, 9, 10, 14, 10,
            -13, 24, 37, 34, 37, 44, 36, -24,
            -3, 21, 30, 41, 42, 33, 6, 18,
            -36, -3, 12, 19, 14, 14, 3, -18,
            -71, -12, -7, 14, 3, -9, -6, -47,
            -112, -44, -31, -41, -26, -27, -42, -127,
        },

    // pstBishopMG
    {
            -78, -88, -28, -77, -12, -165, -126, -11,
            -34, 19, 19, -43, 9, -13, 35, -34,
            -15, 38, 31, 77, 49, 50, 24, 27,
            -5, 14, 52, 35, 46, 31, 17, 11,
            -11, 14, 12, 35, 34, 18, 21, -18,
            1, 8, 14, 20, 13, 24, 9, 7,
            2, 10, 20, -6, 14, 21, 29, 19,
            -10, -37, -22, -24, -33, -20, -21, 3,
    },

        {   // Bishop
            35, 30, 7, 40, 1, 42, 19, -40,
            14, -17, 2, 18, 8, -2, 16, -20,
            -5, 8, 23, -14, 0, 24, 24, -10,
            1, 23, 4, 19, 25, 22, 12, -14,
            -1, -1, 15, 30, 28, 22, -4, 3,
            -13, 3, 18, 19, 22, 2, -13, -19,
            -30, -11, -10, 11, -5, -8, -21, -40,
            -55, -42, -32, -21, -17, -37, -30, -58,
        },
    // pstRookMG
    { 
            52, 83, 61, 130, 151, 114, 47, 86,
            28, 21, 60, 103, 121, 112, 23, 13,
            -16, 23, 25, 56, 81, 64, 53, 16,
            -34, -45, -16, 0, 25, 3, -2, -27,
            -51, -61, -44, -27, -28, -33, -24, -47,
            -79, -44, -49, -53, -57, -55, -25, -66,
            -73, -35, -26, -31, -32, -27, -38, -94,
            -53, -43, -36, -16, -12, -33, -59, -61,
    },

    // pstRookEG
          {   // Rook
            10, 4, 12, -16, -21, -14, 5, -5,
            18, 22, 20, 1, -10, -13, 11, 18,
            30, 15, 22, 9, -5, -3, 1, 5,
            21, 34, 27, 25, 13, 14, 11, 21,
            -1, 20, 17, 18, 6, 18, 0, -9,
            -7, -14, -8, -6, 6, 7, -27, -20,
            -26, -36, -22, -16, -10, -21, -24, -26,
            -12, -14, 5, -10, -19, -17, -1, -32,
        },

    // pstQueenMG
    {
            -80, -47, 52, 71, 28, 63, 41, 1,
            -32, -21, -11, 6, 7, 16, 14, 4,
            -29, 8, -8, 13, 55, 64, 25, 22,
            -11, -17, -8, 2, 12, 14, -9, 8,
            -2, -29, -5, -1, 0, 3, -9, -16,
            -22, 4, 6, -8, -4, -4, 10, -9,
            -30, -2, 11, 1, 4, 22, 6, -42,
            -26, -16, -7, 1, -12, -24, -6, -45,
    },

    // pstQueenEG
       {   // Queen
            85, 74, -22, -32, 56, 30, 8, 2,
            -7, 26, 75, 59, 84, 77, 38, 47,
            23, 1, 47, 74, 67, 55, 62, 9,
            -5, -1, 64, 76, 81, 81, 76, 29,
            -76, 36, 10, 73, 50, 42, 57, 1,
            -56, -59, -45, 14, 7, 26, -36, -7,
            -53, -80, -102, -39, -45, -93, -104, -107,
            -47, -201, -103, -114, -69, -66, -97, -55,
       },

    // pstKingMG
    {   // King
            100, 159, 152, 41, 273, 83, 28, -274,
            210, 33, 184, 286, 279, 96, -89, 19,
            15, 11, -33, 24, 23, 1, 51, -18,
            -114, -82, -166, -61, -49, -18, -55, -75,
            -123, -81, -128, -131, -129, -95, -98, -106,
            -3, -35, -109, -112, -98, -67, -38, -24,
            32, 9, 8, -44, -37, 17, 39, 49,
            54, 48, 30, -25, 24, 0, 73, 66,
    },

         {   // King
            -263, -94, -80, -46, -66, -18, 6, 9,
            -51, 2, -32, -48, -13, 16, 62, 13,
            1, 49, 59, 35, 43, 71, 39, 25,
            26, 54, 73, 49, 52, 44, 60, 30,
            20, 34, 55, 57, 52, 45, 46, 15,
            -28, 20, 39, 48, 44, 31, 18, -2,
            -36, -18, -6, 15, 21, -5, -20, -42,
            -122, -59, -39, -39, -54, -48, -59, -88
         }

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