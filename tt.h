#pragma once

#include <cstdint>
#include <cstddef>

#include "move.h"

enum Bound : uint8_t {
    BOUND_NONE,
    BOUND_LOWER,
    BOUND_UPPER,
    BOUND_EXACT
};

struct TTEntry {
    uint64_t key = 0;
    int score = 0;
    int depth = -1;
    Move bestMove = 0;
    Bound bound = BOUND_NONE;
};

void tt_init(size_t mb);
void tt_clear();

TTEntry* tt_probe(uint64_t key);