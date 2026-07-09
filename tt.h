#pragma once
#include <cstdint>
#include <cstddef>

enum TTFlag : uint8_t {
    TT_ALPHA = 0,
    TT_BETA = 1,
    TT_EXACT = 2
};

// EXACTLY 24 bytes
struct TTEntry {
    uint64_t key;     // 8
    int32_t  score;   // 4  (12)
    int32_t  depth;   // 4  (16)
    uint16_t move;    // 2  (18)
    uint8_t  flag;    // 1  (19)
    uint8_t  gen;     // 1  (20)
    uint32_t pad;     // 4  (24) <-- ensures alignment
};
static_assert(sizeof(TTEntry) == 24, "TTEntry must be 24 bytes");

// 4 entries per cluster = 96 bytes
struct TTCluster {
    TTEntry entry[4];
};
static_assert(sizeof(TTCluster) == 96, "TTCluster must be 96 bytes");

void tt_init(size_t mb);
void tt_clear();
void tt_new_generation();

bool tt_probe(uint64_t key, TTEntry& out);
void tt_store(uint64_t key, int depth, int score, TTFlag flag, uint16_t move);

int tt_hashfull();
size_t tt_get_size();
