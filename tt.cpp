#include "tt.h"
#include <cstring>

static TTCluster* table = nullptr;
static size_t tt_size = 0;        // number of clusters
static uint8_t current_gen = 1;

void tt_init(size_t mb) {
    size_t bytes = mb * 1024ULL * 1024ULL;
    tt_size = bytes / sizeof(TTCluster);

    if (tt_size == 0)
        tt_size = 1;

    delete[] table;
    table = new TTCluster[tt_size];

    tt_clear();
}

void tt_clear() {
    std::memset(table, 0, tt_size * sizeof(TTCluster));
    current_gen = 1;
}

void tt_new_generation() {
    current_gen++;
    if (current_gen == 0)
        current_gen = 1;
}

bool tt_probe(uint64_t key, TTEntry& out) {
    TTCluster& c = table[key % tt_size];

    for (int i = 0; i < 4; i++) {
        TTEntry& e = c.entry[i];
        if (e.key == key) {
            out = e;
            return true;
        }
    }
    return false;
}

void tt_store(uint64_t key, int depth, int score, TTFlag flag, uint16_t move) {
    TTCluster& c = table[key % tt_size];

    // 1. Replace same key
    for (int i = 0; i < 4; i++) {
        TTEntry& e = c.entry[i];
        if (e.key == key) {
            e.key = key;
            e.depth = depth;
            e.score = score;
            e.flag = flag;
            e.move = move;
            e.gen = current_gen;
            return;
        }
    }

    // 2. Replacement policy
    int best = 0;
    int bestScore = -999999;

    for (int i = 0; i < 4; i++) {
        TTEntry& e = c.entry[i];

        int replaceScore =
            (e.gen == current_gen ? 0 : 1000) +   // prefer old generations
            (depth - e.depth);                    // prefer deeper entries

        if (replaceScore > bestScore) {
            bestScore = replaceScore;
            best = i;
        }
    }

    TTEntry& e = c.entry[best];
    e.key = key;
    e.depth = depth;
    e.score = score;
    e.flag = flag;
    e.move = move;
    e.gen = current_gen;
}

int tt_hashfull() {
    int used = 0;

    for (size_t i = 0; i < tt_size; i++) {
        TTCluster& c = table[i];

        // cluster is "used" if ANY entry has a nonzero key
        if (c.entry[0].key ||
            c.entry[1].key ||
            c.entry[2].key ||
            c.entry[3].key)
        {
            used++;
        }
    }

    return int((used * 1000) / tt_size);
}

size_t tt_get_size() {
    return tt_size;
}
