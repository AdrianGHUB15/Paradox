#pragma once

#include "move.h"

#include <cstddef>
#include <cstdint>

enum TTFlag : uint8_t {
    TT_EXACT = 0,
    TT_LOWER = 1,
    TT_UPPER = 2
};

static constexpr uint8_t GENERATION_BITS = 5;
static constexpr uint8_t GENERATION_MASK = (1 << GENERATION_BITS) - 1;
static constexpr uint8_t BOUND_SHIFT = GENERATION_BITS;
static constexpr uint8_t BOUND_MASK = 0b11 << BOUND_SHIFT;
static constexpr uint8_t PV_SHIFT = BOUND_SHIFT + 2;
static constexpr uint8_t PV_MASK = 1 << PV_SHIFT;

static constexpr uint32_t TT_KEY_MASK = 0xFFFFFFFF;

struct TTEntry {
    uint8_t depth8;
    uint8_t genBound8;
    Move move16;
    int16_t value16;
};

struct TTCluster {
    uint32_t keys[3];
    TTEntry entries[3];
    uint16_t padding;
};
static_assert(sizeof(TTEntry) == 6, "TTEntry must be 6 bytes");
static_assert(sizeof(TTCluster) == 32, "TTCluster must be 32 bytes");

void tt_init(std::size_t megabytes);
void tt_clear();
void tt_store(uint64_t key, int depth, int ply, int score, TTFlag flag, Move bestMove);

bool tt_probe(uint64_t key, int depth, int ply, int alpha, int beta, int& score, Move& bestMove);

Move tt_get_move(uint64_t key);

uint16_t tt_hashfull();
