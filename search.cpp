#include <chrono>
#include <iostream>
#include <algorithm>
#include <cstdio>

#include "board.h"
#include "eval.h"
#include "movegen.h"
#include "search.h"
#include "see.h"
#include "tt.h"

uint64_t nodes = 0;
std::chrono::steady_clock::time_point startTime;
int TIME_LIMIT_MS = 0;

static int history[64][64];
static Move killers[128][2];
uint64_t NODE_LIMIT = 0;

bool stopRequested = false;
bool infiniteSearch = false;
bool interrupted = false;
bool showCurrMove = true;

Move finalPV[128];
int finalPV_len = 0;
int finalScore = 0;

Move currentPV[128];
int currentPV_len = 0;
int currentScore = 0;

constexpr int MATE = 32000;

static constexpr int PieceValue[6] = {
    100,   // pawn
    320,   // knight
    330,   // bishop
    500,   // rook
    900,   // queen
    20000  // king
};

void update_history(Move m, int depth) {
    int from = from_sq(m);
    int to = to_sq(m);

    history[from][to] += depth * depth;
}

int move_score(Board& pos, Move m, int ply) {

    if (is_capture(m)) {

        Piece victim = pos.piece_at(to_sq(m));
        Piece attacker = pos.piece_at(from_sq(m));

        if (victim != NO_PIECE)
            return 30000000
            + PieceValue[victim] * 100
            - PieceValue[attacker];
    }
    if (m == killers[ply][0])
        return 20000000;

    if (m == killers[ply][1])
        return 19000000;

    return history[from_sq(m)][to_sq(m)];
}

bool time_up() {
    if (stopRequested)
        return true;

    if (NODE_LIMIT > 0 && nodes >= NODE_LIMIT)
        return true;

    if (infiniteSearch)
        return false;

    if (TIME_LIMIT_MS <= 0)
        return false;

    auto now = std::chrono::steady_clock::now();

    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        now - startTime
    ).count();

    return ms >= TIME_LIMIT_MS;
}

void print_info(int depth, int score, int ms,
    uint64_t nodes, uint64_t nps, uint64_t hashfull,
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

    printf("info depth %d", depth);

    if (is_mate(score)) {
        printf(" score mate %d", score_to_mate(score));
    }
    else if (is_mated(score)) {
        printf(" score mate -%d", score_to_mate(score));
    }
    else {
        printf(" score cp %d", score);
    }

    printf(" time %d nodes %llu nps %llu hashfull %llu pv",
        ms,
        (unsigned long long)nodes,
        (unsigned long long)nps,
        (unsigned long long)hashfull);

    for (int i = 0; i < pv_len; i++) {
        std::string s = move_to_string(pv[i]);
        printf(" %s", s.c_str());
    }

    printf("\n");
    fflush(stdout);
}

int qsearch(Board& pos, int alpha, int beta) {
    nodes++;
    if (time_up()) {
        interrupted = true;
        return 0;
    }
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

        // SEE pruning
        if (see(pos, m) < -50)
            continue;

        pos.make_move(m, st);

        int score = -qsearch(pos, -beta, -alpha);

        pos.unmake_move(st);

        if (score >= beta)
            return beta;

        if (score > alpha)
            alpha = score;
    }

    return alpha;
}
int negamax(Board& pos, int depth, int ply, int alpha, int beta, Move pv[], int& pv_len) {
    nodes++;

    int originalAlpha = alpha;

    pv_len = 0;

    if (pos.is_repetition() && ply > 0)
        return 0;

    if (time_up()) {
        interrupted = true;
        return 0;
    }

    if (depth == 0)
        return qsearch(pos, alpha, beta);

    Move ttMove = 0;
    int ttScore = 0;

    if (tt_probe(pos.hash, depth, alpha, beta, ttScore, ttMove)) {
        if (ply != 0)
            return ttScore;
    }

    int eval = evaluate(pos);

    int bestScore = -100000000;

    MoveList list;
    generate_legal(pos, list);

    if (list.size == 0) {
        pv_len = 0;
        if (in_check(pos, pos.stm))
            return -MATE + ply;
        return 0;
    }
    if (ttMove != 0) {

        for (int i = 0; i < list.size; ++i) {

            if (list.moves[i] == ttMove) {

                std::swap(list.moves[0], list.moves[i]);
                break;
            }
        }
    }
    // Stable, so that tied moves keep generation order rather than whatever
    // the standard library's introsort happens to produce. Keeps node counts
    // identical across compilers/platforms, as OpenBench requires.
    int sortStart = 0;

    if (ttMove != 0 && list.size > 0 && list.moves[0] == ttMove)
        sortStart = 1;

    std::stable_sort(list.moves + sortStart,
        list.moves + list.size,
        [&](Move a, Move b) {
            return move_score(pos, a, ply)
                         > move_score(pos, b, ply);
        });

    Move childPV[128];
    int childPV_len = 0;

    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];
        if (showCurrMove && ply == 0 && depth >= 9) {
            std::cout << "info depth " << depth
                << " currmove " << move_to_string(m)
                << " currmovenumber " << (i + 1)
                << " nodes " << nodes
                << "\n";
            std::cout.flush();
        }
        State st;
        // --- Reverse Futility Pruning (RFP) ---
        if (depth <= 4 && !is_capture(m)) {

            if (eval + 50 * depth <= alpha) {
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
            update_history(m, depth);
        }
        if (alpha >= beta) {

            if (!is_capture(m)) {
                killers[ply][1] = killers[ply][0];
                killers[ply][0] = m;
            }
            break;
        }
    }
    if (!interrupted && bestScore != -100000000) {

        TTFlag flag;

        if (bestScore <= originalAlpha)
            flag = TT_ALPHA;
        else if (bestScore >= beta)
            flag = TT_BETA;
        else
            flag = TT_EXACT;

        if (pv_len > 0)
            bestMove = pv[0];

        tt_store(pos.hash,
            depth,
            bestScore,
            flag,
            bestMove);
    }

    return bestScore;

}

Move search_bestmove(Board& pos, const SearchLimits& limits) {
    stopRequested = false;
    infiniteSearch = limits.infinite;
    showCurrMove = limits.show_currmove;

    finalPV_len = 0;
    finalScore = 0;

    currentPV_len = 0;
    currentScore = 0;

    std::memset(finalPV, 0, sizeof(finalPV));
    std::memset(currentPV, 0, sizeof(currentPV));

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
    std::memset(killers, 0, sizeof(killers));

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
    NODE_LIMIT = limits.nodes;

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

            print_info(depth, score, ms, nodes, nps, tt_hashfull(), pv, pv_len);
            continue;
        }

        if (interrupted && (timeManaged || limits.nodes > 0)) {

            // 1. Print interruption message
            std::cout << "info string search finished before depth "
                << depth << " was completed, falling back to depth "
                << (depth - 1) << "\n";

            // 2. Print depth D only if it has a PV
            if (currentPV_len > 0)
                print_info(depth, currentScore, ms, nodes, nps, tt_hashfull(),
                    currentPV, currentPV_len);

            // 3. Print depth D-1 full info
            print_info(depth - 1, finalScore, ms, nodes, nps, tt_hashfull(),
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
