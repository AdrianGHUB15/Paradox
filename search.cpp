#include "eval.h"
#include "movegen.h"
#include "search.h"
#include "see.h"
#include "tt.h"

#include <condition_variable>
#include <functional>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <sstream>

std::mutex outputMutex;
std::atomic<uint64_t> nodes{ 0 };
std::atomic<uint64_t> NODE_LIMIT{ 0 };

uint32_t nodes_printing = 10000000ULL;

thread_local Move finalPV[128];
thread_local Move currentPV[128];

static thread_local Move killers[128][2];

static thread_local int captureHistory[6][64][6];
static thread_local int history[64][64];

static thread_local int pvIdx = 0;
static thread_local int pvLast = 0;
static thread_local int multiPV = 1;

static constexpr int ASPIRATION_WINDOW = 25;

constexpr int MAX_PLY = 128;
constexpr int VALUE_INFINITE = MATE + 1;

thread_local int finalPV_len = 0;
thread_local int finalScore = 0;

thread_local int currentPV_len = 0;
thread_local int currentScore = 0;

thread_local int seldepth = 0;
thread_local int currentSeldepth = 0;
thread_local int finalSeldepth = 0;

thread_local int rootDepth = 0;

std::atomic<int> TIME_LIMIT_MS{ 0 };
std::atomic<int> SOFT_TIME_LIMIT_MS{ 0 };

std::atomic<bool> stopRequested{ false };
std::atomic<bool> infiniteSearch{ false };

thread_local bool interrupted = false;
thread_local bool showCurrMove = true;

thread_local int finalDepth = 0;

struct RootMove {
    Move move = 0;
    Move pv[128]{};

    int score = -VALUE_INFINITE;
    int pv_len = 0;
    int seldepth = 0;
    uint64_t effort = 0;
};

struct WorkerResult {
    Move bestMove = 0;
    int score = -VALUE_INFINITE;
    int depth = 0;
    Move pv[128]{};
    int pv_len = 0;
    int seldepth = 0;
};

enum NodeType {
    NonPV,
    PV,
    Root
};

static thread_local std::vector<RootMove> rootMoves;
static thread_local std::unordered_map<Move, uint64_t> rootMoveEffort;

static thread_local bool silentOutput = false;

std::chrono::steady_clock::time_point startTime;


static void sort_root_moves() {
    std::stable_sort(rootMoves.begin(), rootMoves.end(),
        [](const RootMove& a, const RootMove& b) {
            return a.score 
                 > b.score;
        });
}

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

void update_history(Move m, int bonus) {
    int from = from_sq(m);
    int to = to_sq(m);

    int& h = history[from][to];

    h += bonus - h * std::abs(bonus) / 16384;
}

void update_capture_history(Board& pos, Move m, int depth) {
    Piece attacker = pos.piece_at(from_sq(m));
    Piece victim = is_ep(m) ? PAWN : pos.piece_at(to_sq(m));

    if (attacker == NO_PIECE || victim == NO_PIECE)
        return;

    int bonus = std::min(2048, depth * depth * 16);

    int& h = captureHistory[attacker][to_sq(m)][victim];

    h += bonus - h * bonus / 16384;
}

int move_score(Board& pos, Move m, int ply) {

    if (is_capture(m)) {
        Piece victim = is_ep(m) ? PAWN : pos.piece_at(to_sq(m));
        Piece attacker = pos.piece_at(from_sq(m));

        if (victim != NO_PIECE && attacker != NO_PIECE) {
            return 30000000
                + PieceValue[victim] * 100
                - PieceValue[attacker]
                + captureHistory[attacker][to_sq(m)][victim] / 4;
        }
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

    return elapsed_ms() >= uint64_t(TIME_LIMIT_MS);
}


void print_info(int depth, int seldepth, int score, uint64_t ms,
    uint64_t nodes, uint64_t nps, uint16_t hashfull,
    Move pv[], int pv_len, const char* bound = nullptr, int multipvIndex = 1)
{
    if (silentOutput)
        return;

    auto is_mate = [](int s) {
        return s >= MATE - 1000;
        };

    auto is_mated = [](int s) {
        return s <= -MATE + 1000;
        };

    auto score_to_mate = [](int s) {
        int plies = (s > 0 ? MATE - s : MATE + s);
        return (plies + 1) / 2;
        };

    std::ostringstream out;

    out << "info depth " << depth
        << " seldepth " << seldepth
        << " multipv " << multipvIndex;

    if (is_mate(score))
        out << " score mate " << score_to_mate(score);
    else if (is_mated(score))
        out << " score mate -" << score_to_mate(score);
    else
        out << " score cp " << score;

    if (bound != nullptr && nodes > nodes_printing)
        out << ' ' << bound;

    out << " time " << ms
        << " nodes " << nodes
        << " nps " << nps
        << " hashfull " << hashfull
        << " pv";

    for (int i = 0; i < pv_len; i++)
        out << ' ' << move_to_string(pv[i]);

    out << '\n';

    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << out.str() << std::flush;
    }
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

    if (pos.halfmoveClock >= 100) {
        if (in_check(pos, pos.stm)) {
            MoveList legal;
            generate_legal(pos, legal);

            if (legal.size == 0)
                return -MATE + ply;
        }

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

    if (pos.halfmoveClock >= 100) {
        if (in_check(pos, pos.stm)) {
            MoveList legal;
            generate_legal(pos, legal);

            if (legal.size == 0)
                return -MATE + ply;
        }

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
        if (ply != 0 && !PvNode)
            return ttScore;
    }

    int eval = evaluate(pos);
    bool inCheck = in_check(pos, pos.stm);

    if (inCheck)
        goto moves_loop;

    // Reverse Futility Pruning (RFP)
    if (depth <= 6 && !rootNode && !is_decisive(beta) && beta <= eval - 100 * depth)
        return eval;

    // Null Move Pruning (NMP)
    if (depth >= 3 && !PvNode && !rootNode && eval >= beta + 200) {
        const int reduction = 4;

        State nullState;
        pos.make_null_move(nullState);

        Move nullPV[128];
        int nullPV_len = 0;

        int nullValue = -negamax<NonPV>(pos, depth - reduction, ply + 1, -beta, -beta + 1, nullPV, nullPV_len, !cutNode);

        pos.unmake_null_move(nullState);

        if (interrupted)
            return 0;

        if (nullValue >= beta) {
            return nullValue;
        }
    }

    // Razoring
    if (!PvNode && !rootNode && !is_decisive(alpha) && eval + 250 * depth <= alpha)
        return qsearch<NonPV>(pos, alpha, beta, ply);
   
moves_loop:

    int bestScore = -VALUE_INFINITE;

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
    Move quietMoves[256];

    int quietMoveCount = 0;
    int childPV_len = 0;
    int moveCount = 0;

    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];

        if (rootNode) {
            if (pvIdx > 0) {
                bool alreadyUsed = false;

                for (int j = 0; j < pvIdx; j++) {
                    if (m == rootMoves[j].move) {
                        alreadyUsed = true;
                        break;
                    }
                }

                if (alreadyUsed)
                    continue;
            }
        }

        Move ParentMove = m;
        int ParentCurrMove = i + 1;

        bool capture = is_capture(m);
        bool promotion = is_promo(m);

        uint64_t moveNodesStart = nodes;

        State st;
        pos.make_move(m, st);

        bool isCheck = in_check(pos, pos.stm);
        bool quiet = !capture && !promotion && !isCheck;

        // History Pruning
        if (!PvNode && !rootNode && !isCheck && depth <= 4 && moveCount > 1 && quiet && history[from_sq(m)][to_sq(m)] < -750) {
            pos.unmake_move(st);
            continue;
        }

        // Futility Pruning
        if (!PvNode && !rootNode && !isCheck && depth <= 4 && moveCount > 1 && quiet && eval + 120 * depth <= alpha) {
            pos.unmake_move(st);
            continue;
        }

        if (quiet)
            quietMoves[quietMoveCount++] = m;

        ++moveCount;

        int score;
        int reduction = 0;
        int extension = 0;

        if (isCheck)
            extension++;

        if (depth >= 3 && moveCount >= 4 && quiet) {
            reduction = int(0.75 + std::log(depth) * std::log(moveCount) / 2.25);

            // Reduce less at PV nodes.
            if (PvNode)
                reduction--;

            reduction = std::max(reduction, 0);
        }

        if (!silentOutput && showCurrMove && rootNode && nodes > nodes_printing) {
            
            std::ostringstream out;

            out << "info depth " << depth
                << " search depth " << depth + extension - reduction
                << " currmove " << move_to_string(ParentMove)
                << " currmovenumber " << ParentCurrMove
                << " nodes " << nodes << '\n';

            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << out.str() << std::flush;
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

        if (rootNode)
            rootMoveEffort[m] += nodes - moveNodesStart;

        if (interrupted)
            return 0;

        if (score > bestScore) {
            bestScore = score;

            pv[0] = m;
            for (int j = 0; j < childPV_len; j++)
                pv[j + 1] = childPV[j];

            pv_len = childPV_len + 1;

            if (rootNode) {
                rootMoves[pvIdx].move = m;
                rootMoves[pvIdx].score = score;

                rootMoves[pvIdx].pv[0] = m;

                for (int j = 0; j < childPV_len; j++)
                    rootMoves[pvIdx].pv[j + 1] = childPV[j];

                rootMoves[pvIdx].pv_len = childPV_len + 1;
            }
        }

        if (score > alpha)
            alpha = score;

        if (alpha >= beta) {
            int bonus = std::min(2048, depth * depth * 16);

            if (capture) {
                update_capture_history(pos, m, depth);
            }
            else {
                update_history(m, bonus);

                killers[ply][1] = killers[ply][0];
                killers[ply][0] = m;
            }

            for (int j = 0; j < quietMoveCount; j++) {
                if (quietMoves[j] != m)
                    update_history(quietMoves[j], -bonus);
            }

            break;
        }
    }
    if (!interrupted) {

        TTFlag flag;

        if (bestScore <= originalAlpha)
            flag = TT_LOWER;
        else if (bestScore >= beta)
            flag = TT_UPPER;
        else
            flag = TT_EXACT;

        Move bestMove = 0;

        if (pv_len > 0)
            bestMove = pv[0];

        tt_store(pos.hash, depth, ply, bestScore, flag, bestMove);
    }

    return bestScore;

}

static Move search_worker(Board& pos, const SearchLimits& limits, WorkerResult& result);

static int get_best_worker(const std::vector<WorkerResult>& results) {

    int minScore = VALUE_INFINITE;

    // Find the lowest score among valid workers.
    for (const auto& result : results) {
        if (result.bestMove && result.depth > 0 && result.pv_len > 0)
            minScore = std::min(minScore, result.score);
    }

    if (minScore == VALUE_INFINITE)
        return 0;

    // Accumulate votes for each worker's preferred root move.
    std::unordered_map<Move, int64_t> votes;

    for (const auto& result : results) {

        if (!result.bestMove || result.depth == 0 || result.pv_len == 0)
            continue;

        int64_t weight = int64_t(result.score) - minScore + 14;

        votes[result.bestMove] += weight;
    }

    int bestThread = -1;

    for (int i = 0; i < static_cast<int>(results.size()); ++i) {

        const auto& candidate = results[i];

        if (!candidate.bestMove || candidate.depth == 0 || candidate.pv_len == 0)
            continue;

        if (bestThread == -1) {
            bestThread = i;
            continue;
        }

        const auto& current = results[bestThread];

        // Prefer a forced win over a non-winning result.
        if (is_win(candidate.score) && !is_win(current.score)) {
            bestThread = i;
            continue;
        }

        if (!is_win(candidate.score) && is_win(current.score))
            continue;

        // Among winning results, prefer the higher score.
        if (is_win(candidate.score) && is_win(current.score)) {
            if (candidate.score > current.score)
                bestThread = i;

            continue;
        }

        // Avoid a forced loss if another worker has a non-losing result.
        if (is_loss(candidate.score) && !is_loss(current.score))
            continue;

        if (!is_loss(candidate.score) && is_loss(current.score)) {
            bestThread = i;
            continue;
        }

        // Among losing results, prefer the higher score.
        if (is_loss(candidate.score) && is_loss(current.score)) {
            if (candidate.score > current.score)
                bestThread = i;

            continue;
        }

        // Otherwise, compare voting totals.
        int64_t candidateVotes = votes[candidate.bestMove];
        int64_t currentVotes = votes[current.bestMove];

        if (candidateVotes > currentVotes
            || (candidateVotes == currentVotes
                && candidate.pv_len > current.pv_len)) {
            bestThread = i;
        }
    }

    return bestThread == -1 ? 0 : bestThread;
}

Move search_bestmove(Board& pos, const SearchLimits& limits) {
    // Reset shared search state once, before starting any workers.
    stopRequested = false;
    nodes = 0;
    NODE_LIMIT = limits.nodes;
    infiniteSearch = limits.infinite;

    int time = (pos.stm == WHITE ? limits.wtime : limits.btime);
    int inc = (pos.stm == WHITE ? limits.winc : limits.binc);

    if (limits.infinite) {
        // No time limit.
    }
    else if (limits.movetime > 0) {
        int reserve = std::min(20, limits.movetime / 20);

        TIME_LIMIT_MS = std::max(1, limits.movetime - reserve);
        SOFT_TIME_LIMIT_MS = TIME_LIMIT_MS * 9 / 10;
    }
    else if (time > 0) {
        int reserve = std::min(time - 1, std::max(20, time / 50));
        int usableTime = time - reserve;

        int movesToGo = limits.movestogoProvided
            ? std::max(1, limits.movestogo)
            : 25;

        int optimumTime = time / movesToGo + inc * 3 / 4;
        optimumTime = std::clamp(optimumTime, 1, usableTime);

        int maximumTime = std::min(usableTime, std::max(optimumTime * 3, optimumTime + 50));

        SOFT_TIME_LIMIT_MS = optimumTime;
        TIME_LIMIT_MS = maximumTime;
    }
    else {
        infiniteSearch = true;
    }

    startTime = std::chrono::steady_clock::now();

    MoveList legalMoves;
    generate_legal(pos, legalMoves);

    if (legalMoves.size == 1 && !limits.movetime) {
        TIME_LIMIT_MS = std::min(TIME_LIMIT_MS.load(), 500);

        if (SOFT_TIME_LIMIT_MS > 0)
            SOFT_TIME_LIMIT_MS = std::min(
                SOFT_TIME_LIMIT_MS.load(),
                TIME_LIMIT_MS.load()
            );
    }

    // Number of search workers.
    const int threadCount = std::clamp(limits.threads, 1, 128);

    // One result slot per worker.
    std::vector<WorkerResult> results(threadCount);

    std::vector<std::thread> workers;
    workers.reserve(threadCount - 1);

    // Start helper workers.
    for (int i = 1; i < threadCount; ++i) {

        SearchLimits workerLimits = limits;
        workerLimits.silent = true;

        workers.emplace_back(
            [workerPos = pos, workerLimits, &results, i]() mutable {
                search_worker(workerPos, workerLimits, results[i]);
            }
        );
    }

    // Main worker searches on the original position.
    SearchLimits mainLimits = limits;
    search_worker(pos, mainLimits, results[0]);

    // Wait for every helper worker to finish.
    for (auto& worker : workers) {
        if (worker.joinable())
            worker.join();
    }

    // Select the best worker.
    int bestThread = get_best_worker(results);

    const WorkerResult& best = results[bestThread];

    // Print the selected worker's final PV.
    if (best.bestMove && best.pv_len > 0) {
        print_info(best.depth, best.seldepth, best.score, elapsed_ms(), nodes, update_nps(), tt_hashfull(), const_cast<Move*>(best.pv), best.pv_len);
    }

    return best.bestMove ? best.bestMove : results[0].bestMove;
}
static Move search_worker(Board& pos, const SearchLimits& limits, WorkerResult& result) {
    silentOutput = limits.silent;
    multiPV = std::max(1, limits.multipv);
    pvIdx = 0;
    pvLast = 0;

    showCurrMove = limits.show_currmove;

    finalPV_len = 0;
    finalScore = 0;

    currentPV_len = 0;
    currentScore = 0;

    finalDepth = 0;

    std::memset(finalPV, 0, sizeof(finalPV));
    std::memset(currentPV, 0, sizeof(currentPV));

    bool timeManaged =
        limits.movetime > 0 ||
        limits.wtime > 0 || limits.btime > 0 ||
        limits.winc > 0 || limits.binc > 0;

    std::memset(history, 0, sizeof(history));
    std::memset(killers, 0, sizeof(killers));
    std::memset(captureHistory, 0, sizeof(captureHistory));

    MoveList legalMoves;
    generate_legal(pos, legalMoves);

    rootMoves.clear();

    for (int i = 0; i < legalMoves.size; ++i) {
        RootMove rm;
        rm.move = legalMoves.moves[i];
        rootMoves.push_back(rm);
    }

    rootMoveEffort.clear();

    for (int i = 0; i < legalMoves.size; ++i)
        rootMoveEffort[legalMoves.moves[i]] = 0;

    Move bestMove = 0;
    Move pv[128];

    int pv_len = 0;

    uint64_t lastDepthNodes = 0;
    Move previousBestMove = 0;
    int previousIterationScore = 0;
    int stableBestMoveCount = 0;
    bool havePreviousIteration = false;

    int dynamicSoftTimeLimit = SOFT_TIME_LIMIT_MS;

    int deepenDepth = limits.depth > 0 ? std::min(limits.depth, MAX_PLY) : MAX_PLY;

    auto saveResult = [&]() {
        result = WorkerResult{};

        result.depth = finalDepth;
        result.score = finalScore;
        result.seldepth = finalSeldepth;

        if (finalPV_len > 0) {
            result.pv_len = finalPV_len;
            std::copy_n(finalPV, finalPV_len, result.pv);
            result.bestMove = finalPV[0];
        }
        else if (currentPV_len > 0) {
            result.pv_len = currentPV_len;
            std::copy_n(currentPV, currentPV_len, result.pv);
            result.bestMove = currentPV[0];
            result.score = currentScore;
            result.seldepth = currentSeldepth;
        }
        else if (!rootMoves.empty()) {
            result.bestMove = rootMoves[0].move;
        }
        };

    for (int depth = 1; depth <= deepenDepth; depth++) {
        // Soft time limit: don't start another iteration.
        if (depth > 1 && dynamicSoftTimeLimit > 0 && elapsed_ms() >= uint64_t(dynamicSoftTimeLimit))
            break;

        pvIdx = 0;
        pvLast = std::min(multiPV, static_cast<int>(rootMoves.size())) - 1;

        for (auto& rm : rootMoves) {
            rm.score = -VALUE_INFINITE;
            rm.pv_len = 0;
        }

        interrupted = false;
        seldepth = 0;
        rootDepth = depth;

        for (pvIdx = 0; pvIdx <= pvLast; ++pvIdx) {

            int alpha = -VALUE_INFINITE;
            int beta = VALUE_INFINITE;
            int score;
            int window = ASPIRATION_WINDOW;

            if (depth >= 4 && finalPV_len > 0 && pvIdx == 0) {
                alpha = finalScore - window;
                beta = finalScore + window;
            }

            while (true) {
                seldepth = 0;

                score = negamax<Root>(pos, rootDepth, 0, alpha, beta, pv, pv_len, false);

                if (interrupted)
                    break;

                if (score <= alpha) {
                    if (nodes > nodes_printing) {
                        print_info(rootDepth, seldepth, score, elapsed_ms(), nodes, update_nps(),
                            tt_hashfull(), pv, pv_len, "upperbound", pvIdx + 1);
                    }

                    alpha = std::max(-VALUE_INFINITE, alpha - window);
                    window *= 2;
                }
                else if (score >= beta) {
                    if (nodes > nodes_printing) {
                        print_info(rootDepth, seldepth, score, elapsed_ms(), nodes, update_nps(),
                            tt_hashfull(), pv, pv_len, "lowerbound", pvIdx + 1);
                    }

                    beta = std::min(VALUE_INFINITE, beta + window);
                    window *= 2;
                }
                else {
                    break;
                }
            }

            if (interrupted)
                break;

            rootMoves[pvIdx].score = score;
            rootMoves[pvIdx].seldepth = seldepth;

            sort_root_moves();

            for (auto& rm : rootMoves) {
                auto it = rootMoveEffort.find(rm.move);

                if (it != rootMoveEffort.end())
                    rm.effort = it->second;
            }
        }

        uint64_t ms = elapsed_ms();
        uint64_t nps = update_nps();

        if (!interrupted) {
            finalScore = rootMoves[0].score;
            finalPV_len = rootMoves[0].pv_len;
            finalSeldepth = rootMoves[0].seldepth;

            finalDepth = depth;

            for (int i = 0; i < finalPV_len; i++)
                finalPV[i] = rootMoves[0].pv[i];

            for (int i = 0; i <= pvLast; i++) {
                print_info(rootDepth, rootMoves[i].seldepth, rootMoves[i].score,
                    ms, nodes, nps, tt_hashfull(),
                    rootMoves[i].pv, rootMoves[i].pv_len, nullptr, i + 1);
            }

            // --- ADAPTIVE TIME MANAGEMENT ---
            if (timeManaged && !limits.movetime && SOFT_TIME_LIMIT_MS > 0 && finalPV_len > 0) {

                int elapsed = std::max(1, int(elapsed_ms()));

                Move currentBestMove = rootMoves[0].move;

                double timeFactor = 1.0;

                if (havePreviousIteration) {

                    if (currentBestMove == previousBestMove) {
                        stableBestMoveCount++;

                        if (stableBestMoveCount >= 3)
                            timeFactor = 0.75;

                        else if (stableBestMoveCount == 2)
                            timeFactor = 0.85;

                        else
                            timeFactor = 0.95;
                    }
                    else {
                        stableBestMoveCount = 0;
                        timeFactor = 1.15;
                    }

                    // Allow more time if the evaluation drops significantly.
                    int scoreDrop = previousIterationScore - finalScore;

                    if (scoreDrop >= 80)
                        timeFactor += 0.15;
                }

                previousBestMove = currentBestMove;
                previousIterationScore = finalScore;
                havePreviousIteration = true;

                dynamicSoftTimeLimit = std::clamp(int(SOFT_TIME_LIMIT_MS.load() * timeFactor), 1, SOFT_TIME_LIMIT_MS.load());

                int timeLeft = dynamicSoftTimeLimit - elapsed;

                if (timeLeft <= 0) {
                    if (!silentOutput) {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        std::cout << "info string adaptive soft time reached, stopping at depth "
                            << depth << "\n";
                    }
                    break;
                }

                // Estimate the cost of the next iteration.
                uint64_t estimatedNextMs = 2 * (nodes - lastDepthNodes) * 1000ULL / std::max(uint64_t(1), update_nps());

                if (uint64_t(timeLeft) < estimatedNextMs) {
                    if (!silentOutput) {
                        std::lock_guard<std::mutex> lock(outputMutex);
                        std::cout << "info string insufficient time for next iteration, stopping at depth "
                            << depth << "\n";
                    }
                    break;
                }

                lastDepthNodes = nodes;
            }

            continue;
        }

        if (interrupted && (timeManaged || limits.nodes > 0)) {

            if (!silentOutput) {
                std::lock_guard<std::mutex> lock(outputMutex);
                std::cout << "info string search finished before depth "
                    << depth << " was completed, falling back to depth "
                    << (depth - 1) << "\n";
            }

            if (currentPV_len > 0)
                print_info(depth, currentSeldepth, currentScore, ms, nodes, nps, tt_hashfull(), currentPV, currentPV_len);


            if (finalPV_len > 0)
                print_info(depth - 1, finalSeldepth, finalScore, ms, nodes, nps, tt_hashfull(), finalPV, finalPV_len);

            saveResult();
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

        if (!silentOutput) {
            std::lock_guard<std::mutex> lock(outputMutex);

            std::cout << "info string bench summary: "
                << nodes << " nodes " << nps << " nps\n";

            std::cout << "bench: " << ms << " ms "
                << nodes << " nodes "
                << nps << " nps" << std::endl;
        }
    }

    saveResult();
    return result.bestMove ? result.bestMove : bestMove;
}