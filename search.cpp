#include <chrono>
#include <iostream>
#include <algorithm>

#include "board.h"
#include "eval.h"
#include "movegen.h"

uint64_t nodes = 0;
std::chrono::steady_clock::time_point startTime;
int TIME_LIMIT_MS = 0;

static int history[64][64];

int move_score(Move m) {
    int from = from_sq(m);
    int to = to_sq(m);
    return history[from][to];
}

bool time_up() {
    auto now = std::chrono::steady_clock::now();
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    return ms >= TIME_LIMIT_MS;
}

int negamax(Board& pos, int depth, int alpha, int beta, Move pv[], int& pv_len) {
    nodes++;

    if (time_up())
        return 0;

    if (depth == 0) {
        pv_len = 0;
        return evaluate(pos);
    }

    MoveList list;
    generate_legal(pos, list);

    if (list.size == 0) {
        pv_len = 0;
        if (in_check(pos, pos.stm))
            return -30000;
        return 0;
    }
    std::sort(list.moves, list.moves + list.size,
        [&](Move a, Move b) {
            return move_score(a) > move_score(b);
        });

    int bestScore = -100000000;
    Move bestMove = 0;

    Move childPV[128];
    int childPV_len = 0;

    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];
        State st;

        pos.make_move(m, st);
        int score = -negamax(pos, depth - 1, -beta, -alpha, childPV, childPV_len);
        pos.unmake_move(st);

        if (time_up())
            break;

        if (score > bestScore) {
            bestScore = score;
            bestMove = m;

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

Move search_bestmove(Board& pos, int movetime) {
    TIME_LIMIT_MS = movetime;
    startTime = std::chrono::steady_clock::now();

    std::memset(history, 0, sizeof(history));


    Move bestMove = 0;
    Move pv[128];
    int pv_len = 0;

    for (int depth = 1; depth <= 99; depth++) {

        nodes = 0;
        auto dstart = std::chrono::steady_clock::now();

        int score = negamax(pos, depth, -100000000, 100000000, pv, pv_len);

        auto dend = std::chrono::steady_clock::now();
        int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(dend - dstart).count();
        if (ms == 0) ms = 1;

        uint64_t nps = nodes * 1000 / ms;

        if (pv_len > 0)
            bestMove = pv[0];

        // UCI printing
        std::cout << "info depth " << depth
            << " score cp " << score
            << " time " << ms
            << " nodes " << nodes
            << " nps " << nps
            << " pv";

        for (int i = 0; i < pv_len; i++)
            std::cout << " " << move_to_string(pv[i]);

        std::cout << "\n";

        if (time_up())
            break;
    }

    return bestMove;
}
