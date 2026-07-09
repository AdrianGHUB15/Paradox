#pragma once
#include <cstdint>

class Board;  // forward declaration

extern uint64_t ZP[2][6][64];
extern uint64_t Z_CASTLE[16];
extern uint64_t Z_EP[8];
extern uint64_t Z_STM;

void zobrist_init();
uint64_t compute_hash(const Board& pos);
