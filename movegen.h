#pragma once

#include "board.h"

struct MoveList {
    Move moves[256];
    int  scores[256];
    int  size;

    MoveList() : size(0) {}
};

// Pseudo-legal moves (no king safety)
void generate_pseudo(const Board& pos, MoveList& list);

// Fully legal moves (king safety enforced)
void generate_legal(Board& pos, MoveList& list);