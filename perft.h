#pragma once

#include "board.h"

// Core perft
std::uint64_t perft(Board& pos, int depth);
std::uint64_t perft_divide(Board& pos, int depth);

// Reference-based debugging
void perft_break(Board& pos, int depth);