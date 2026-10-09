#pragma once

#include <cstdint>
#include <cstddef>

#include "move.h"

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_ALPHA = 1,
    TT_BETA = 2
};

struct TTEntry {
    uint64_t key;
    int score;
    int depth;
    Move bestMove;
    TTFlag flag;
};

void tt_init(size_t megabytes);
void tt_clear();

bool tt_probe(uint64_t key,
    int depth,
    int ply,
    int alpha,
    int beta,
    int& score,
    Move& bestMove);

Move tt_get_move(uint64_t key);

void tt_store(uint64_t key,
    int depth,
    int ply,
    int score,
    TTFlag flag,
    Move bestMove);

uint64_t tt_hashfull();