#pragma once
#include "board.h"

struct EvalParams {
    // --- Piece values ---
    int pieceMG[6];
    int pieceEG[6];

    // --- PSTs ---
    int pstPawnMG[64];
    int pstPawnEG[64];

    int pstKnightMG[64];
    int pstKnightEG[64];

    int pstBishopMG[64];
    int pstBishopEG[64];

    int pstRookMG[64];
    int pstRookEG[64];

    int pstQueenMG[64];
    int pstQueenEG[64];

    int pstKingMG[64];
    int pstKingEG[64];

    // --- Mobility ---
    int knightMobMG, knightMobEG;
    int bishopMobMG, bishopMobEG;
    int rookMobMG, rookMobEG;
    int queenMobMG, queenMobEG;

    // --- Bishop pair ---
    int bishopPairMG;
    int bishopPairEG;

    // --- Rook activity ---
    int rookOpenFile;
    int rookSemiOpenFile;

    // --- Passed pawns ---
    int passedPawnMG[8];
    int passedPawnEG[8];

    // --- King safety ---
    int pawnShieldPenalty;

    // --- Tempo ---
    int tempoBonus;
};

extern EvalParams evalParams;


extern EvalParams evalParams;

int evaluate(const Board& pos);