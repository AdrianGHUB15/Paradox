#pragma once
#include "types.h"
#include "move.h"
#include <vector>
#include <string>
#include <iostream>

struct SearchLimits {
    int depth = 0;
    int movetime = 0;
    size_t nodes = 0;
    bool infinite = false;

    int wtime = 0, btime = 0;
    int winc = 0, binc = 0;
    int movestogo = 30;

    int multipv = 1;
};

class Board;

Move search_bestmove(Board& pos, const SearchLimits& limits);

extern bool stopRequested;
extern bool infiniteSearch;
extern int MAX_NODES;
extern int MAX_DEPTH;
extern bool Option_ProfilePrint;
extern int  Option_HashMB;
