#include "eval.h"
#include "movegen.h"
#include "search.h"
#include "see.h"
#include "tt.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <unordered_map>
#include <vector>

uint64_t nodes = 0;
uint64_t NODE_LIMIT = 0;

uint32_t nodes_printing = 10000000ULL;

Move finalPV[128];
Move currentPV[128];

static Move killers[128][2];

static int captureHistory[6][64][6];
static int history[64][64];

static int pvIdx = 0;
static int pvLast = 0;
static int multiPV = 1;

static constexpr int ASPIRATION_WINDOW = 25;

constexpr int MAX_PLY = 128;

int finalPV_len = 0;
int finalScore = 0;

int currentPV_len = 0;
int currentScore = 0;

int VALUE_INFINITE = MATE + 1;

int seldepth = 0;
int currentSeldepth = 0;
int finalSeldepth = 0;

int rootDepth = 0;
int TIME_LIMIT_MS = 0;
int SOFT_TIME_LIMIT_MS = 0;

bool stopRequested = false;
bool infiniteSearch = false;
bool interrupted = false;
bool showCurrMove = true;

struct RootMove {
    Move move = 0;
    Move pv[128]{};

    int score = -VALUE_INFINITE;
    int pv_len = 0;
    int seldepth = 0;
    uint64_t effort = 0;
};

enum NodeType {
    NonPV,
    PV,
    Root
};

static std::vector<RootMove> rootMoves;
static std::unordered_map<Move, uint64_t> rootMoveEffort;

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

    printf(" multipv %d", multipvIndex);

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
        if (ply != 0 && (!PvNode || multiPV == 1))
            return ttScore;
    }

    int eval = evaluate(pos);

    // Reverse Futility Pruning (RFP)
    if (depth <= 6 && !rootNode && !in_check(pos, pos.stm) 
        && !is_decisive(beta) && beta <= eval - 100 * depth)
        return eval;


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

        if (showCurrMove && rootNode && nodes > nodes_printing) {
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

Move search_bestmove(Board& pos, const SearchLimits& limits) {
    multiPV = std::max(1, limits.multipv);
    pvIdx = 0;
    pvLast = 0;

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

    TIME_LIMIT_MS = 0;
    SOFT_TIME_LIMIT_MS = 0;

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

        int maximumTime = std::min(
            usableTime,
            std::max(optimumTime * 3, optimumTime + 50)
        );

        SOFT_TIME_LIMIT_MS = optimumTime;
        TIME_LIMIT_MS = maximumTime;
    }
    else {
        infiniteSearch = true;
    }

    startTime = std::chrono::steady_clock::now();

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

    if (rootMoves.size() == 1 && !limits.movetime) {
        TIME_LIMIT_MS = std::min(TIME_LIMIT_MS, 500);

        if (SOFT_TIME_LIMIT_MS > 0)
            SOFT_TIME_LIMIT_MS = std::min(SOFT_TIME_LIMIT_MS, TIME_LIMIT_MS);
    }

    Move bestMove = 0;
    Move pv[128];

    int pv_len = 0;

    // Cumulative across the whole iterative deepening run, so that the
    // reported nodes/nps and the elapsed time refer to the same interval.
    nodes = 0;
    NODE_LIMIT = limits.nodes;

    uint64_t lastDepthNodes = 0;
    Move previousBestMove = 0;
    int previousIterationScore = 0;
    int stableBestMoveCount = 0;
    bool havePreviousIteration = false;

    int dynamicSoftTimeLimit = SOFT_TIME_LIMIT_MS;

    int deepenDepth = limits.depth > 0 ? std::min(limits.depth, MAX_PLY) : MAX_PLY;

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

                dynamicSoftTimeLimit = std::clamp(int(SOFT_TIME_LIMIT_MS * timeFactor), 1, TIME_LIMIT_MS);

                int timeLeft = dynamicSoftTimeLimit - elapsed;

                if (timeLeft <= 0) {
                    std::cout << "info string adaptive soft time reached, stopping at depth "
                        << depth << "\n";
                    break;
                }

                // Estimate the cost of the next iteration.
                uint64_t estimatedNextMs = 2 * (nodes - lastDepthNodes) * 1000ULL / std::max(uint64_t(1), update_nps());

                if (uint64_t(timeLeft) < estimatedNextMs) {
                    std::cout << "info string insufficient time for next iteration, stopping at depth "
                        << depth << "\n";
                    break;
                }

                lastDepthNodes = nodes;
            }

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