#include "eval.h"

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

static inline int mirror_sq(int sq) { return sq ^ 56; }

// Evaluation
int evaluate(const Board& pos) {
    int mg = 0;
    int eg = 0;
    int phase = 0;

    // ---------------- Material + PST ----------------
    for (int c = 0; c < 2; c++) {
        Color col = Color(c);
        int sign = (col == WHITE ? 1 : -1);

        for (int p = PAWN; p <= KING; p++) {
            Bitboard bb = pos.pieceBB[col][p];

            int phaseWeight = (p == KNIGHT || p == BISHOP) ? 1 :
                              (p == ROOK) ? 2 :
                              (p == QUEEN) ? 4 : 0;

            phase += phaseWeight * popcount(bb);

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

        Bitboard own = pos.colorBB[col];

        Bitboard ourPawn = pos.pieceBB[col][PAWN];
        Bitboard theirPawn = pos.pieceBB[!col][PAWN];

        Bitboard knight = pos.pieceBB[col][KNIGHT];
        Bitboard bishop = pos.pieceBB[col][BISHOP];
        Bitboard rook = pos.pieceBB[col][ROOK];
        Bitboard queen = pos.pieceBB[col][QUEEN];

        // Mobility
        {
            // Knights
            Bitboard knightBB = knight;

            while (knightBB) {
                int sq = pop_lsb(knightBB);
                int moves = popcount(attacks_knight(sq) & ~own);
                mg += sign * moves * evalParams.knightMobMG;
                eg += sign * moves * evalParams.knightMobEG;
            }

            // Bishops
            Bitboard bishopBB = bishop;

            while (bishopBB) {
                int sq = pop_lsb(bishopBB);
                int moves = popcount(bishop_attack(sq, occ) & ~own);
                mg += sign * moves * evalParams.bishopMobMG;
                eg += sign * moves * evalParams.bishopMobEG;
            }

            // Rooks
            Bitboard rookBB = rook;

            while (rookBB) {
                int sq = pop_lsb(rookBB);
                int moves = popcount(rook_attack(sq, occ) & ~own);
                mg += sign * moves * evalParams.rookMobMG;
                eg += sign * moves * evalParams.rookMobEG;
            }

            // Queens
            Bitboard queenBB = queen;

            while (queenBB) {
                int sq = pop_lsb(queenBB);
                int moves = popcount(queen_attack(sq, occ) & ~own);
                mg += sign * moves * evalParams.queenMobMG;
                eg += sign * moves * evalParams.queenMobEG;
            }
        }

        // Knight outposts
        {
            Bitboard outpostKnights = knight;

            while (outpostKnights) {
                int sq = pop_lsb(outpostKnights);
                int file = sq & 7;
                int rank = sq / 8;

                // Only reward knights in enemy territory.
                if ((col == WHITE && rank < 4) ||
                    (col == BLACK && rank > 3))
                    continue;

                bool protectedByPawn = false;
                bool attackedByPawn = false;

                if (col == WHITE) {
                    if (file > 0 && sq >= 9)
                        protectedByPawn |= ourPawn & (1ULL << (sq - 9));
                    if (file < 7 && sq >= 7)
                        protectedByPawn |= ourPawn & (1ULL << (sq - 7));

                    if (file > 0 && sq <= 55)
                        attackedByPawn |= theirPawn & (1ULL << (sq + 7));
                    if (file < 7 && sq <= 54)
                        attackedByPawn |= theirPawn & (1ULL << (sq + 9));
                }
                else {
                    if (file > 0 && sq <= 54)
                        protectedByPawn |= ourPawn & (1ULL << (sq + 7));
                    if (file < 7 && sq <= 55)
                        protectedByPawn |= ourPawn & (1ULL << (sq + 9));

                    if (file > 0 && sq >= 9)
                        attackedByPawn |= theirPawn & (1ULL << (sq - 9));
                    if (file < 7 && sq >= 7)
                        attackedByPawn |= theirPawn & (1ULL << (sq - 7));
                }

                if (protectedByPawn && !attackedByPawn) {
                    mg += sign * 15;
                    eg += sign * 10;
                }
            }
        }

        // Bishop pair
        if (popcount(bishop) >= 2) {
            mg += sign * evalParams.bishopPairMG;
            eg += sign * evalParams.bishopPairEG;
        }

        // Rook on 7th rank
        {
            int seventhRank = (col == WHITE) ? 6 : 1;

            int count = popcount(rook & rank_mask[seventhRank]);

            if (count) {
                int bonus = count * 20;
                mg += sign * bonus;
                eg += sign * bonus;
            }
        }

        // Rook on open / semi-open file
        {
            Bitboard rookBB = rook;

            while (rookBB) {
                int sq = pop_lsb(rookBB);
                int file = sq & 7;

                Bitboard mask = file_mask[file];

                bool usPawn = (ourPawn & mask) != 0;
                bool themPawn = (theirPawn & mask) != 0;

                if (!usPawn && !themPawn)
                    mg += sign * evalParams.rookOpenFile;

                else if (!usPawn)
                    mg += sign * evalParams.rookSemiOpenFile;
            }
        }

        // Doubled and isolated pawns
        {
            for (int file = 0; file < 8; ++file) {
                int count = popcount(ourPawn & file_mask[file]);

                // Doubled pawns
                if (count > 1) {
                    int penalty = (count - 1) * 12;
                    mg -= sign * penalty;
                    eg -= sign * penalty;
                }

                // Isolated pawns
                if (count) {
                    Bitboard adjacentFiles = 0;

                    if (file > 0)
                        adjacentFiles |= file_mask[file - 1];

                    if (file < 7)
                        adjacentFiles |= file_mask[file + 1];

                    if (!(ourPawn & adjacentFiles)) {
                        int penalty = count * 10;
                        mg -= sign * penalty;
                        eg -= sign * penalty;
                    }
                }
            }
        }

        // Passed pawns
        {
            Bitboard bb = ourPawn;

            while (bb) {
                int sq = pop_lsb(bb);
                int rank = (col == WHITE ? sq / 8 : 7 - (sq / 8));

                Bitboard mask = passed_mask[col][sq];

                if (!(theirPawn & mask)) {
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

                if (!(ourPawn & (1ULL << sq)))
                    missing++;
            }

            mg -= sign * missing * evalParams.pawnShieldPenalty;
        }
    }

    // Tempo
    mg += (pos.stm == WHITE ? evalParams.tempoBonus : -evalParams.tempoBonus);

    // ---------------- Tapered eval ----------------
    phase = std::min(phase, 24);
    int score = (mg * phase + eg * (24 - phase)) / 24;

    return (pos.stm == WHITE ? score : -score);
}