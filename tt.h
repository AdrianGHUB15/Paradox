#pragma once

#include "move.h"

#include <cstddef>

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_LOWER = 1,
    TT_UPPER = 2
};

struct TTEntry {
    uint64_t key;
    uint16_t score;
    uint8_t depth;
    Move bestMove;
    TTFlag flag;
};

static_assert(sizeof(TTEntry) == 16, "TTEntry should be 16 bytes");

void tt_init(std::size_t megabytes);
void tt_clear();
void tt_store(uint64_t key, int depth, int ply, int score, TTFlag flag, Move bestMove);

bool tt_probe(uint64_t key, int depth, int ply, int alpha, int beta, int& score, Move& bestMove);

Move tt_get_move(uint64_t key);

uint16_t tt_hashfull();