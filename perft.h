#pragma once

#include <cstdint>
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "perft_tables.h"

// Core perft
std::uint64_t perft(Board& pos, int depth);
std::uint64_t perft_divide(Board& pos, int depth);

// Reference-based debugging
void perft_break(Board& pos, int depth);