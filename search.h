#pragma once

#include "move.h"

#include <atomic>
#include <string>
#include <mutex>

extern std::mutex outputMutex;
struct SearchLimits {
    int depth = 0;
    int movetime = 0;
    size_t nodes = 0;
    bool infinite = false;
    bool bench_mode = false;
    bool movestogoProvided = false;
    bool show_currmove = true;

    int wtime = 0, btime = 0;
    int winc = 0, binc = 0;
    int movestogo = 30;
    int multipv = 1;
    int threads = 1;
    bool silent = false;
};

class Board;

Move search_bestmove(Board& pos, const SearchLimits& limits);

int qsearch(Board& pos, int alpha, int beta, int ply);
// Fixed-depth startpos bench; prints a "<nodes> nodes <nps> nps" summary.
// depth <= 0 uses the default bench depth.
Move run_bench(int depth = 0);

constexpr int MATE = 32000;

extern std::atomic<bool> stopRequested;
extern std::atomic<bool> infiniteSearch;

extern int MAX_NODES;
extern int MAX_DEPTH;

extern std::atomic<uint64_t> nodes;