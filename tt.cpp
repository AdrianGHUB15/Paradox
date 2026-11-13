
#include "tt.h"
#include "search.h"

#include <algorithm>
#include <cstring>
#include <new>

static TTCluster* TT = nullptr;
static size_t TTSize = 0;

static constexpr uint64_t KEY_MASK = (1ULL << 21) - 1;
static constexpr uint8_t BOUND_MASK = 0x03;

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

// Use the upper 21 bits of the full Zobrist key as its signature.
static uint64_t tt_signature(uint64_t key) {
    return (key >> 43) & KEY_MASK;
}

static uint64_t get_signature(const TTCluster& cluster, int index) {
    return (cluster.keys >> (index * 21)) & KEY_MASK;
}

static void set_signature(TTCluster& cluster, int index, uint64_t signature) {
    int shift = index * 21;
    uint64_t mask = KEY_MASK << shift;

    cluster.keys = (cluster.keys & ~mask)
        | ((signature & KEY_MASK) << shift);
}

static TTFlag get_bound(const TTEntry& entry) {
    return static_cast<TTFlag>(entry.genBoundPv & BOUND_MASK);
}

static void set_bound(TTEntry& entry, TTFlag flag) {
    entry.genBoundPv =
        (entry.genBoundPv & ~BOUND_MASK) | static_cast<uint8_t>(flag);
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
    uint64_t signature = tt_signature(key);

    for (int i = 0; i < 3; ++i) {
        TTEntry& entry = cluster.entries[i];

        if (entry.depth != 0 && get_signature(cluster, i) == signature)
            return entry.bestMove;
    }

    return 0;
}

bool tt_probe(uint64_t key, int depth, int ply,
    int alpha, int beta, int& score, Move& bestMove) {

    bestMove = 0;

    if (!TT || TTSize == 0)
        return false;

    TTCluster& cluster = TT[tt_index(key)];
    uint64_t signature = tt_signature(key);

    for (int i = 0; i < 3; ++i) {
        TTEntry& entry = cluster.entries[i];

        if (entry.depth == 0 || get_signature(cluster, i) != signature)
            continue;

        bestMove = entry.bestMove;

        if (entry.depth < depth)
            return false;

        int ttScore = score_from_tt(entry.score, ply);

        switch (get_bound(entry)) {
        case TT_EXACT:
            score = ttScore;
            return true;

        case TT_LOWER:
            if (ttScore <= alpha) {
                score = ttScore;
                return true;
            }
            break;

        case TT_UPPER:
            if (ttScore >= beta) {
                score = ttScore;
                return true;
            }
            break;
        }

        return false;
    }

    return false;
}

void tt_store(uint64_t key, int depth, int ply,
    int score, TTFlag flag, Move bestMove) {

    if (!TT || TTSize == 0)
        return;

    TTCluster& cluster = TT[tt_index(key)];
    uint64_t signature = tt_signature(key);

    int replacement = 0;

    // Prefer an existing matching entry.
    for (int i = 0; i < 3; ++i) {
        TTEntry& entry = cluster.entries[i];

        if (entry.depth != 0 && get_signature(cluster, i) == signature) {
            if (entry.depth > depth)
                return;

            replacement = i;
            goto store;
        }
    }

    // Otherwise prefer an empty entry.
    for (int i = 0; i < 3; ++i) {
        if (cluster.entries[i].depth == 0) {
            replacement = i;
            goto store;
        }
    }

    // If full, replace the shallowest entry.
    for (int i = 1; i < 3; ++i) {
        if (cluster.entries[i].depth < cluster.entries[replacement].depth)
            replacement = i;
    }

store:
    TTEntry& entry = cluster.entries[replacement];

    set_signature(cluster, replacement, signature);

    entry.depth = static_cast<uint8_t>(depth);
    entry.genBoundPv = 0;
    set_bound(entry, flag);
    entry.bestMove = bestMove;
    entry.score = static_cast<int16_t>(score_to_tt(score, ply));
    entry.eval = 0;
}

uint16_t tt_hashfull() {
    if (!TT || TTSize == 0)
        return 0;

    size_t sample = std::min(TTSize, size_t(334));
    size_t used = 0;
    size_t total = sample * 3;

    for (size_t i = 0; i < sample; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (TT[i].entries[j].depth != 0)
                ++used;
        }
    }

    return static_cast<uint16_t>((used * 1000) / total);
}