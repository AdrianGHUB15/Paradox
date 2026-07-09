#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "board.h"

struct RefEntry {
    std::string moveStr;
    uint64_t nodes;

    RefEntry(const std::string& m, uint64_t n)
        : moveStr(m), nodes(n) {
    }
};

using RefMoveList = std::vector<RefEntry>;

// FEN-aware lookup by FEN string
RefMoveList load_reference_for_depth(const std::string& fen, int depth);

// Convenience overload: Board -> FEN -> reference
std::string board_to_fen(const Board& pos);
inline RefMoveList load_reference_for_depth(const Board& pos, int depth) {
    return load_reference_for_depth(board_to_fen(pos), depth);
}

// Reference FENs
extern const char* FEN_STARTPOS;
extern const char* FEN_1;
extern const char* FEN_2;
extern const char* FEN_3;
extern const char* FEN_4;
extern const char* FEN_5;

// STARTPOS tables
extern const RefMoveList TABLE_STARTPOS_D1;
extern const RefMoveList TABLE_STARTPOS_D2;
extern const RefMoveList TABLE_STARTPOS_D3;
extern const RefMoveList TABLE_STARTPOS_D4;
extern const RefMoveList TABLE_STARTPOS_D5;
extern const RefMoveList TABLE_STARTPOS_D6;

// FEN 1 tables
extern const RefMoveList TABLE_FEN1_D1;
extern const RefMoveList TABLE_FEN1_D2;
extern const RefMoveList TABLE_FEN1_D3;
extern const RefMoveList TABLE_FEN1_D4;
extern const RefMoveList TABLE_FEN1_D5;
extern const RefMoveList TABLE_FEN1_D6;

// FEN 2 tables
extern const RefMoveList TABLE_FEN2_D1;
extern const RefMoveList TABLE_FEN2_D2;
extern const RefMoveList TABLE_FEN2_D3;
extern const RefMoveList TABLE_FEN2_D4;
extern const RefMoveList TABLE_FEN2_D5;
extern const RefMoveList TABLE_FEN2_D6;

// FEN 3 tables
extern const RefMoveList TABLE_FEN3_D1;
extern const RefMoveList TABLE_FEN3_D2;
extern const RefMoveList TABLE_FEN3_D3;
extern const RefMoveList TABLE_FEN3_D4;
extern const RefMoveList TABLE_FEN3_D5;
extern const RefMoveList TABLE_FEN3_D6;

// FEN 4 tables
extern const RefMoveList TABLE_FEN4_D1;
extern const RefMoveList TABLE_FEN4_D2;
extern const RefMoveList TABLE_FEN4_D3;
extern const RefMoveList TABLE_FEN4_D4;
extern const RefMoveList TABLE_FEN4_D5;
extern const RefMoveList TABLE_FEN4_D6;

// FEN 5 tables
extern const RefMoveList TABLE_FEN5_D1;
extern const RefMoveList TABLE_FEN5_D2;
extern const RefMoveList TABLE_FEN5_D3;
extern const RefMoveList TABLE_FEN5_D4;
extern const RefMoveList TABLE_FEN5_D5;
extern const RefMoveList TABLE_FEN5_D6;
