#pragma once

#include "board.h"

// Core perft
std::uint64_t perft(Board& pos, int depth);
std::uint64_t perft_divide(Board& pos, int depth);

// Reference-based debugging
void perft_break(Board& pos, int depth);

// Perft suites
void run_perft_suite_fast();
void run_perft_suite_long();
void run_perft_suite_very_long();