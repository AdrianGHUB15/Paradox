#include "san.h"
#include "movegen.h"

static bool gives_check(Board& pos, Move m) {
    State st;
    pos.make_move(m, st);
    bool check = in_check(pos, pos.stm);
    pos.unmake_move(st);
    return check;
}

static bool is_mate(Board& pos) {
    if (!in_check(pos, pos.stm)) return false;
    MoveList list;
    generate_legal(pos, list);
    return list.size == 0;
}

std::string move_to_san(Board& pos, Move m) {
    MoveList legal;
    generate_legal(pos, legal);

    // Ensure m is actually legal
    bool found = false;
    for (int i = 0; i < legal.size; i++) {
        if (legal.moves[i] == m) {
            found = true;
            break;
        }
    }
    if (!found) return "(illegal)";

    int from = from_sq(m);
    int to = to_sq(m);
    int promo = promo_of(m);
    int flag = flags_of(m);

    Piece pc = pos.piece_at(from);
    Color us = pos.stm;

    // Castling
    if (flag == FLAG_CASTLING) {
        if (to == 6 || to == 62) return "O-O";
        else return "O-O-O";
    }

    std::string san;

    // Piece letter (pawns are empty)
    if (pc != PAWN) {
        static const char* P = "PNBRQK";
        san += P[pc];
    }

    // Disambiguation
    bool needFile = false, needRank = false;
    if (pc != PAWN) {
        for (int i = 0; i < legal.size; i++) {
            Move m2 = legal.moves[i];
            if (m2 == m) continue;
            if (to_sq(m2) != to_sq(m)) continue;
            if (promo_of(m2) != promo_of(m)) continue;

            int f2 = from_sq(m2);
            Piece pc2 = pos.piece_at(f2);
            if (pc2 != pc) continue;

            int fFile = from & 7;
            int fRank = from >> 3;
            int oFile = f2 & 7;
            int oRank = f2 >> 3;

            if (fFile != oFile) needFile = true;
            if (fRank != oRank) needRank = true;
        }

        if (needFile)
            san += char('a' + (from & 7));
        if (needRank)
            san += char('1' + (from >> 3));
    }

    // Capture marker
    bool isCapture = (flag == FLAG_CAPTURE || flag == FLAG_ENPASSANT);
    if (pc == PAWN && isCapture) {
        san += char('a' + (from & 7));
    }
    if (isCapture) san += 'x';

    // Destination square
    san += char('a' + (to & 7));
    san += char('1' + (to >> 3));

    // Promotion
    if (promo != PROMO_NONE) {
        san += '=';
        san += " NBRQ"[promo];
    }

    // Check / mate
    bool check = gives_check(pos, m);
    if (check) {
        State st;
        pos.make_move(m, st);
        bool mate = is_mate(pos);
        pos.unmake_move(st);
        san += (mate ? '#' : '+');
    }

    return san;
}
