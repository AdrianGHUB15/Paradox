#include "zobrist.h"
#include "bitboard.h"
#include "board.h"
#include <random>

uint64_t ZP[2][6][64];
uint64_t Z_CASTLE[16];
uint64_t Z_EP[8];
uint64_t Z_STM;

static uint64_t rnd() {
    static std::mt19937_64 gen(0x9E3779B97F4A7C15ULL);
    return gen();
}

void zobrist_init() {
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++)
            for (int sq = 0; sq < 64; sq++)
                ZP[c][p][sq] = rnd();

    for (int i = 0; i < 16; i++)
        Z_CASTLE[i] = rnd();

    for (int i = 0; i < 8; i++)
        Z_EP[i] = rnd();

    Z_STM = rnd();
}

uint64_t compute_hash(const Board& pos) {
    uint64_t h = 0;

    // pieces
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 6; p++) {
            Bitboard bb = pos.pieceBB[c][p];
            while (bb) {
                int sq = pop_lsb_index(bb);
                h ^= ZP[c][p][sq];
            }
        }

    // castling
    h ^= Z_CASTLE[pos.castling];

    // en passant
    if (pos.epSquare != -1)
        h ^= Z_EP[pos.epSquare & 7];

    // side to move
    if (pos.stm == BLACK)
        h ^= Z_STM;

    return h;
}
