#include "uci.h"
#include "bitboard.h"
#include "zobrist.h"
#include "tt.h"
#include "search.h"

#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    init_bitboards();
    zobrist_init();
    tt_init(64); // 64 MB TT

    // "./Paradox bench [depth]" must bench and exit without touching stdin.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "bench") == 0) {
            int depth = (i + 1 < argc ? std::atoi(argv[i + 1]) : 0);
            run_bench(depth);
            return 0;
        }
    }

    uci_loop();
    return 0;
}