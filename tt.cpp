#include "tt.h"

#include <cstring>
#include <new>

static TTEntry* TT = nullptr;
static size_t TTSize = 0;

static size_t tt_index(uint64_t key) {
    return key & (TTSize - 1);
}

void tt_init(size_t megabytes) {

    delete[] TT;

    TT = nullptr;
    TTSize = 0;

    if (megabytes == 0)
        return;

    size_t bytes = megabytes * 1024ULL * 1024ULL;

    size_t entries = bytes / sizeof(TTEntry);

    // TTSize must be a power of two because tt_index()
    // uses a bit mask.
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

bool tt_probe(uint64_t key,
    int depth,
    int alpha,
    int beta,
    int& score,
    Move& bestMove) {

    bestMove = 0;

    if (!TT || TTSize == 0)
        return false;

    TTEntry& entry = TT[tt_index(key)];

    // No matching position.
    if (entry.key != key)
        return false;

    // Always return the stored move, even if
    // the entry isn't deep enough for a cutoff.
    bestMove = entry.bestMove;

    // Not deep enough to use the score.
    if (entry.depth < depth)
        return false;

    switch (entry.flag) {

    case TT_EXACT:
        score = entry.score;
        return true;

    case TT_ALPHA:
        if (entry.score <= alpha) {
            score = entry.score;
            return true;
        }
        break;

    case TT_BETA:
        if (entry.score >= beta) {
            score = entry.score;
            return true;
        }
        break;
    }

    return false;
}

void tt_store(uint64_t key,
    int depth,
    int score,
    TTFlag flag,
    Move bestMove) {

    if (!TT || TTSize == 0)
        return;

    TTEntry& entry = TT[tt_index(key)];

    // Don't replace a deeper entry with a shallower one
    // for the same position.
    if (entry.key == key && entry.depth > depth)
        return;

    entry.key = key;
    entry.depth = depth;
    entry.score = score;
    entry.bestMove = bestMove;
    entry.flag = flag;
}
uint64_t tt_hashfull() {

    if (!TT || TTSize == 0)
        return 0;

    // UCI hashfull is conventionally reported in permille:
    // Sampling 1000 entries keeps this essentially free.
    size_t sample = TTSize < 1000 ? TTSize : 1000;

    size_t used = 0;

    for (size_t i = 0; i < sample; ++i) {
        if (TT[i].key != 0)
            ++used;
    }

    return (used * 1000ULL) / sample;
}
