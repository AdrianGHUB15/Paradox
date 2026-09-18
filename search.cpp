#include <chrono>
#include <iostream>
#include <algorithm>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "search.h"

uint64_t nodes = 0;
std::chrono::steady_clock::time_point startTime;
int TIME_LIMIT_MS = 0;

static int history[64][64];

bool stopRequested = false;
bool infiniteSearch = false;
bool interrupted = false;

int MAX_NODES = 0;
int MAX_DEPTH = 0;

Move finalPV[128];
int finalPV_len = 0;
int finalScore = 0;

Move currentPV[128];
int currentPV_len = 0;
int currentScore = 0;

constexpr int MATE = 32000;

int move_score(Move m) {
    int from = from_sq(m);
    int to = to_sq(m);
    return history[from][to];
}

bool time_up() {
    if (stopRequested)
        return true;

    if (infiniteSearch)
        return false; // only stop ends infinite search

    if (TIME_LIMIT_MS <= 0)
        return false;

    auto now = std::chrono::steady_clock::now();
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return ms >= TIME_LIMIT_MS;
}

void print_info(int depth, int score, int ms, uint64_t nodes, uint64_t nps,
    Move pv[], int pv_len)
{
    auto is_mate = [&](int s) {
        return s >= MATE - 1000;   // delivering mate
        };

    auto is_mated = [&](int s) {
        return s <= -MATE + 1000;  // being mated
        };

    auto score_to_mate = [&](int s) {
        int plies = (s > 0 ? MATE - s : MATE + s);
        return (plies + 1) / 2;
        };


        std::cout << "info depth " << depth;

        if (is_mate(score)) {
            std::cout << " score mate " << score_to_mate(score);
        }
        else if (is_mated(score)) {
            std::cout << " score mate -" << score_to_mate(score);
        }
        else {
            std::cout << " score cp " << score;
        }
        std::cout << " time " << ms
            << " nodes " << nodes
            << " nps " << nps
            << " pv";

        for (int i = 0; i < pv_len; i++)
            std::cout << " " << move_to_string(pv[i]);

        std::cout << "\n";
 
};
int qsearch(Board& pos, int alpha, int beta) {
    nodes++;
    int standPat = evaluate(pos);

    if (standPat >= beta)
        return beta;

    if (standPat > alpha)
        alpha = standPat;

    MoveList list;
    generate_legal(pos, list);

    for (int i = 0; i < list.size; i++) {

        Move m = list.moves[i];

        State st;

        if (!is_capture(m))
            continue;

        pos.make_move(m, st);

        int score = -qsearch(pos, -beta, -alpha);

        pos.unmake_move(st);

        if (score >= beta)
            return beta;

        if (score > alpha)
            alpha = score;
    }

    return standPat;
}
int negamax(Board& pos, int depth, int ply, int alpha, int beta, Move pv[], int& pv_len) {
    nodes++;
    int eval = evaluate(pos);

    int bestScore = -100000000;

    if (time_up()) {
        interrupted = true;
        return bestScore;
    }


    if (depth == 0) {
        pv_len = 0;
        return qsearch(pos, alpha, beta);
    }

    MoveList list;
    generate_legal(pos, list);

    if (list.size == 0) {
        pv_len = 0;
        if (in_check(pos, pos.stm))
            return -MATE + ply;
        return 0;
    }
    // Stable, so that tied moves keep generation order rather than whatever
    // the standard library's introsort happens to produce. Keeps node counts
    // identical across compilers/platforms, as OpenBench requires.
    std::stable_sort(list.moves, list.moves + list.size,
        [&](Move a, Move b) {
            return move_score(a) > move_score(b);
        });

    Move childPV[128];
    int childPV_len = 0;

    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];
        State st;
        // --- Reverse Futility Pruning (RFP) ---
        if (depth <= 4 && !is_capture(m)) {

            if (eval + 150 <= alpha) {
                continue;
            }
        }

        pos.make_move(m, st);
        int score = -negamax(pos, depth - 1, ply + 1, -beta, -alpha, childPV, childPV_len);
        pos.unmake_move(st);

        if (time_up()) {
            interrupted = true;
            break;
        }

        if (score > bestScore) {
            bestScore = score;

            pv[0] = m;
            for (int j = 0; j < childPV_len; j++)
                pv[j + 1] = childPV[j];
            pv_len = childPV_len + 1;
        }

        if (score > alpha) {
            alpha = score;

            int from = from_sq(m);
            int to = to_sq(m);

            history[from][to] += depth * depth;
        }

        if (alpha >= beta)
            break;
    }

    return bestScore;
}

Move search_bestmove(Board& pos, const SearchLimits& limits) {
    stopRequested = false;
    infiniteSearch = limits.infinite;

    bool timeManaged =
        limits.movetime > 0 ||
        limits.wtime > 0 || limits.btime > 0 ||
        limits.winc > 0 || limits.binc > 0;

    int time = (pos.stm == WHITE ? limits.wtime : limits.btime);
    int inc = (pos.stm == WHITE ? limits.winc : limits.binc);

    // TIME LIMIT SETUP
    if (limits.infinite) {
        TIME_LIMIT_MS = 0; // only stop ends search
    }
    else if (limits.movetime > 0) {
        TIME_LIMIT_MS = limits.movetime;
    }
    else if (limits.wtime > 0 || limits.btime > 0) {

        if (limits.movestogoProvided) {
            TIME_LIMIT_MS = time / limits.movestogo + inc / 2;
        }
        else {
            TIME_LIMIT_MS = time / 20 + inc / 2;
        }
        if (TIME_LIMIT_MS > time)
            TIME_LIMIT_MS = time - 50;

        if (TIME_LIMIT_MS < 10)
            TIME_LIMIT_MS = 10;
    }
    else {
        TIME_LIMIT_MS = 0; // no limit → infinite unless stopRequested
        infiniteSearch = true;
    }

    startTime = std::chrono::steady_clock::now();
    std::memset(history, 0, sizeof(history));

    MoveList rootMoves;
    generate_legal(pos, rootMoves);

    if (rootMoves.size == 1 && !limits.movetime) {
        TIME_LIMIT_MS = std::min(TIME_LIMIT_MS, 500);
    }

    Move bestMove = 0;
    Move pv[128];
    int pv_len = 0;

    // Cumulative across the whole iterative deepening run, so that the
    // reported nodes/nps and the elapsed time refer to the same interval.
    nodes = 0;
    uint64_t lastDepthNodes = 0;

    for (int depth = 1; depth <= (limits.depth > 0 ? limits.depth : 99); depth++) {
        interrupted = false;

        // --- TIME MANAGEMENT: stop if we can't afford depth+1 ---
        if (timeManaged && !limits.movetime && depth >= 2 && finalPV_len > 0) {
            auto now = std::chrono::steady_clock::now();
            int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

            if (elapsed <= 0) elapsed = 1;

            int time_left = TIME_LIMIT_MS - elapsed;

            if (time_left <= 0) {
                std::cout << "info string no time left, stopping at depth "
                    << (depth - 1) << "\n";
                    return finalPV[0];
            }

            // Estimate cost of next iteration from previous depth
            uint64_t nodesThisDepth = nodes - lastDepthNodes;

            if (nodesThisDepth == 0) nodesThisDepth = 1;

            uint64_t nps = nodes * 1000ULL / elapsed;

            if (nps == 0) nps = 1;

            // Simple model: next depth ≈ 2x current depth cost
            uint64_t estimatedNextNodes = nodesThisDepth * 2;
            int estimatedNextMs = (int)(estimatedNextNodes * 1000ULL / nps);

            if (time_left < estimatedNextMs) {
                std::cout << "info string not enough time for depth "
                    << (depth + 1) << ", stopping at depth "
                    << depth << "\n";
                return finalPV[0];
            }

            lastDepthNodes = nodes;
        }

        int score = negamax(pos, depth, 0,  -100000000, 100000000, pv, pv_len);


        // compute ms and nps
        auto dend = std::chrono::steady_clock::now();
        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(dend - startTime).count();
        if (ms == 0) ms = 1;
        uint64_t nps = nodes * 1000 / ms;

        // Save current depth PV (even if interrupted)
        currentScore = score;
        currentPV_len = pv_len;
        for (int i = 0; i < pv_len; i++)
            currentPV[i] = pv[i];

        if (!interrupted) {
            finalScore = score;
            finalPV_len = pv_len;
            for (int i = 0; i < pv_len; i++)
                finalPV[i] = pv[i];

            print_info(depth, score, ms, nodes, nps, pv, pv_len);
            continue;
        }

        if (interrupted && timeManaged) {

            // 1. Print interruption message
            std::cout << "info string search finished before depth "
                << depth << " was completed, falling back to depth "
                << (depth - 1) << "\n";

            // 2. Print depth D only if it has a PV
            if (currentPV_len > 0)
                print_info(depth, currentScore, ms, nodes, nps,
                    currentPV, currentPV_len);

            // 3. Print depth D-1 full info
            print_info(depth - 1, finalScore, ms, nodes, nps,
                finalPV, finalPV_len);

            // 4. Return best move from last completed depth
            return finalPV[0];
        }

        if (time_up())
            break;

        if (limits.nodes > 0 && nodes >= limits.nodes)
            break;
    }
    if (limits.bench_mode) {
        auto end = std::chrono::steady_clock::now();
        uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
        if (ms == 0) ms = 1;

        uint64_t nps = (nodes * 1000ULL) / ms;

        std::cout << "info string bench summary: " << nodes << " nodes " << nps << " nps\n";
        // Trailing "<n> nodes <n> nps" is what OpenBench scrapes.
        std::cout << "bench: " << ms << " ms "
            << nodes << " nodes "
            << nps << " nps" << std::endl;
    }

    // After iterative deepening loop ends
    if (finalPV_len > 0)
        return finalPV[0];        // last completed depth

    if (currentPV_len > 0)
        return currentPV[0];      // partial PV from interrupted depth

    return bestMove;              // fallback (should never be 0 now)
}
