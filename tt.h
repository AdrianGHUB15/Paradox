#pragma once

#include "move.h"

#include <cstddef>

enum TTFlag {
    TT_NONE = 0,
    TT_UPPER = 1,
    TT_LOWER = 2,
    TT_EXACT = TT_UPPER | TT_LOWER
};

struct TTEntry {
    uint64_t key;
    int score;
    int depth;
    Move bestMove;
    TTFlag flag;
};

void tt_init(std::size_t megabytes);
void tt_clear();
void tt_store(uint64_t key, int depth, int ply, int score, TTFlag flag, Move bestMove);

bool tt_probe(uint64_t key, int depth, int ply, int alpha, int beta, int& score, Move& bestMove);
bool tt_get_entry(uint64_t key, int ply, TTEntry& out);

Move tt_get_move(uint64_t key);

uint16_t tt_hashfull();