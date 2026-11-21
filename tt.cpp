#include "tt.h"
#include "search.h"

#include <cstring>
#include <new>

static TTCluster* TT = nullptr;
static size_t TTSize = 0;

static int score_to_tt(int score, int ply) {
    if (score >= MATE - 1000)
        return score + ply;

    if (score <= -MATE + 1000)
        return score - ply;

    return score;
}

static int score_from_tt(int score, int ply) {
    if (score >= MATE - 1000)
        return score - ply;

    if (score <= -MATE + 1000)
        return score + ply;

    return score;
}

static size_t tt_index(uint64_t key) {
    return key & (TTSize - 1);
}

static uint32_t tt_key(uint64_t key) {
    return static_cast<uint32_t>(key >> 32);
}

static TTFlag tt_bound(const TTEntry& entry) {
    return TTFlag((entry.genBound8 & BOUND_MASK) >> BOUND_SHIFT);
}

void tt_init(std::size_t megabytes) {
    delete[] TT;

    TT = nullptr;
    TTSize = 0;

    if (megabytes == 0)
        return;

    size_t bytes = megabytes * 1024ULL * 1024ULL;
    size_t clusters = bytes / sizeof(TTCluster);

    size_t size = 1;
    while ((size << 1) <= clusters)
        size <<= 1;

    TT = new TTCluster[size];
    TTSize = size;

    tt_clear();
}

void tt_clear() {
    if (!TT || TTSize == 0)
        return;

    std::memset(TT, 0, TTSize * sizeof(TTCluster));
}

Move tt_get_move(uint64_t key) {
    if (!TT || TTSize == 0)
        return 0;

    TTCluster& cluster = TT[tt_index(key)];
    uint32_t key32 = tt_key(key);

    for (int i = 0; i < 3; ++i) {
        TTEntry& entry = cluster.entries[i];

        if (cluster.keys[i] == key32 && entry.depth8)
            return entry.move16;
    }

    return 0;
}

bool tt_probe(uint64_t key, int depth, int ply, int alpha, int beta, int& score, Move& bestMove) {

    bestMove = 0;

    if (!TT || TTSize == 0)
        return false;

    TTCluster& cluster = TT[tt_index(key)];
    uint32_t key32 = tt_key(key);

    for (int i = 0; i < 3; ++i) {
        TTEntry& entry = cluster.entries[i];

        if (cluster.keys[i] != key32 || !entry.depth8)
            continue;

        bestMove = entry.move16;

        if (entry.depth8 - 1 < depth)
            return false;

        int ttScore = score_from_tt(entry.value16, ply);

        switch (tt_bound(entry)) {
        case TT_EXACT:
            score = ttScore;
            return true;

        case TT_LOWER:
            if (ttScore >= beta) {
                score = ttScore;
                return true;
            }
            break;

        case TT_UPPER:
            if (ttScore <= alpha) {
                score = ttScore;
                return true;
            }
            break;
        }

        return false;
    }

    return false;
}

void tt_store(uint64_t key, int depth, int ply, int score, TTFlag flag, Move bestMove) {

    if (!TT || TTSize == 0)
        return;

    TTCluster& cluster = TT[tt_index(key)];
    uint32_t key32 = tt_key(key);

    int replace = 0;

    for (int i = 0; i < 3; ++i) {
        TTEntry& entry = cluster.entries[i];

        if (!entry.depth8 || cluster.keys[i] == key32) {
            replace = i;
            break;
        }

        if (entry.depth8 < cluster.entries[replace].depth8)
            replace = i;
    }

    TTEntry& entry = cluster.entries[replace];

    if (entry.depth8 && cluster.keys[replace] == key32
        && entry.depth8 > depth + 1)
        return;

    cluster.keys[replace] = key32;
    entry.depth8 = static_cast<uint8_t>(depth + 1);
    entry.genBound8 = static_cast<uint8_t>(flag << BOUND_SHIFT);
    entry.move16 = bestMove;
    entry.value16 = static_cast<int16_t>(score_to_tt(score, ply));
}

uint16_t tt_hashfull() {
    if (!TT || TTSize == 0)
        return 0;

    size_t sample = TTSize < 333 ? TTSize : 333;
    size_t used = 0;

    for (size_t i = 0; i < sample; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (TT[i].entries[j].depth8)
                ++used;
        }
    }

    return static_cast<uint16_t>((used * 1000) / (sample * 3));
}