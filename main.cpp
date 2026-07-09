#include "uci.h"
#include "bitboard.h"
#include "zobrist.h"
#include "tt.h"

int main() {
    init_bitboards();
    zobrist_init();
    tt_init(64); // 64 MB TT

    uci_loop();
    return 0;
}
