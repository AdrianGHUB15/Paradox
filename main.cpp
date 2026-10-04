#include "uci.h"
#include "search.h"
#include "tt.h"

int main(int argc, char** argv) {
    init_bitboards();
    zobrist_init();

    tt_init(16);

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