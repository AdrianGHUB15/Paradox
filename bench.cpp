#include "search.h"
#include "move.h"
#include "board.h"
#include <chrono>
#include <iostream>
extern uint64_t nodes;

Move run_bench(int depth) {
    static const char* bench_fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10"
    };

    const int num_positions = 6;

    SearchLimits limits;
    limits.depth = (depth > 0 ? depth : 2);
    limits.bench_mode = false;   // disable built‑in bench summary

    uint64_t totalNodes = 0;
    auto benchStart = std::chrono::steady_clock::now();

    Move lastMove = 0;

    for (int idx = 0; idx < num_positions; idx++) {
        const char* fen = bench_fens[idx];

        std::cout << "position " << (idx + 1) << "/" << num_positions
            << " (" << fen << ")\n";

        Board b;
        b.set_fen(fen);

        nodes = 0;   // reset node counter

        // This prints depth, score, nodes, nps, pv automatically
        lastMove = search_bestmove(b, limits);

        std::cout << "bestmove " << move_to_string(lastMove) << "\n\n";

        totalNodes += nodes;
    }

    auto benchEnd = std::chrono::steady_clock::now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStart).count();
    if (ms == 0) ms = 1;

    uint64_t nps = (totalNodes * 1000ULL) / ms;

    std::cout << "bench: " << ms << " ms "
        << totalNodes << " nodes "
        << nps << " nps\n";

    return lastMove;
}

