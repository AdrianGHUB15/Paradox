#include "tt.h"

#include <vector>
#include <algorithm>

static std::vector<TTEntry> TT;
static uint64_t TTMask = 0;

void tt_init(size_t mb)
{
    size_t bytes = mb * 1024ULL * 1024ULL;

    size_t entries = bytes / sizeof(TTEntry);

    size_t size = 1;

    while (size < entries)
        size <<= 1;

    TT.clear();
    TT.resize(size);

    TTMask = size - 1;
}

void tt_clear()
{
    std::fill(TT.begin(), TT.end(), TTEntry{});
}

TTEntry* tt_probe(uint64_t key)
{
    return &TT[key & TTMask];
}