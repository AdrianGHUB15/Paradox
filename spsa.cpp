#include "spsa.h"
#include "eval.h"
#include "board.h"
#include "search.h"
#include "movegen.h"

#include <vector>
#include <random>
#include <cmath>
#include <iostream>

extern bool Search_Silent;
extern EvalParams evalParams;

// ------------------------------------------------------------
// Flatten parameters → vector<double>
// ------------------------------------------------------------
static void params_to_vector(std::vector<double>& v) {
    v.clear();

    for (int i = 0; i < 6; i++) v.push_back(evalParams.pieceMG[i]);
    for (int i = 0; i < 6; i++) v.push_back(evalParams.pieceEG[i]);

    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstPawnMG[i]);
    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstPawnEG[i]);

    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstKnightMG[i]);
    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstKnightEG[i]);

    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstBishopMG[i]);
    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstBishopEG[i]);

    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstRookMG[i]);
    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstRookEG[i]);

    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstQueenMG[i]);
    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstQueenEG[i]);

    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstKingMG[i]);
    for (int i = 0; i < 64; i++) v.push_back(evalParams.pstKingEG[i]);
}

// ------------------------------------------------------------
// Vector → EvalParams
// ------------------------------------------------------------
static void vector_to_params(const std::vector<double>& v) {
    int k = 0;

    for (int i = 0; i < 6; i++) evalParams.pieceMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 6; i++) evalParams.pieceEG[i] = (int)std::round(v[k++]);

    for (int i = 0; i < 64; i++) evalParams.pstPawnMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 64; i++) evalParams.pstPawnEG[i] = (int)std::round(v[k++]);

    for (int i = 0; i < 64; i++) evalParams.pstKnightMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 64; i++) evalParams.pstKnightEG[i] = (int)std::round(v[k++]);

    for (int i = 0; i < 64; i++) evalParams.pstBishopMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 64; i++) evalParams.pstBishopEG[i] = (int)std::round(v[k++]);

    for (int i = 0; i < 64; i++) evalParams.pstRookMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 64; i++) evalParams.pstRookEG[i] = (int)std::round(v[k++]);

    for (int i = 0; i < 64; i++) evalParams.pstQueenMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 64; i++) evalParams.pstQueenEG[i] = (int)std::round(v[k++]);

    for (int i = 0; i < 64; i++) evalParams.pstKingMG[i] = (int)std::round(v[k++]);
    for (int i = 0; i < 64; i++) evalParams.pstKingEG[i] = (int)std::round(v[k++]);
}

// ------------------------------------------------------------
// Self-play game at fixed depth
// ------------------------------------------------------------
static int play_game(int depth) {
    Board pos;
    pos.set_fen("startpos");

    for (int ply = 0; ply < 200; ply++) {

        Search_Silent = true;
        Move m = search_bestmove(pos, depth, 0);
        Search_Silent = false;

        if (!m) return 0;

        State st;
        pos.make_move(m, st);

        MoveList legal;
        generate_legal(pos, legal);

        if (legal.size == 0) {
            Color stm = pos.stm;
            if (in_check(pos, stm))
                return (stm == WHITE ? -1 : 1);
            return 0;
        }
    }

    return 0;
}

static double evaluate_strength(int games, int depth) {
    int sum = 0;
    for (int i = 0; i < games; i++)
        sum += play_game(depth);
    return (double)sum / games;
}

// ------------------------------------------------------------
// SPSA step
// ------------------------------------------------------------
static void spsa_step(std::vector<double>& theta,
    int gamesPerEval,
    int depth,
    int k,
    std::mt19937& rng)
{
    int n = (int)theta.size();
    std::vector<double> delta(n);
    std::uniform_int_distribution<int> coin(0, 1);

    double a = 0.2;
    double c = 0.1;
    double alpha = 0.602;
    double gamma = 0.101;

    double ak = a / std::pow(k + 1.0, alpha);
    double ck = c / std::pow(k + 1.0, gamma);

    for (int i = 0; i < n; i++)
        delta[i] = (coin(rng) ? 1.0 : -1.0);

    std::vector<double> theta_plus(n), theta_minus(n);

    for (int i = 0; i < n; i++) {
        theta_plus[i] = theta[i] + ck * delta[i];
        theta_minus[i] = theta[i] - ck * delta[i];
    }

    vector_to_params(theta_plus);
    double y_plus = evaluate_strength(gamesPerEval, depth);

    vector_to_params(theta_minus);
    double y_minus = evaluate_strength(gamesPerEval, depth);

    for (int i = 0; i < n; i++) {
        double ghat = (y_plus - y_minus) / (2.0 * ck * delta[i]);
        theta[i] += ak * ghat;
    }

    vector_to_params(theta);

    std::cout << "info string SPSA iter " << k
        << " y+ = " << y_plus
        << " y- = " << y_minus << std::endl;
}

// ------------------------------------------------------------
// Top-level SPSA loop
// ------------------------------------------------------------
void run_spsa(int iterations, int gamesPerEval, int depth) {
    std::mt19937 rng(123456);

    std::vector<double> theta;
    params_to_vector(theta);

    std::cout << "info string SPSA starting with "
        << theta.size() << " parameters" << std::endl;

    for (int k = 0; k < iterations; k++) {
        spsa_step(theta, gamesPerEval, depth, k, rng);
    }

    std::cout << "info string SPSA finished" << std::endl;
}
