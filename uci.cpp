#define _CRT_SECURE_NO_WARNINGS

#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>

#include "board.h"
#include "perft.h"
#include "movegen.h"
#include "move.h"
#include "search.h"
#include "tt.h"
#include "spsa.h"
#include "eval.h"

// ------------------------------------------------------------
// Global engine state
// ------------------------------------------------------------
static Board g_board;

// From search.cpp
extern bool stop_search;
extern bool Search_Silent;
extern bool Option_ProfilePrint;
extern int  Option_HashMB;
extern int  Option_MultiPV;

void run_bench();

// ------------------------------------------------------------
// Search thread state
// ------------------------------------------------------------
static std::thread       g_searchThread;
static std::atomic<bool> g_searchRunning{ false };

// Always join if joinable — the ONLY safe rule
static void join_search_thread() {
    if (g_searchThread.joinable())
        g_searchThread.join();
    g_searchRunning.store(false);
}

// Worker thread
static void search_worker(int maxDepth, int movetime) {
    Move m = search_bestmove(g_board, maxDepth, movetime);
    g_searchRunning.store(false);
}


// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static int uci_square_to_index(const std::string& s, int pos) {
    int file = s[pos] - 'a';
    int rank = s[pos + 1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;
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
        if (from == -1 || to == -1)
            continue;

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

            int p = promo_of(m);
            if (promoCode != PROMO_NONE && p != promoCode) continue;
            if (promoCode == PROMO_NONE && p != PROMO_NONE) continue;

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
    std::string s = line;

    if (s.find("startpos") != std::string::npos) {
        g_board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

        size_t mpos = s.find("moves");
        if (mpos != std::string::npos) {
            mpos += 5;
            while (mpos < s.size() && s[mpos] == ' ') ++mpos;
            apply_moves_uci(g_board, s.substr(mpos));
        }
        return;
    }

    size_t fenPos = s.find("fen");
    if (fenPos != std::string::npos) {
        fenPos += 3;
        while (fenPos < s.size() && s[fenPos] == ' ') ++fenPos;

        size_t movesPos = s.find(" moves", fenPos);
        std::string fen = (movesPos == std::string::npos
            ? s.substr(fenPos)
            : s.substr(fenPos, movesPos - fenPos));

        g_board.set_fen(fen.c_str());

        if (movesPos != std::string::npos) {
            movesPos += 6;
            while (movesPos < s.size() && s[movesPos] == ' ') ++movesPos;
            apply_moves_uci(g_board, s.substr(movesPos));
        }
    }
}

// ------------------------------------------------------------
// GO command — **correct, safe, final version**
// ------------------------------------------------------------
static void cmd_go(const std::string& line) {

    int depth = 0;
    int movetime = 0;
    int wtime = -1, btime = -1;
    int winc = 0, binc = 0;
    bool infinite = false;

    std::istringstream iss(line);
    std::string tok;
    iss >> tok;

    while (iss >> tok) {
        if (tok == "depth")        iss >> depth;
        else if (tok == "movetime") iss >> movetime;
        else if (tok == "wtime")    iss >> wtime;
        else if (tok == "btime")    iss >> btime;
        else if (tok == "winc")     iss >> winc;
        else if (tok == "binc")     iss >> binc;
        else if (tok == "infinite") infinite = true;
    }

    // 1. Stop any running search
    stop_search = true;
    join_search_thread();

    // 2. Time management
    if (!movetime && !infinite && depth == 0) {
        Color stm = g_board.stm;
        if (stm == WHITE && wtime >= 0)
            movetime = wtime / 20 + winc / 2;
        else if (stm == BLACK && btime >= 0)
            movetime = btime / 20 + binc / 2;
        if (movetime < 0)
            movetime = 0;
    }

    // 3. Reset stop flag
    stop_search = false;

    // 4. Depth
    int maxDepth = depth ? depth : 99;

    // 5. Start new search thread (NO double join, NO reset)
    g_searchRunning.store(true);
    g_searchThread = std::thread(search_worker, maxDepth, movetime);
}

// ------------------------------------------------------------
// SETOPTION command
// ------------------------------------------------------------
static void cmd_setoption(const std::string& line) {
    std::stringstream ss(line);
    std::string tok, name, value;

    ss >> tok; // setoption
    ss >> tok; // name
    if (tok != "name") return;

    while (ss >> tok && tok != "value") {
        if (!name.empty()) name += " ";
        name += tok;
    }

    std::getline(ss, value);
    while (!value.empty() && value.front() == ' ')
        value.erase(value.begin());

    if (name == "ProfilePrint") {
        Option_ProfilePrint = (value == "true" || value == "1");
        std::printf("info string ProfilePrint = %s\n",
            Option_ProfilePrint ? "true" : "false");
        return;
    }

    if (name == "Hash") {
        int mb = 16;
        try { mb = std::stoi(value); }
        catch (...) { mb = 16; }
        if (mb < 1)  mb = 1;
        if (mb > 32) mb = 32;
        Option_HashMB = mb;
        tt_init(Option_HashMB);
        std::printf("info string Hash = %d MB\n", Option_HashMB);
        return;
    }

    if (name == "MultiPV") {
        int mpv = 1;
        try { mpv = std::stoi(value); }
        catch (...) { mpv = 1; }
        if (mpv < 1)   mpv = 1;
        if (mpv > 256) mpv = 256;
        Option_MultiPV = mpv;
        std::printf("info string MultiPV = %d\n", Option_MultiPV);
        return;
    }
}

// ------------------------------------------------------------
// UCI LOOP
// ------------------------------------------------------------
void uci_loop() {
    std::string line;

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    std::cout.tie(nullptr);

    g_board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    while (true) {
        if (!std::getline(std::cin, line)) {
            if (std::cin.eof()) {
                stop_search = true;
                join_search_thread();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line == "uci") {
            std::cout << "id name simple engine 2D\n";
            std::cout << "id author Adrian\n";
            std::cout << "option name ProfilePrint type check default false\n";
            std::cout << "option name Hash type spin default 16 min 1 max 32\n";
            std::cout << "option name MultiPV type spin default 1 min 1 max 256\n";
            std::cout << "uciok\n";
        }
        else if (line == "isready") {
            std::cout << "readyok\n";
        }
        else if (line.rfind("setoption", 0) == 0) {
            cmd_setoption(line);
        }
        else if (line.rfind("position", 0) == 0) {
            cmd_position(line);
        }
        else if (line.rfind("go", 0) == 0) {
            cmd_go(line);
        }
        else if (line == "stop") {
            stop_search = true;
            join_search_thread();
        }
        else if (line == "quit") {
            stop_search = true;
            join_search_thread();
            break;
        }
        else if (line == "d") {
            g_board.print();
        }
        else if (line == "bench") {
            stop_search = true;
            join_search_thread();
            run_bench();
        }
        
        std::cout.flush();
    }
}