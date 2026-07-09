#pragma once
#include <string>
#include "board.h"
#include "move.h"

std::string move_to_san(Board& pos, Move m);
