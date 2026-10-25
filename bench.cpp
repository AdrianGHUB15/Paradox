#include "search.h"
#include "move.h"
#include "board.h"
#include <chrono>
#include <iostream>
#include "tt.h"

extern uint64_t nodes;

Move run_bench(int depth) {
    static const char* bench_fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",

        // Positions with high numbers of changed threats
        "k7/2n1n3/1nbNbn2/2NbRBn1/1nbRQR2/2NBRBN1/3N1N2/7K w - - 0 1",
        "K7/8/8/BNQNQNB1/N5N1/R1Q1q2r/n5n1/bnqnqnbk w - - 0 1",

        // Mate positions
        "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
        "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1",

        // 5-man positions
        "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",     // Kc2 - mate
        "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",      // Na2 - mate
        "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1",    // draw

        // 6-man positions
        "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",   // Re5 - mate
        "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",    // Ka2 - mate
        "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1",  // Nd2 - draw

        // 7-man positions
        "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124" // Draw
    };

    const int num_positions = 17;

    SearchLimits limits;
    limits.depth = (depth > 0 ? depth : 8);
    limits.bench_mode = false;   // disable built‑in bench summary
    limits.show_currmove = false;

    uint64_t totalNodes = 0;
    auto benchStart = std::chrono::steady_clock::now();

    Move lastMove = 0;

    for (int idx = 0; idx < num_positions; idx++) {
        tt_clear();

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

