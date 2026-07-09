#pragma once
#include "board.h"
#include "move.h"
#include "types.h"
#include "bitboard.h"
#include <cstring>

struct MoveList {
    Move moves[256];
    int  scores[256];
    int  size;

    MoveList() : size(0) {
        std::memset(moves, 0, sizeof(moves));
        std::memset(scores, 0, sizeof(scores));
    }
};

// Pseudo-legal moves (no king safety)
void generate_pseudo(const Board& pos, MoveList& list);

// Fully legal moves (king safety enforced)
void generate_legal(Board& pos, MoveList& list);
void generate_captures(const Board& pos, MoveList& list);