#include "tt.h"

#include <algorithm>

static std::vector<TTEntry> TT;
static uint64_t TTMask = 0;

void tt_init(size_t mb)
{
    if (mb < 1)
        mb = 1;

    size_t bytes = mb * 1024ULL * 1024ULL;

    size_t entries = bytes / sizeof(TTEntry);

    if (entries < 1)
        entries = 1;

    // Round DOWN to a power of two so we never allocate
    // substantially more memory than requested.
    size_t size = 1;

    while ((size << 1) <= entries)
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
    if (TT.empty())
        return nullptr;

    return &TT[key & TTMask];
}

int tt_hashfull()
{
    if (TT.empty())
        return 0;

    // Sample 1000 entries spread across the entire table.
    constexpr size_t SAMPLES = 1000;

    size_t samples = std::min(SAMPLES, TT.size());
    size_t used = 0;

    for (size_t i = 0; i < samples; ++i) {

        size_t index;

        if (samples == TT.size())
            index = i;
        else
            index = (i * TT.size()) / samples;

        if (TT[index].bound != BOUND_NONE)
            ++used;
    }

    return static_cast<int>((used * 1000ULL) / samples);
}
