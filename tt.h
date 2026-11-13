#pragma once

#include "move.h"

#include <cstddef>
#include <cstdint>

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_LOWER = 1,
    TT_UPPER = 2
};

struct TTEntry {
    uint8_t depth;
    uint8_t genBoundPv;
    Move bestMove;
    int16_t score;
    int16_t eval;
};

struct TTCluster {
    TTEntry entries[3];
    uint64_t keys;
};

static_assert(sizeof(TTEntry) == 8);
static_assert(sizeof(TTCluster) == 32);

void tt_init(std::size_t megabytes);
void tt_clear();
void tt_store(uint64_t key, int depth, int ply, int score, TTFlag flag, Move bestMove);

bool tt_probe(uint64_t key, int depth, int ply, int alpha, int beta, int& score, Move& bestMove);

Move tt_get_move(uint64_t key);

uint16_t tt_hashfull();