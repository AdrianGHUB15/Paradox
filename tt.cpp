#include "tt.h"
#include "search.h"

#include <cstring>
#include <new>

static TTEntry* TT = nullptr;
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

void tt_init(std::size_t megabytes) {
    delete[] TT;

    TT = nullptr;
    TTSize = 0;

    if (megabytes == 0)
        return;

    size_t bytes = megabytes * 1024ULL * 1024ULL;
    size_t entries = bytes / sizeof(TTEntry);

    size_t size = 1;
    while ((size << 1) <= entries)
        size <<= 1;

    TT = new TTEntry[size];
    TTSize = size;

    tt_clear();
}

void tt_clear() {
    if (!TT || TTSize == 0)
        return;

    std::memset(TT, 0, TTSize * sizeof(TTEntry));
}

Move tt_get_move(uint64_t key) {
    if (!TT || TTSize == 0)
        return 0;

    TTEntry& entry = TT[tt_index(key)];

    if (entry.key != key)
        return 0;

    return entry.bestMove;
}

bool tt_probe(uint64_t key, int depth, int ply, int alpha, int beta, int& score, Move& bestMove, int& ttDepth) {

    bestMove = 0;
    ttDepth = 0;

    if (!TT || TTSize == 0)
        return false;

    TTEntry& entry = TT[tt_index(key)];

    if (entry.key != key)
        return false;

    bestMove = entry.bestMove;
    ttDepth = entry.depth;

    // Always expose the TT value, even if the entry is too shallow.
    score = score_from_tt(entry.score, ply);

    if (entry.depth < depth)
        return false;

    switch (entry.flag) {
    case TT_EXACT:
        return true;

    case TT_LOWER:
        if (score <= alpha)
            return true;
        break;

    case TT_UPPER:
        if (score >= beta)
            return true;
        break;
    }

    return false;
}

void tt_store(uint64_t key, int depth, int ply, int score, TTFlag flag, Move bestMove) {

    if (!TT || TTSize == 0)
        return;

    TTEntry& entry = TT[tt_index(key)];

    if (entry.key == key && entry.depth > depth)
        return;

    entry.key = key;
    entry.depth = static_cast<uint8_t>(depth);
    entry.score = static_cast<int16_t>(score_to_tt(score, ply));
    entry.bestMove = bestMove;
    entry.flag = flag;
}

uint16_t tt_hashfull() {
    if (!TT || TTSize == 0)
        return 0;

    size_t sample = TTSize < 1000 ? TTSize : 1000;
    size_t used = 0;

    for (size_t i = 0; i < sample; ++i) {
        if (TT[i].key != 0)
            ++used;
    }

    return static_cast<uint16_t>((used * 1000) / sample);
}

bool tt_is_pv(uint64_t key, int depth) {
    if (!TT || TTSize == 0)
        return false;

    TTEntry& entry = TT[tt_index(key)];

    return entry.key == key
        && entry.depth >= depth
        && entry.flag == TT_EXACT
        && entry.bestMove != 0;
}