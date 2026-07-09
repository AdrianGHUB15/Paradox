
#include "search.h"
#include "board.h"
#include "movegen.h"
#include "move.h"
#include "eval.h"
#include "tt.h"
#include "see.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>

bool Search_Silent = false;
static const int INF = 1000000000;
static const int VALUE_MATE = 32000;
static const int VALUE_MATE_IN_MAX_PLY = VALUE_MATE - 256;
static const int VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;
static uint64_t estNodesNextDepthDepth = 0; // frozen estimate for next depth

bool Option_ProfilePrint = false;
int  Option_HashMB = 16;
int  Option_MultiPV = 1;   // NEW: MultiPV option
static std::chrono::steady_clock::time_point lastInputCheck;

// ---------------- Mate helpers ----------------

inline int to_tt(int v, int ply) {
    if (v >= VALUE_MATE_IN_MAX_PLY)
        return v + ply;
    if (v <= VALUE_MATED_IN_MAX_PLY)
        return v - ply;
    return v;
}

inline int from_tt(int v, int ply) {
    if (v >= VALUE_MATE_IN_MAX_PLY)
        return v - ply;
    if (v <= VALUE_MATED_IN_MAX_PLY)
        return v + ply;
    return v;
}

inline bool is_mate_score(int s) {
    return s >= VALUE_MATE_IN_MAX_PLY || s <= VALUE_MATED_IN_MAX_PLY;
}

inline int mate_distance(int s) {
    if (s >= VALUE_MATE_IN_MAX_PLY)
        return VALUE_MATE - s;
    else
        return -VALUE_MATE - s;
}

// ------------------------------------------------------------

static uint64_t nodes;
static uint64_t nodesAtDepthStart; // nodes at start of current depth
static uint64_t nodesLastDepth;    // nodes spent in previous depth
// --- Pruning statistics ---
static uint64_t stat_tt_cut = 0;
static uint64_t stat_null_cut = 0;
static uint64_t stat_razor_prune = 0;
static uint64_t stat_futility_prune = 0;
static uint64_t stat_lmp_prune = 0;
static uint64_t stat_lmr_reduced = 0;
static uint64_t stat_qsee_prune = 0;   // NEW: SEE prune in qsearch


bool stop_search = false;

// --- Time management ---
int hard_time_limit = 0;   // ms

// PV
static Move PV[64][64];
static int  PV_len[64];

// killers[ply][2]
static Move killer[64][2];

// history[color][from][to]
static int historyTable[2][64][64];

// MVV-LVA
static int mvv_lva[6][6];

// LMR table
static int LMR_TABLE[64][64];

// time
static std::chrono::steady_clock::time_point startTime;
static inline int to_cp(int s) { return s; }
static const std::vector<std::string> SF18_BENCH = {
  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
  "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
  "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
  "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
  "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
  "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
  "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
  "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
  "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
  "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
  "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
  "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
  "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
  "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
  "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
  "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
  "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
  "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
  "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
  "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
  "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
  "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
  "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
  "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
  "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
  "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
  "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
  "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
  "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
  "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
  "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
  "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
  "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
  "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",

  // Positions with high numbers of changed threats
  "k7/2n1n3/1nbNbn2/2NbRBn1/1nbRQR2/2NBRBN1/3N1N2/7K w - - 0 1",
  "K7/8/8/BNQNQNB1/N5N1/R1Q1q2r/n5n1/bnqnqnbk w - - 0 1",

  // 5-man positions
  "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",     // Kc2 - mate
  "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",      // Na2 - mate
  "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1",    // draw

  // 6-man positions
  "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",   // Re5 - mate
  "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",    // Ka2 - mate
  "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1",  // Nd2 - draw

  // 7-man positions
  "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124", // Draw

  // Mate and stalemate positions
  "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
  "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1"
};
void run_bench() {
    int depth = 6; // bench depth

    uint64_t totalNodes = 0;
    // Reset pruning stats
    stat_tt_cut = 0;
    stat_null_cut = 0;
    stat_razor_prune = 0;
    stat_futility_prune = 0;
    stat_lmp_prune = 0;
    stat_lmr_reduced = 0;
    stat_qsee_prune = 0;

    auto benchStart = std::chrono::steady_clock::now();

    for (size_t i = 0; i < SF18_BENCH.size(); i++) {
        const std::string& fenLine = SF18_BENCH[i];

        // Skip "setoption" lines
        if (fenLine.rfind("setoption", 0) == 0)
            continue;

        // Split FEN and optional moves
        std::string fen = fenLine;
        std::string moves;

        size_t mp = fenLine.find(" moves ");
        if (mp != std::string::npos) {
            fen = fenLine.substr(0, mp);
            moves = fenLine.substr(mp + 7);
        }

        // Load position
        Board b;
        b.set_fen(fen.c_str());

        // Apply continuation moves if present
        if (!moves.empty()) {
            std::stringstream ss(moves);
            std::string mv;
            while (ss >> mv) {
                MoveList list;
                generate_legal(b, list);
                for (int k = 0; k < list.size; k++) {
                    if (move_to_string(list.moves[k]) == mv) {
                        State st;
                        b.make_move(list.moves[k], st);
                        break;
                    }
                }
            }
        }

        // Reset search
        stop_search = false;
        nodes = 0;

        // Time search
        auto start = std::chrono::steady_clock::now();
        search_bestmove(b, depth, 0);
        auto end = std::chrono::steady_clock::now();

        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        int nps = ms > 0 ? (int)(nodes * 1000 / ms) : 0;

        totalNodes += nodes;

        // -------------------------
        // Output in your requested format
        // -------------------------
        std::printf("info string Pos %zu (%s)\n", i + 1, fen.c_str());

        if (!moves.empty())
            std::printf("info string   Moves: %s\n", moves.c_str());

        std::printf("info string   Nodes: %llu\n", (unsigned long long)nodes);
        std::printf("info string   Time: %d ms\n", ms);
        std::printf("info string   NPS: %d\n", nps);
        std::printf("\n");
    }

    // Final summary
    auto benchEnd = std::chrono::steady_clock::now();
    int totalMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStart).count();
    int totalNps = totalMs > 0 ? (int)(totalNodes * 1000 / totalMs) : 0;

    std::printf("info string ------------------------------\n");
    std::printf("info string Bench Summary\n");
    std::printf("info string   Total Nodes: %llu\n", (unsigned long long)totalNodes);
    std::printf("info string   Total Time: %d ms\n", totalMs);
    std::printf("info string   Total NPS: %d\n", totalNps);

    std::printf("info string   TT cutoffs:        %llu\n", (unsigned long long)stat_tt_cut);
    std::printf("info string   Null-move cutoffs: %llu\n", (unsigned long long)stat_null_cut);
    std::printf("info string   Razor prunes:      %llu\n", (unsigned long long)stat_razor_prune);
    std::printf("info string   Futility prunes:   %llu\n", (unsigned long long)stat_futility_prune);
    std::printf("info string   LMP prunes:        %llu\n", (unsigned long long)stat_lmp_prune);
    std::printf("info string   LMR reduced moves: %llu\n", (unsigned long long)stat_lmr_reduced);
    std::printf("info string   QSEE prunes:       %llu\n", (unsigned long long)stat_qsee_prune);

    std::printf("info string ------------------------------\n");
}


// ------------------------------------------------------------
// Profiling print
// ------------------------------------------------------------
void print_profile() {
    uint64_t total_ns =
        prof.make_move_ns
        + prof.unmake_move_ns
        + prof.movegen_ns
        + prof.qsearch_ns
        + prof.eval_ns
        + prof.see_ns
        + prof.square_attacked_ns
        + prof.in_check_ns
        + prof.alphabeta_ns
        + prof.ab_tt_ns
        + prof.ab_movegen_ns
        + prof.ab_order_ns
        + prof.ab_lmr_ns
        + prof.ab_recurse_ns
        + prof.ab_pv_ns
        + prof.ab_ttstore_ns
        + prof.rec_call1_ns
        + prof.rec_call2_ns
        + prof.rec_call3_ns
        + prof.ab_null_ns;

    auto show = [&](const char* name, uint64_t ns, uint64_t calls) {
        if (calls == 0) return;
        double per = calls ? double(ns) / double(calls) : 0.0;
        double pct = total_ns ? (100.0 * double(ns) / double(total_ns)) : 0.0;

        std::cout
            << std::left << std::setw(18) << name
            << " calls: " << std::setw(10) << calls
            << " total: " << std::setw(12) << ns << " ns"
            << " avg: " << std::setw(10) << per << " ns/call"
            << " (" << std::fixed << std::setprecision(2) << pct << "%)\n";
        };

    std::cout << "\n=== PROFILING SUMMARY ===\n";

    show("make_move", prof.make_move_ns, prof.make_move_calls);
    show("unmake_move", prof.unmake_move_ns, prof.unmake_move_calls);
    show("movegen", prof.movegen_ns, prof.movegen_calls);
    show("qsearch", prof.qsearch_ns, prof.qsearch_calls);
    show("eval", prof.eval_ns, prof.eval_calls);
    show("see", prof.see_ns, prof.see_calls);
    show("square_attacked", prof.square_attacked_ns, prof.square_attacked_calls);
    show("in_check", prof.in_check_ns, prof.in_check_calls);
    show("alphabeta", prof.alphabeta_ns, prof.alphabeta_calls);
    show("ab_tt", prof.ab_tt_ns, prof.ab_tt_calls);
    show("ab_movegen", prof.ab_movegen_ns, prof.ab_movegen_calls);
    show("ab_order", prof.ab_order_ns, prof.ab_order_calls);
    show("ab_lmr", prof.ab_lmr_ns, prof.ab_lmr_calls);
    show("ab_recurse", prof.ab_recurse_ns, prof.ab_recurse_calls);
    show("ab_pv", prof.ab_pv_ns, prof.ab_pv_calls);
    show("ab_ttstore", prof.ab_ttstore_ns, prof.ab_ttstore_calls);
    show("ab_null", prof.ab_null_ns, prof.ab_null_calls);
    show("rec_call1", prof.rec_call1_ns, prof.rec_call1_calls);
    show("rec_call2", prof.rec_call2_ns, prof.rec_call2_calls);
    show("rec_call3", prof.rec_call3_ns, prof.rec_call3_calls);

    std::cout << "---------------------------------------------\n";
    std::cout << "TOTAL time: " << total_ns << " ns\n";
    std::cout << "=============================\n\n";
};

// ------------------------------------------------------------
// MVV-LVA init
// ------------------------------------------------------------
static void init_mvv_lva() {
    static bool init = false;
    if (init) return;
    init = true;

    int victimVal[6] = { 100, 300, 300, 500, 900, 1000 };
    int attackerVal[6] = { 100, 300, 300, 500, 900, 1000 };

    for (int a = 0; a < 6; ++a)
        for (int v = 0; v < 6; ++v)
            mvv_lva[a][v] = victimVal[v] * 10 - attackerVal[a];
}

// ------------------------------------------------------------
// LMR init
// ------------------------------------------------------------
static void init_lmr() {
    static bool init = false;
    if (init) return;
    init = true;

    for (int depth = 1; depth < 64; ++depth) {
        for (int move = 1; move < 64; ++move) {
            double d = std::max(1.0, (double)depth);
            double m = std::max(1.0, (double)move);
            int r = (int)(std::log(d) * std::log(m) / (double)2);
            if (r < 1) r = 1;
            if (r > depth - 1) r = depth - 1;
            LMR_TABLE[depth][move] = r;
        }
    }
}

static bool time_up() {
    if (hard_time_limit <= 0) return false;
    auto now = std::chrono::steady_clock::now();
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return ms >= hard_time_limit;
}

// ------------------------------------------------------------
// Move ordering
// ------------------------------------------------------------
static int score_move(const Board& pos, Move m, Move ttMove, int ply) {
    if (m == ttMove)
        return 100000000;

    int from = from_sq(m);
    int to = to_sq(m);

    Piece attacker = pos.piece_at(from);
    Piece victim = pos.piece_at(to);
    int flags = move_flags(m);

    if (victim != NO_PIECE) {
        int a = attacker >= PAWN && attacker <= KING ? attacker : PAWN;
        int v = victim >= PAWN && victim <= KING ? victim : PAWN;
        return 5000000 + mvv_lva[a][v];
    }

    if (killer[ply][0] == m) return 4000000;
    if (killer[ply][1] == m) return 3999000;

    if (flags == FLAG_DBL_PUSH)
        return 3500000;

    return historyTable[pos.stm][from][to];
}

static void order_moves(const Board& pos, MoveList& list, Move ttMove, int ply) {
    for (int i = 0; i < list.size; ++i)
        list.scores[i] = score_move(pos, list.moves[i], ttMove, ply);

    for (int i = 0; i < list.size; ++i) {
        int best = i;
        for (int j = i + 1; j < list.size; ++j)
            if (list.scores[j] > list.scores[best])
                best = j;
        if (best != i) {
            std::swap(list.scores[i], list.scores[best]);
            std::swap(list.moves[i], list.moves[best]);
        }
    }
}

// ------------------------------------------------------------
// Quiescence
// ------------------------------------------------------------
static int quiescence(Board& pos, int alpha, int beta, int ply) {
    if (stop_search || time_up()) {
        stop_search = true;
        return 0;
    }
    auto nowCheck = std::chrono::steady_clock::now();
    if (nowCheck - lastInputCheck >= std::chrono::milliseconds(10)) {
        lastInputCheck = nowCheck;
        if (stop_search || time_up())
            return 0;
    }

    nodes++;

    // TT probe
    TTEntry tte;
    if (tt_probe(pos.hash, tte)) {
        int ttScore = from_tt(tte.score, ply);

        if (tte.flag == TT_EXACT) {
            stat_tt_cut++;
            return ttScore;
        }
        if (tte.flag == TT_ALPHA && ttScore <= alpha) {
            stat_tt_cut++;
            return ttScore;
        }
        if (tte.flag == TT_BETA && ttScore >= beta) {
            stat_tt_cut++;
            return ttScore;
        }
    }


    int stand = evaluate(pos);

    // Stand pat
    if (stand >= beta) {
        tt_store(pos.hash, 0, to_tt(stand, ply), TT_BETA, 0);
        if (ply >= 62)
            return stand;

    }

    if (stand > alpha)
        alpha = stand;

    MoveList caps;
    generate_captures(pos, caps);

    // TT move ordering
    Move ttMove = tte.move;
    order_moves(pos, caps, ttMove, ply);

    int bestScore = stand;
    Move bestMove = 0;

    int captureCount = 0;

    for (int i = 0; i < caps.size; i++) {
        if (stop_search) return 0;
        Move m = caps.moves[i];

        // SEE pruning
        if (see(pos, m) < 0) {
            stat_qsee_prune++;
            continue;
        }

        // Capture limit pruning
        if (++captureCount > 32)
            break;

     

        State st;
        if (!pos.make_move(m, st))
            continue;

        Color us = Color(pos.stm ^ 1);
        if (in_check(pos, us)) {
            pos.unmake_move(st);
            continue;
        }

        int score = -quiescence(pos, -beta, -alpha, ply + 1);

        pos.unmake_move(st);
        
        if (stop_search)
            return score;

        if (score >= beta) {
            tt_store(pos.hash, 0, to_tt(score, ply), TT_BETA, m);
            return score;
        }

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;

            if (score > alpha)
                alpha = score;
        }
    }

    // Store in TT
    TTFlag flag;
    if (bestScore <= stand) flag = TT_ALPHA;
    else if (bestScore >= beta) flag = TT_BETA;
    else flag = TT_EXACT;

    tt_store(pos.hash, 0, to_tt(bestScore, ply), flag, bestMove);

    return bestScore;
}

// ------------------------------------------------------------
// Alpha-beta
// ------------------------------------------------------------
static int alphabeta(Board& pos, int depth, int ply, int alpha, int beta, int rootDepth) {
    if (stop_search || time_up())
        return 0;
    int p = (ply < 63 ? ply : 62);
    PV_len[p] = 0;
    ply = std::min(ply, 62);
    depth = std::min(depth, 62);


    uint64_t t0 = now_ns();
    prof.alphabeta_calls++;

    auto finish = [&](int result) {
        prof.alphabeta_ns += now_ns() - t0;
        return result;
        };

    auto nowCheck = std::chrono::steady_clock::now();
    if (nowCheck - lastInputCheck >= std::chrono::milliseconds(10)) {
        lastInputCheck = nowCheck;

        // UCI thread may have set stop_search
        if (stop_search || time_up()) {
            stop_search = true;
            return finish(0);
        }
    }

    if (ply >= 62) {
        return finish(evaluate(pos));
    }

    nodes++;

    // Clamp ply to safe range for repetition logic
    int rep_ply = (ply < 63 ? ply : 62);

    if (pos.is_repetition(rep_ply))
        return finish(0);


    Color us = pos.stm;
    bool inCheckNow = in_check(pos, us);
    if (inCheckNow)
        depth++;

    if (depth <= 0)
        return finish(quiescence(pos, alpha, beta, ply));
    int staticEval = evaluate(pos);

    // --- RAZORING (safe version) ---
    if (!inCheckNow && depth <= 3) {
        if (staticEval + 200 * depth <= alpha) {
            int q = quiescence(pos, alpha, beta, ply);
            if (q <= alpha) {
                stat_razor_prune++;
                return finish(q);
            }
        }
    }


    alpha = std::max(alpha, -VALUE_MATE + ply);
    beta = std::min(beta, VALUE_MATE - ply);
    if (alpha >= beta)
        return finish(alpha);

    // NULL MOVE PRUNING (improved)
    uint64_t nm_t0 = now_ns();
    prof.ab_null_calls++;

    // Safe conditions:
    // - not in check
    // - enough depth
    // - enough non-pawn material (avoid zugzwang)
    // - beta is not "mate-ish"
    if (!inCheckNow &&
        depth >= 3 &&
        pos.non_pawn_material(us) >= 200 &&        // was 500, now more permissive
        beta < VALUE_MATE_IN_MAX_PLY)             // avoid pruning near mate scores
    {
        State st;
        pos.make_null_move(st);

        // Reduction R: slightly more aggressive
        // Typical: R = 3 + depth / 6 for mid depths
        int R = 3 + depth / 6;
        if (R > depth - 2)
            R = depth - 2;

        int score = -alphabeta(pos, depth - 1 - R, ply + 1, -beta, -beta + 1, rootDepth);

        pos.unmake_null_move(st);

        if (stop_search) {
            prof.ab_null_ns += now_ns() - nm_t0;
            return finish(0);
        }

        if (score >= beta) {
            stat_null_cut++;  // count null-move cutoffs
            prof.ab_null_ns += now_ns() - nm_t0;
            return finish(beta);
        }
    }

    prof.ab_null_ns += now_ns() - nm_t0;


    // TT PROBE
    uint64_t tt_t0 = now_ns();
    prof.ab_tt_calls++;

    TTEntry tte;
    Move ttMove = 0;

    if (tt_probe(pos.hash, tte)) {
        if (tte.depth >= depth) {
            int ttScore = from_tt(tte.score, ply);
            prof.ab_tt_ns += now_ns() - tt_t0;

            if (tte.flag == TT_EXACT) {
                stat_tt_cut++;
                return finish(ttScore);
            }
            if (tte.flag == TT_ALPHA && ttScore <= alpha) {
                stat_tt_cut++;
                return finish(ttScore);
            }
            if (tte.flag == TT_BETA && ttScore >= beta) {
                stat_tt_cut++;
                return finish(ttScore);
            }
        }
        ttMove = (Move)tte.move;

        // Validate TT move
        if (ttMove) {
            bool legal = false;
            MoveList legalMoves;
            generate_legal(pos, legalMoves);
            for (int i = 0; i < legalMoves.size; i++)
                if (legalMoves.moves[i] == ttMove)
                    legal = true;
            if (!legal)
                ttMove = 0;
        }

    }
    prof.ab_tt_ns += now_ns() - tt_t0;

    // MOVEGEN
    uint64_t mg_t0 = now_ns();
    prof.ab_movegen_calls++;

    MoveList list;
    generate_pseudo(pos, list);

    prof.ab_movegen_ns += now_ns() - mg_t0;

    // ORDERING
    uint64_t ord_t0 = now_ns();
    prof.ab_order_calls++;

    order_moves(pos, list, ttMove, ply);

    prof.ab_order_ns += now_ns() - ord_t0;

    int  bestScore = -INF;
    Move bestMove = 0;
    bool raisedAlpha = false;
    int  movesTried = 0;

    // MAIN LOOP
    for (int i = 0; i < list.size; ++i) {
        if (stop_search) return finish(0);
        Move m = list.moves[i];


        int from = from_sq(m);
        int to = to_sq(m);
        int flags = move_flags(m);

        bool isEP = (flags == FLAG_ENPASSANT);
        Piece victim = pos.piece_at(to);
        bool isCapture = (victim != NO_PIECE) || isEP;

        // LMP
        if (!isCapture && depth <= 2 && movesTried >= 6) {
            stat_lmp_prune++;
            continue;
        }

        // STATIC FUTILITY PRUNING (improved)
        if (!inCheckNow && depth <= 2 && !isCapture) {
            int margin = (depth == 1 ? 200 :
                depth == 2 ? 300 :
                400);

            if (staticEval + margin <= alpha) {
                stat_futility_prune++;
                continue;
            }
        }

        State st;
        if (!pos.make_move(m, st))
            continue;

        if (in_check(pos, us)) {
            pos.unmake_move(st);
            continue;
        }

        int newDepth = depth - 1;

        // LMR
        uint64_t lmr_t0 = now_ns();
        prof.ab_lmr_calls++;

        int reduction = 0;
        if (!inCheckNow && !isCapture && depth >= 3 && movesTried > 0) {

            // Clamp indices to avoid overflow
            int d = depth;
            if (d > 62) d = 62;          // depth index max = 62

            int mv = movesTried + 1;
            if (mv > 62) mv = 62;        // move index max = 62

            reduction = LMR_TABLE[d][mv];

            // Also clamp reduction itself
            if (reduction > depth - 1)
                reduction = depth - 1;

            if (reduction > 0)
                stat_lmr_reduced++;
        }

        prof.ab_lmr_ns += now_ns() - lmr_t0;


        // RECURSION
        uint64_t rec_t0 = now_ns();
        prof.ab_recurse_calls++;

        int score;

        uint64_t t0_call1 = now_ns();
        prof.rec_call1_calls++;

        if (movesTried == 0) {
            score = -alphabeta(pos, newDepth, ply + 1, -beta, -alpha, rootDepth);
        }
        else {
            int rScore = -alphabeta(pos, newDepth - reduction, ply + 1, -alpha - 1, -alpha, rootDepth);

            if (rScore > alpha) {
                score = -alphabeta(pos, newDepth, ply + 1, -beta, -alpha, rootDepth);
            }
            else {
                score = rScore;
            }
        }

        prof.rec_call1_ns += now_ns() - t0_call1;
        prof.ab_recurse_ns += now_ns() - rec_t0;

        pos.unmake_move(st);
    
        movesTried++;

        if (stop_search)
            return finish(0);

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;
        }

        if (score > alpha) {
            alpha = score;
            raisedAlpha = true;

            if (score < beta) {
                uint64_t pv_t0 = now_ns();
                prof.ab_pv_calls++;

                int p = (ply < 63 ? ply : 62);
                int pn = (p + 1 < 63 ? p + 1 : 62);

                PV[p][0] = m;

                int limit = std::min(PV_len[pn], 63);
                int safeLimit = std::min(limit, 61);   // j+1 max = 62
                for (int j = 0; j < safeLimit; ++j)
                    PV[p][j + 1] = PV[pn][j];


                PV_len[p] = std::min(limit + 1, 62);


                prof.ab_pv_ns += now_ns() - pv_t0;
            }
        }

        if (alpha >= beta) {
            if (!isCapture) {
                if (ply < 63) {
                    if (killer[ply][0] != m) {
                        killer[ply][1] = killer[ply][0];
                        killer[ply][0] = m;
                    }
                }

                if (ply < 63 && from < 64 && to < 64)
                    historyTable[us][from][to] += depth * depth;


            }
            break;
        }
    }

    if (movesTried == 0) {
        // Only terminal if no legal moves exist
        MoveList legal;
        generate_legal(pos, legal);

        if (legal.size == 0) {
            if (inCheckNow)
                return finish(-VALUE_MATE + ply); // checkmate
            return finish(0); // stalemate
        }

        // Otherwise, pruning removed all moves → search deeper
        return finish(quiescence(pos, alpha, beta, ply));
    }

    // TT STORE
    uint64_t ts_t0 = now_ns();
    prof.ab_ttstore_calls++;

    TTFlag flag;
    if (!raisedAlpha)            flag = TT_ALPHA;
    else if (bestScore >= beta)  flag = TT_BETA;
    else                         flag = TT_EXACT;

    int safeScore = to_tt(bestScore, std::min(ply, 62));

    tt_store(pos.hash, depth, safeScore, flag, (uint16_t)bestMove);

    prof.ab_ttstore_ns += now_ns() - ts_t0;

    return finish(bestScore);
}

// ------------------------------------------------------------
// Root result for MultiPV
// ------------------------------------------------------------
struct RootResult {
    Move move;
    int  score;
    Move pv[64];
    int  pv_len;
};

// ------------------------------------------------------------
// Top-level search (with MultiPV)
// ------------------------------------------------------------
Move search_bestmove(Board& pos, int maxDepth, int timeMs) {

    tt_init(Option_HashMB);
    init_lmr();
    init_mvv_lva();
    tt_new_generation();
    
    // --- fallback move ---
    Move fallbackMove = 0;
    // temporarily disable fallback generation
    // {
    //     static MoveList rootTmp;
    //     generate_legal(pos, rootTmp);
    //     if (rootTmp.size > 0)
    //         fallbackMove = rootTmp.moves[0];
    // }


    nodes = 0;
    nodesAtDepthStart = 0;
    nodesLastDepth = 1;

    stop_search = false;
    startTime = std::chrono::steady_clock::now();
    lastInputCheck = startTime;

    hard_time_limit = (timeMs > 0 ? timeMs : 0);

    std::memset(PV, 0, sizeof(PV));
    std::memset(PV_len, 0, sizeof(PV_len));
    std::memset(killer, 0, sizeof(killer));
    std::memset(historyTable, 0, sizeof(historyTable));

    Move bestMove = 0;
    // --- root move list ---
    static MoveList rootMoves;

    generate_legal(pos, rootMoves);
    
    if (rootMoves.size == 0) {
        if (!Search_Silent) {
            std::printf("bestmove 0000\n");
            std::fflush(stdout);
        }
        return 0;
    }
    
    static RootResult results[256];

    for (int depth = 1; depth <= maxDepth; ++depth) {

        if (stop_search || time_up())
            break;

        nodesAtDepthStart = nodes;
        int resultCount = 0;
        for (int i = 0; i < rootMoves.size; ++i) {

            if (stop_search || time_up())
                goto FINISH;

            Move m = rootMoves.moves[i];
            State st;

            PV_len[1] = 0;
            std::memset(PV[1], 0, sizeof(PV[1]));
            if (!pos.make_move(m, st))
                continue;
            int score = -alphabeta(pos, depth - 1, 1, -INF, INF, depth);

            pos.unmake_move(st);

            if (stop_search || time_up())
                goto FINISH;

            RootResult& rr = results[resultCount++];
            rr.move = m;
            rr.score = score;
            rr.pv_len = 1;
            rr.pv[0] = m;

            if (PV_len[1] > 0 && PV_len[1] < 64) {
                for (int j = 0; j < PV_len[1]; ++j)
                    rr.pv[1 + j] = PV[1][j];
                rr.pv_len = 1 + PV_len[1];
            }
        }

        if (resultCount == 0)
            break;

        std::sort(results, results + resultCount,
            [](const RootResult& a, const RootResult& b) {
                return a.score > b.score;
            });

        bestMove = results[0].move;

        auto now = std::chrono::steady_clock::now();
        int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsedMs <= 0) elapsedMs = 1;
        uint64_t nodesThisDepth = nodes - nodesAtDepthStart;
        int nps = (int)(nodes * 1000 / elapsedMs);

        estNodesNextDepthDepth = nodesThisDepth ? nodesThisDepth * 2 : nodesLastDepth * 2;
        nodesLastDepth = nodesThisDepth;

        int hashfull = tt_hashfull();

        int multi = Option_MultiPV;
        if (multi < 1) multi = 1;
        if (multi > resultCount) multi = resultCount;

        if (!Search_Silent) {
            for (int i = 0; i < multi; ++i) {
                const RootResult& rr = results[i];
                int score = rr.score;
                bool mate = is_mate_score(score);

                std::printf("info depth %d multipv %d ", depth, i + 1);

                if (mate) {
                    int md = mate_distance(score);
                    int plyToMate = (md + 1) / 2;
                    std::printf("score mate %d ", plyToMate);
                }
                else {
                    std::printf("score cp %d ", to_cp(score));
                }

                std::printf("time %d nodes %llu nps %d hashfull %d pv",
                    elapsedMs,
                    (unsigned long long)nodes,
                    nps,
                    hashfull);

                for (int j = 0; j < rr.pv_len; ++j) {
                    std::string ms = move_to_string(rr.pv[j]);
                    std::printf(" %s", ms.c_str());
                }
                std::printf("\n");
            }

            std::printf("info string   prunes: tt %llu  null %llu  razor %llu  fut %llu  lmp %llu  lmr %llu  qsee %llu\n",
                (unsigned long long)stat_tt_cut,
                (unsigned long long)stat_null_cut,
                (unsigned long long)stat_razor_prune,
                (unsigned long long)stat_futility_prune,
                (unsigned long long)stat_lmp_prune,
                (unsigned long long)stat_lmr_reduced,
                (unsigned long long)stat_qsee_prune);
            std::fflush(stdout);
        }

        if (time_up())
            break;
    }

FINISH:

    if (!bestMove)
        bestMove = fallbackMove;

    if (!Search_Silent) {
        std::string bm = move_to_string(bestMove);
        std::printf("bestmove %s", bm.c_str());
        std::printf(" prunes: tt %llu  null %llu  razor %llu  fut %llu  lmp %llu  lmr %llu  qsee %llu\n",
            (unsigned long long)stat_tt_cut,
            (unsigned long long)stat_null_cut,
            (unsigned long long)stat_razor_prune,
            (unsigned long long)stat_futility_prune,
            (unsigned long long)stat_lmp_prune,
            (unsigned long long)stat_lmr_reduced,
            (unsigned long long)stat_qsee_prune);
        std::fflush(stdout);
    }

    if (Option_ProfilePrint)
        print_profile();

    return bestMove;
}
