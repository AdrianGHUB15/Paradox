#pragma once
#include "move.h"
#include <string>

struct SearchLimits {
    int depth = 0;
    int movetime = 0;
    size_t nodes = 0;
    bool infinite = false;
    bool bench_mode = false;
    bool movestogoProvided = false;

    int wtime = 0, btime = 0;
    int winc = 0, binc = 0;
    int movestogo = 30;
};

class Board;

Move search_bestmove(Board& pos, const SearchLimits& limits);


// Fixed-depth startpos bench; prints a "<nodes> nodes <nps> nps" summary.
// depth <= 0 uses the default bench depth.
Move run_bench(int depth = 0);

extern bool stopRequested;
extern bool infiniteSearch;
extern int MAX_NODES;
extern int MAX_DEPTH;
