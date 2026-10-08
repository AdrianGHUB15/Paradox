#include "movegen.h"
#include "perft_tables.h"
#include <unordered_map>
#include <algorithm>
#include <chrono>

// ------------------------------------------------------------
// MOVE ORDERING
// ------------------------------------------------------------

static int move_sort_key(const Board& pos, Move m) {
    int from = from_sq(m);
    int to = to_sq(m);

    // Get the moving piece from the board
    Piece pt = pos.piece_at(from);

    int group = 0;

    // Pawn single pushes
    if (pt == PAWN && to == from + 8)
        group = 0;

    // Pawn double pushes
    else if (pt == PAWN && to == from + 16)
        group = 1;

    // Knights
    else if (pt == KNIGHT)
        group = 2;

    // Bishops
    else if (pt == BISHOP)
        group = 3;

    // Queens
    else if (pt == QUEEN)
        group = 4;

    // King
    else if (pt == KING)
        group = 5;

    // Final key: group → from → to
    return (group << 12) | (from << 6) | to;
}

// ------------------------------------------------------------
// FEN GENERATION
// ------------------------------------------------------------

static char piece_to_fen(Piece p, Color c) {
    static const char* white = "PNBRQK";
    static const char* black = "pnbrqk";
    return (c == WHITE ? white[p] : black[p]);
}

std::string board_to_fen(const Board& pos) {
    std::string fen;

    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;
            Piece p = pos.piece_at(sq);

            if (p == NO_PIECE) {
                empty++;
                continue;
            }

            if (empty > 0) {
                fen += char('0' + empty);
                empty = 0;
            }

            fen += piece_to_fen(p, pos.color_at(sq));
        }
        if (empty > 0)
            fen += char('0' + empty);
        if (rank > 0)
            fen += '/';
    }

    fen += (pos.stm == WHITE ? " w " : " b ");

    std::string c = "";
    if (pos.castling & 1) c += 'K';
    if (pos.castling & 2) c += 'Q';
    if (pos.castling & 4) c += 'k';
    if (pos.castling & 8) c += 'q';
    fen += (c.empty() ? "-" : c);
    fen += ' ';

    if (pos.epSquare == -1)
        fen += "- ";
    else {
        int f = pos.epSquare & 7;
        int r = pos.epSquare >> 3;
        fen += char('a' + f);
        fen += char('1' + r);
        fen += ' ';
    }

    fen += std::to_string(pos.halfmoveClock);
    fen += ' ';
    fen += std::to_string(pos.fullmoveNumber);

    return fen;
}
std::uint64_t perft(Board& pos, int depth) {
    if (depth == 0)
        return 1;

    MoveList list;
    generate_legal(pos, list);

    std::uint64_t nodes = 0;

    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        State st;

        pos.make_move(m, st);
        nodes += perft(pos, depth - 1);
        pos.unmake_move(st);
    }

    return nodes;
}

std::uint64_t perft_divide(Board& pos, int depth) {
    MoveList list;
    generate_legal(pos, list);

    std::uint64_t total = 0;

    auto global_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < list.size; ++i) {
        Move m = list.moves[i];
        State st;

        auto start = std::chrono::high_resolution_clock::now();

        pos.make_move(m, st);
        std::uint64_t n = perft(pos, depth - 1);
        pos.unmake_move(st);

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double nps = (ms > 0 ? (n * 1000.0) / ms : 0);

        std::printf("%s: %llu  (%.3f ms, %.2f nps)\n",
            move_to_string(m),
            (unsigned long long)n,
            ms,
            nps);

        total += n;
    }

    auto global_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(global_end - global_start).count();
    double total_nps = (total_ms > 0 ? (total * 1000.0) / total_ms : 0);

    std::printf("Nodes searched: %llu  (%.3f ms, %.2f nps)\n",
        (unsigned long long)total,
        total_ms,
        total_nps);

    return total;
}

// ------------------------------------------------------------
// PERFT BREAKDOWN (COMPARE TO REFERENCE TABLES)
// ------------------------------------------------------------

void perft_break(Board& pos, int depth) {
    RefMoveList ref =
        load_reference_for_depth(board_to_fen(pos), depth);

    if (ref.empty()) {
        std::printf("No reference table for this position/depth.\n");
        return;
    }

    MoveList list;
    generate_legal(pos, list);

    std::vector<Move> ordered;
    ordered.reserve(list.size);
    for (int i = 0; i < list.size; i++)
        ordered.push_back(list.moves[i]);

    std::sort(ordered.begin(), ordered.end(),
        [&](Move a, Move b) {
            return move_sort_key(pos, a) < move_sort_key(pos, b);
        });

    std::unordered_map<std::string, uint64_t> refMap;
    for (auto& r : ref)
        refMap[r.moveStr] = r.nodes;

    std::uint64_t total_engine = 0;
    std::uint64_t total_ref = 0;

    auto global_start = std::chrono::high_resolution_clock::now();

    std::printf("\n=== FULL MOVE COMPARISON ===\n");

    for (Move m : ordered) {
        std::string key = move_to_string(m);

        uint64_t refNodes = refMap.count(key) ? refMap[key] : 0;

        auto start = std::chrono::high_resolution_clock::now();

        State st;
        pos.make_move(m, st);
        uint64_t engNodes = perft(pos, depth - 1);
        pos.unmake_move(st);

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        double nps = (ms > 0 ? (engNodes * 1000.0) / ms : 0);

        total_engine += engNodes;
        total_ref += refNodes;

        if (engNodes == refNodes)
            std::printf("OK: %s  nodes=%llu  (%.3f ms, %.2f nps)\n",
                key.c_str(),
                (unsigned long long)engNodes,
                ms,
                nps);
        else
            std::printf("WRONG NODE COUNT: %s  ref=%llu  eng=%llu  (%.3f ms, %.2f nps)\n",
                key.c_str(),
                (unsigned long long)refNodes,
                (unsigned long long)engNodes,
                ms,
                nps);
    }

    auto global_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(global_end - global_start).count();
    double total_nps = (total_ms > 0 ? (total_engine * 1000.0) / total_ms : 0);

    std::printf("\nReference total: %llu\n", (unsigned long long)total_ref);
    std::printf("Engine total:    %llu  (%.3f ms, %.2f nps)\n\n",
        (unsigned long long)total_engine,
        total_ms,
        total_nps);
}
// ------------------------------------------------------------
// FULL PERFT SUITE
// ------------------------------------------------------------

struct PerftSuiteTest
{
    const char* name;
    const char* fen;
    int maxDepth;
};

struct PerftSuiteConfig
{
    const char* name;
    const PerftSuiteTest* tests;
    int testCount;
};

// ------------------------------------------------------------
// Generic suite runner
// ------------------------------------------------------------

static void run_perft_suite(const PerftSuiteConfig& config)
{
    int passed = 0;
    int failed = 0;

    std::printf("\n");
    std::printf("============================================================\n");
    std::printf("                 PARADOX PERFT SUITE %s\n", config.name);
    std::printf("============================================================\n");

    for (int t = 0; t < config.testCount; ++t)
    {
        const auto& test = config.tests[t];

        std::printf("\n[%s]\n", test.name);
        std::printf("FEN: %s\n", test.fen);

        for (int depth = 1; depth <= test.maxDepth; ++depth)
        {
            RefMoveList ref =
                load_reference_for_depth(test.fen, depth);

            if (ref.empty())
            {
                std::printf(
                    "  D%d: NO REFERENCE\n",
                    depth
                );

                ++failed;
                continue;
            }

            Board pos;
            pos.set_fen(test.fen);

            MoveList list;
            generate_legal(pos, list);

            std::unordered_map<std::string, std::uint64_t> refMap;

            for (const auto& r : ref)
                refMap[r.moveStr] = r.nodes;

            std::uint64_t engineTotal = 0;
            std::uint64_t referenceTotal = 0;

            bool depthPassed = true;

            auto start =
                std::chrono::high_resolution_clock::now();

            // ------------------------------------------------
            // Compare every legal root move
            // ------------------------------------------------

            for (int i = 0; i < list.size; ++i)
            {
                Move m = list.moves[i];
                std::string moveStr = move_to_string(m);

                State st;

                pos.make_move(m, st);

                std::uint64_t nodes =
                    perft(pos, depth - 1);

                pos.unmake_move(st);

                engineTotal += nodes;

                auto it = refMap.find(moveStr);

                if (it == refMap.end())
                {
                    std::printf(
                        "  D%d FAIL: unexpected move %s "
                        "(engine=%llu)\n",
                        depth,
                        moveStr.c_str(),
                        (unsigned long long)nodes
                    );

                    depthPassed = false;
                    continue;
                }

                std::uint64_t expected = it->second;

                referenceTotal += expected;

                if (nodes != expected)
                {
                    std::printf(
                        "  D%d FAIL: %s "
                        "expected=%llu "
                        "actual=%llu\n",
                        depth,
                        moveStr.c_str(),
                        (unsigned long long)expected,
                        (unsigned long long)nodes
                    );

                    depthPassed = false;
                }
            }

            // ------------------------------------------------
            // Detect reference moves missing from engine
            // ------------------------------------------------

            for (const auto& r : ref)
            {
                bool found = false;

                for (int i = 0; i < list.size; ++i)
                {
                    if (move_to_string(list.moves[i]) == r.moveStr)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    std::printf(
                        "  D%d FAIL: missing move %s "
                        "(expected=%llu)\n",
                        depth,
                        r.moveStr.c_str(),
                        (unsigned long long)r.nodes
                    );

                    depthPassed = false;
                    referenceTotal += r.nodes;
                }
            }

            auto end =
                std::chrono::high_resolution_clock::now();

            double ms =
                std::chrono::duration<double, std::milli>(
                    end - start
                ).count();

            double nps =
                ms > 0.0
                ? (engineTotal * 1000.0) / ms
                : 0.0;

            // ------------------------------------------------
            // Final result
            // ------------------------------------------------

            if (depthPassed &&
                engineTotal == referenceTotal)
            {
                std::printf(
                    "  D%d PASS: %llu nodes "
                    "(%.3f ms, %.2f nps)\n",
                    depth,
                    (unsigned long long)engineTotal,
                    ms,
                    nps
                );

                ++passed;
            }
            else
            {
                std::printf(
                    "  D%d FAIL: "
                    "reference=%llu "
                    "engine=%llu "
                    "(%.3f ms, %.2f nps)\n",
                    depth,
                    (unsigned long long)referenceTotal,
                    (unsigned long long)engineTotal,
                    ms,
                    nps
                );

                ++failed;
            }
        }
    }

    // --------------------------------------------------------
    // Final summary
    // --------------------------------------------------------

    std::printf("\n");
    std::printf("============================================================\n");

    if (failed == 0)
        std::printf("PERFT SUITE: PASS\n");
    else
        std::printf("PERFT SUITE: FAIL\n");

    std::printf(
        "Tests: %d  Passed: %d  Failed: %d\n",
        passed + failed,
        passed,
        failed
    );

    std::printf("============================================================\n");
    std::fflush(stdout);
}

// ------------------------------------------------------------
// FAST
// ------------------------------------------------------------

void run_perft_suite_fast()
{
    static const PerftSuiteTest tests[] = {
        { "STARTPOS", FEN_STARTPOS, 5 },
        { "FEN1",     FEN_1,        4 },
        { "FEN2",     FEN_2,        6 },
        { "FEN3",     FEN_3,        5 },
        { "FEN4",     FEN_4,        4 },
        { "FEN5",     FEN_5,        4 }
    };

    run_perft_suite({
        "FAST",
        tests,
        sizeof(tests) / sizeof(tests[0])
        });
}

// ------------------------------------------------------------
// LONG
// ------------------------------------------------------------

void run_perft_suite_long()
{
    static const PerftSuiteTest tests[] = {
        { "STARTPOS", FEN_STARTPOS, 6 },
        { "FEN1",     FEN_1,        5 },
        { "FEN2",     FEN_2,        7 },
        { "FEN3",     FEN_3,        6 },
        { "FEN4",     FEN_4,        5 },
        { "FEN5",     FEN_5,        5 }
    };

    run_perft_suite({
        "LONG",
        tests,
        sizeof(tests) / sizeof(tests[0])
        });
}

// ------------------------------------------------------------
// VERY LONG
// ------------------------------------------------------------

void run_perft_suite_very_long()
{
    static const PerftSuiteTest tests[] = {
        { "STARTPOS", FEN_STARTPOS, 7 },
        { "FEN1",     FEN_1,        6 },
        { "FEN2",     FEN_2,        8 },
        { "FEN3",     FEN_3,        7 },
        { "FEN4",     FEN_4,        6 },
        { "FEN5",     FEN_5,        6 }
    };

    run_perft_suite({
        "VERY LONG",
        tests,
        sizeof(tests) / sizeof(tests[0])
        });
}
