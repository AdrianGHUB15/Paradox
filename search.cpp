#include <chrono>
#include <iostream>
#include <algorithm>
#include <cstdio>
#include <cassert>

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
uint64_t nodes_printing = 10000000ULL;

constexpr int MAX_PLY = 128;

int VALUE_INFINITE = MATE + 1;

int seldepth = 0;
int currentSeldepth = 0;
int finalSeldepth = 0;

static constexpr int ASPIRATION_WINDOW = 25;
static constexpr int ASPIRATION_MAX_WINDOW = 1000;

int rootDepth = 0;

enum NodeType {
    NonPV,
    PV,
    Root
};

uint64_t elapsed_ms() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        ).count()
        );
}

uint64_t update_nps() {
    uint64_t ms = elapsed_ms();

    if (ms == 0)
        ms = 1;

    return nodes * 1000ULL / ms;
}

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

bool is_win(int score) {
    return score >= MATE - 1000;
}

bool is_loss(int score) {
    return score <= -MATE + 1000;
}

bool is_decisive(int score) {
    return is_win(score) || is_loss(score);
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

void print_info(int depth, int seldepth, int score, uint64_t ms,
    uint64_t nodes, uint64_t nps, uint64_t hashfull,
    Move pv[], int pv_len, const char* bound = nullptr)
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

    printf("info depth %d seldepth %d", depth, seldepth);

    if (is_mate(score)) {
        printf(" score mate %d", score_to_mate(score));
    }
    else if (is_mated(score)) {
        printf(" score mate -%d", score_to_mate(score));
    }
    else {
        printf(" score cp %d", score);
    }

    if (bound != nullptr && nodes > nodes_printing)
        printf(" %s", bound);

    printf(" time %llu nodes %llu nps %llu hashfull %llu pv",
        static_cast<unsigned long long>(ms),
        static_cast<unsigned long long>(nodes),
        static_cast<unsigned long long>(nps),
        static_cast<unsigned long long>(hashfull));

    for (int i = 0; i < pv_len; i++) {
        std::string s = move_to_string(pv[i]);
        printf(" %s", s.c_str());
    }

    printf("\n");
    fflush(stdout);
}

template<NodeType nodeType>
int qsearch(Board& pos, int alpha, int beta, int ply) {

    static_assert(nodeType != Root);

    constexpr bool PvNode = nodeType == PV;

    assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || alpha == beta - 1);

    seldepth = std::max(seldepth, ply);

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

        if (!is_capture(m))
            continue;

        if (see(pos, m) < -50)
            continue;

        State st;
        pos.make_move(m, st);

        int score = -qsearch<NonPV>(pos, -beta, -alpha, ply + 1);

        pos.unmake_move(st);

        if (interrupted)
            return 0;

        if (score >= beta)
            return beta;

        if (score > alpha)
            alpha = score;
    }

    return alpha;
}
template<NodeType nodeType>
int negamax(Board& pos, int depth, int ply, int alpha, int beta, Move pv[], int& pv_len, bool cutNode) {

    int originalAlpha = alpha;

    constexpr bool PvNode = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;
    const bool allNode = !(PvNode || cutNode);

    seldepth = std::max(seldepth, ply);

    pv_len = 0;

    if (pos.is_repetition() && ply > 0)
        return 0;

    if (time_up()) {
        interrupted = true;
        return 0;
    }

    if (!rootNode) {
        alpha = std::max(alpha, -MATE + ply);
        beta = std::min(beta, MATE - ply - 1);

        if (alpha >= beta)
            return alpha;
    }

    if (depth <= 0)
        return qsearch<PvNode ? PV : NonPV>(pos, alpha, beta, ply);


    depth = std::min(depth, MAX_PLY - 1);

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(0 < depth && depth < MAX_PLY);
    assert(!(PvNode && cutNode));

    Move ttMove = 0;
    int ttScore = 0;

    if (tt_probe(pos.hash, depth, ply, alpha, beta, ttScore, ttMove)) {
        if (ply != 0)
            return ttScore;
    }

    int eval = evaluate(pos);

    // Reverse Futility Pruning (RFP)
    if (depth <= 6 && !rootNode && !in_check(pos, pos.stm) && !is_decisive(beta) && beta <= eval - 100 * depth)
        return eval;
    

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
    int moveCount = 0;

    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];
        Move ParentMove = m;
        int ParentCurrMove = i + 1;

        bool capture = is_capture(m);
        bool promotion = is_promo(m);

        State st;
        pos.make_move(m, st);

        bool isCheck = in_check(pos, pos.stm);
        bool quiet = !capture && !promotion && !isCheck;

        ++moveCount;

        int score;
        int reduction = 0;
        int extension = 0;

        if (isCheck)
            extension++;

        if (depth >= 3 && moveCount >= 4 && quiet) {
            reduction = 1;

            if (depth >= 6 && moveCount >= 8)
                reduction = 2;

            // Never reduce beyond the available child depth.
            reduction = std::min(reduction, depth - 2);
        }

        if (showCurrMove && ply == 0 && nodes > nodes_printing) {
            std::cout << "info depth " << depth
                << " search depth " << depth + extension - reduction
                << " currmove " << move_to_string(ParentMove)
                << " currmovenumber " << ParentCurrMove
                << " nodes " << nodes << '\n';
        }

        if (moveCount == 1 && PvNode) {
            score = -negamax<PV>(pos, depth - 1 + extension, ply + 1, -beta, -alpha, childPV, childPV_len, false);
        }
        else {
            // Later moves: reduced-depth scout search.
            score = -negamax<NonPV>(pos, depth - 1 - reduction + extension, ply + 1, -alpha - 1, -alpha, childPV, childPV_len, !cutNode);

            if (!interrupted && reduction > 0 && score > alpha) {
                score = -negamax<NonPV>(pos, depth - 1 + extension, ply + 1, -alpha - 1, -alpha, childPV, childPV_len, !cutNode);
            }

            if (!interrupted && score > alpha && score < beta && PvNode) {
                score = -negamax<PV>(pos, depth - 1 + extension, ply + 1, -beta, -alpha, childPV, childPV_len, false);
            }
        }

        pos.unmake_move(st);

        if (interrupted)
            return 0;

        if (score > bestScore) {
            bestScore = score;

            pv[0] = m;
            for (int j = 0; j < childPV_len; j++)
                pv[j + 1] = childPV[j];

            pv_len = childPV_len + 1;
        }

        if (score > alpha) {
            alpha = score;

            if (quiet)
                update_history(m, depth);
        }

        if (alpha >= beta) {
            if (quiet) {
                killers[ply][1] = killers[ply][0];
                killers[ply][0] = m;
            }
            break;
        }
    }
    if (!interrupted) {

        TTFlag flag;

        if (bestScore <= originalAlpha)
            flag = TT_ALPHA;
        else if (bestScore >= beta)
            flag = TT_BETA;
        else
            flag = TT_EXACT;

        Move bestMove = 0;

        if (pv_len > 0)
            bestMove = pv[0];

        tt_store(pos.hash, depth, ply, bestScore, flag, bestMove);
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


    for (int depth = 1; depth <= (limits.depth > 0 ? std::min(limits.depth, MAX_PLY) : MAX_PLY); depth++) {
        interrupted = false;
        seldepth = 0;
        rootDepth = depth;

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

        int alpha = -VALUE_INFINITE;
        int beta = VALUE_INFINITE;
        int score;

        if (depth >= 4 && finalPV_len > 0) {
            alpha = finalScore - ASPIRATION_WINDOW;
            beta = finalScore + ASPIRATION_WINDOW;
        }

        while (true) {
            score = negamax<Root>(pos, rootDepth, 0, alpha, beta, pv, pv_len, false);

            if (interrupted)
                break;

            // Calculate current search statistics for this retry.
            uint64_t ms = elapsed_ms();
            uint64_t nps = update_nps();

            if (score <= alpha) {
                if (nodes > nodes_printing)
                    print_info(rootDepth, seldepth, score, ms, nodes, nps, tt_hashfull(), pv, pv_len, "upperbound");
                
                alpha = std::max(-VALUE_INFINITE, alpha - ASPIRATION_MAX_WINDOW);
                beta = (alpha + beta) / 2;
            }
            else if (score >= beta) {
                if (nodes > nodes_printing)
                    print_info(rootDepth, seldepth, score, ms, nodes, nps, tt_hashfull(), pv, pv_len, "lowerbound");
                
                beta = std::min(VALUE_INFINITE, beta + ASPIRATION_MAX_WINDOW);
            }
            else {
                break;
            }
        }

        uint64_t ms = elapsed_ms();
        uint64_t nps = update_nps();

        // Save current depth PV and selective depth
        currentScore = score;
        currentPV_len = pv_len;
        currentSeldepth = seldepth;

        for (int i = 0; i < pv_len; i++)
            currentPV[i] = pv[i];

        if (!interrupted) {
            finalScore = score;
            finalPV_len = pv_len;
            finalSeldepth = seldepth;

            for (int i = 0; i < pv_len; i++)
                finalPV[i] = pv[i];

            print_info(rootDepth, finalSeldepth, score, ms, nodes, nps, tt_hashfull(), pv, pv_len);

            continue;
        }

        if (interrupted && (timeManaged || limits.nodes > 0)) {

            std::cout << "info string search finished before depth "
                << depth << " was completed, falling back to depth "
                << (depth - 1) << "\n";

            if (currentPV_len > 0)
                print_info(depth, currentSeldepth, currentScore, ms, nodes, nps, tt_hashfull(), currentPV, currentPV_len);
            

            if (finalPV_len > 0) 
                print_info(depth - 1, finalSeldepth, finalScore, ms, nodes, nps, tt_hashfull(), finalPV, finalPV_len);

            return finalPV_len > 0 ? finalPV[0] : bestMove;
        }

        if (time_up())
            break;

        if (limits.nodes > 0 && nodes >= limits.nodes)
            break;
    }
    if (limits.bench_mode) {
        uint64_t ms = elapsed_ms();
        uint64_t nps = update_nps();

        std::cout << "info string bench summary: "
            << nodes << " nodes " << nps << " nps\n";

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
