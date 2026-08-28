#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>

#include "board.h"
#include "movegen.h"
#include "move.h"
#include "eval.h"
#include "search.h"
#include <chrono>

// ------------------------------------------------------------
// Global board
// ------------------------------------------------------------
static Board g_board;
extern bool stopRequested;
extern bool infiniteSearch;
extern int MAX_NODES;
extern int MAX_DEPTH;

extern Move run_bench_startpos_depth6();
// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static int uci_square_to_index(const std::string& s, int pos) {
    int file = s[pos] - 'a';
    int rank = s[pos + 1] - '1';
    return rank * 8 + file;
}

static int uci_promo_to_code(char c) {
    switch (c) {
    case 'n': return PROMO_N;
    case 'b': return PROMO_B;
    case 'r': return PROMO_R;
    case 'q': return PROMO_Q;
    default:  return PROMO_NONE;
    }
}

// ------------------------------------------------------------
// Apply UCI moves
// ------------------------------------------------------------
static void apply_moves_uci(Board& pos, const std::string& movesPart) {
    std::stringstream ss(movesPart);
    std::string token;

    while (ss >> token) {
        if (token.size() < 4)
            continue;

        int from = uci_square_to_index(token, 0);
        int to = uci_square_to_index(token, 2);

        int promoCode = PROMO_NONE;
        if (token.size() == 5)
            promoCode = uci_promo_to_code(token[4]);

        MoveList list;
        generate_legal(pos, list);

        Move foundMove = 0;
        for (int i = 0; i < list.size; ++i) {
            Move m = list.moves[i];
            if (from_sq(m) != from || to_sq(m) != to)
                continue;

            if (promo_of(m) != promoCode)
                continue;

            foundMove = m;
            break;
        }

        if (!foundMove)
            continue;

        State st;
        pos.make_move(foundMove, st);
    }
}

// ------------------------------------------------------------
// POSITION command
// ------------------------------------------------------------
static void cmd_position(const std::string& line) {
    if (line.find("startpos") != std::string::npos) {
        g_board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        size_t mpos = line.find("moves");
        if (mpos != std::string::npos) {
            mpos += 5;
            while (mpos < line.size() && line[mpos] == ' ') ++mpos;
            apply_moves_uci(g_board, line.substr(mpos));
        }
        return;
    }

    size_t fenPos = line.find("fen");
    if (fenPos != std::string::npos) {
        fenPos += 3;
        while (fenPos < line.size() && line[fenPos] == ' ') ++fenPos;

        size_t movesPos = line.find(" moves", fenPos);
        std::string fen = (movesPos == std::string::npos
            ? line.substr(fenPos)
            : line.substr(fenPos, movesPos - fenPos));

        g_board.set_fen(fen.c_str());

        if (movesPos != std::string::npos) {
            movesPos += 6;
            while (movesPos < line.size() && line[movesPos] == ' ') ++movesPos;
            apply_moves_uci(g_board, line.substr(movesPos));
        }
    }
}

// ------------------------------------------------------------
// GO command (simple depth-only search)
// ------------------------------------------------------------
static void cmd_go(const std::string& line) {
    SearchLimits limits;

    std::istringstream iss(line);
    std::string tok;
    iss >> tok;

    while (iss >> tok) {
        if (tok == "depth") iss >> limits.depth;
        else if (tok == "movetime") iss >> limits.movetime;
        else if (tok == "wtime") iss >> limits.wtime;
        else if (tok == "btime") iss >> limits.btime;
        else if (tok == "winc") iss >> limits.winc;
        else if (tok == "binc") iss >> limits.binc;
        else if (tok == "nodes") iss >> limits.nodes;
        else if (tok == "infinite") limits.infinite = true;
    }

    // If no parameters → infinite search
    if (!limits.depth && !limits.movetime && !limits.wtime && !limits.btime)
        limits.infinite = true;

    Move best = search_bestmove(g_board, limits);
    std::cout << "bestmove " << move_to_string(best) << "\n";
}


// ------------------------------------------------------------
// UCI LOOP
// ------------------------------------------------------------
void uci_loop() {
    std::string line;

    g_board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    while (true) {
        if (!std::getline(std::cin, line))
            continue;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line == "uci") {
            std::cout << "id name Paradox 3\n";
            std::cout << "id author Adrian Ladoni\n";
            std::cout << "option name Threads type spin default 1 min 1 max 1\n";
            std::cout << "option name Threads type spin default 16 min 1 max 32\n";
            std::cout << "uciok\n";
        }
        else if (line == "isready") {
            std::cout << "readyok\n";
        }
        else if (line.rfind("position", 0) == 0) {
            cmd_position(line);
        }
        else if (line.rfind("go", 0) == 0) {
            cmd_go(line);
        }
        else if (line == "stop") {
            stopRequested = true;
        }
        else if (line == "quit") {
            break;
        }
        else if (line == "d") {
            g_board.print();
        }
        else if (line == "bench") {
            std::cout << "info string running bench depth 6\n";
            Move best = run_bench_startpos_depth6();
            std::cout << "bestmove " << move_to_string(best) << "\n";
        }

        std::cout.flush();
    }
}