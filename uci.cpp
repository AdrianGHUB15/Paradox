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
#include "tt.h"

// ------------------------------------------------------------
// Global board
// ------------------------------------------------------------
static Board g_board;
extern bool stopRequested;
extern bool infiniteSearch;

void perft_break(Board& pos, int depth);
std::uint64_t perft_divide(Board& pos, int depth);

void run_perft_suite_fast();
void run_perft_suite_long();
void run_perft_suite_very_long();
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
static void cmd_setoption(const std::string& line) {

    std::istringstream iss(line);

    std::string word;
    std::string name;
    std::string value;

    iss >> word; // setoption
    iss >> word; // name

    while (iss >> word) {

        if (word == "value")
            break;

        if (!name.empty())
            name += " ";

        name += word;
    }

    std::getline(iss, value);

    while (!value.empty() &&
        (value[0] == ' ' || value[0] == '\t'))
    {
        value.erase(value.begin());
    }

    if (name == "Hash") {

        int mb = std::atoi(value.c_str());

        if (mb < 1)
            mb = 1;

        if (mb > 4096)
            mb = 4096;

        tt_init(mb);

        std::cout
            << "info string Hash set to "
            << mb
            << " MB\n";

        return;
    }

    if (name == "Clear Hash") {

        tt_clear();

        std::cout
            << "info string Hash cleared\n";

        return;
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
// GO command
// ------------------------------------------------------------
static void cmd_go(const std::string& line) {
    tt_clear();

    SearchLimits limits;

    std::istringstream iss(line);
    std::string tok;
    iss >> tok;
    // ------------------------------------------------------------
    // GO divide / perftbreak
    // ------------------------------------------------------------
    {
        std::istringstream iss2(line);
        std::string goTok, mode;
        iss2 >> goTok >> mode;

        if (mode == "divide") {
            int depth;
            iss2 >> depth;
            perft_divide(g_board, depth);
            return;
        }

        if (mode == "perftbreak") {
            int depth;
            iss2 >> depth;
            perft_break(g_board, depth);
            return;
        }
    }

    while (iss >> tok) {
        if (tok == "depth") iss >> limits.depth;
        else if (tok == "movetime") iss >> limits.movetime;
        else if (tok == "wtime") iss >> limits.wtime;
        else if (tok == "btime") iss >> limits.btime;
        else if (tok == "winc") iss >> limits.winc;
        else if (tok == "binc") iss >> limits.binc;
        else if (tok == "nodes") iss >> limits.nodes;
        else if (tok == "infinite") limits.infinite = true;
        else if (tok == "movestogo") {
            iss >> limits.movestogo;
            limits.movestogoProvided = true;
        }

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
            break; // EOF/error: exit rather than spin forever

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line == "uci") {
            std::cout << "id name Paradox 3\n";
            std::cout << "id author Adrian Ladoni\n";
            std::cout << "option name Hash type spin default 16 min 1 max 4096\n";
            std::cout << "option name Threads type spin default 1 min 1 max 1\n";
            std::cout << "uciok\n";
        }
        else if (line == "isready") {
            std::cout << "readyok\n";
        }
        else if (line.rfind("setoption", 0) == 0) {
            cmd_setoption(line);
        }
        else if (line == "perftsuite fast") {
            run_perft_suite_fast();
            std::cout.flush();
        }
        else if (line == "perftsuite long") {
            run_perft_suite_long();
            std::cout.flush();
        }
        else if (line == "perftsuite very long") {
            run_perft_suite_very_long();
            std::cout.flush();
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
        else if (line.rfind("bench", 0) == 0) {
            std::istringstream bss(line);
            std::string btok;
            int bdepth = 0;
            bss >> btok >> bdepth;
            run_bench(bdepth);

            std::cout.flush();
        }
    }
}