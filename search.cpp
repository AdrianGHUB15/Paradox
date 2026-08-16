#include "board.h"
#include "eval.h"
#include "movegen.h"
#include <iostream>
#include <chrono>

uint64_t nodes;   // global node counter

int negamax(Board& pos, int depth, int alpha, int beta) {
    nodes++;

    if (depth == 0)
        return evaluate(pos);

    MoveList list;
    generate_legal(pos, list);

    if (list.size == 0) {
        if (in_check(pos, pos.stm))
            return -30000;
        return 0;
    }

    int best = -100000000;

    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];
        State st;

        pos.make_move(m, st);
        int score = -negamax(pos, depth - 1, -beta, -alpha);
        pos.unmake_move(st);

        if (score > best)
            best = score;

        if (score > alpha)
            alpha = score;

        if (alpha >= beta)
            break;
    }

    return best;
}

Move search_bestmove(Board& pos, int maxDepth) {

    Move bestMove = 0;

    for (int depth = 1; depth <= maxDepth; depth++) {

        nodes = 0;
        auto start = std::chrono::high_resolution_clock::now();

        MoveList list;
        generate_legal(pos, list);

        int bestScore = -100000000;

        for (int i = 0; i < list.size; i++) {
            Move m = list.moves[i];
            State st;

            pos.make_move(m, st);
            int score = -negamax(pos, depth - 1, -100000000, 100000000);
            pos.unmake_move(st);

            if (score > bestScore) {
                bestScore = score;
                bestMove = m;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        if (ms == 0) ms = 1;

        uint64_t nps = nodes * 1000 / ms;

        std::string pv = move_to_string(bestMove);

        std::cout
            << "info depth " << depth
            << " score cp " << bestScore
            << " nodes " << nodes
            << " nps " << nps
            << " time " << ms
            << " pv " << pv
            << "\n";
    }

    return bestMove;
}