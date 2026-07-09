#include "board.h"
#include "zobrist.h"
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cmath>
Board::Board() {
    std::memset(pieceBB, 0, sizeof(pieceBB));
    std::memset(colorBB, 0, sizeof(colorBB));
    occupiedBB = 0ULL;

    kingSq[WHITE] = kingSq[BLACK] = -1;

    stm = WHITE;
    castling = 0;
    epSquare = -1;

    halfmoveClock = 0;
    fullmoveNumber = 1;

    hash = 0ULL;
}

Piece Board::piece_at(int sq) const {
    Bitboard b = 1ULL << sq;

    if (colorBB[WHITE] & b) {
        for (int p = 0; p < 6; p++)
            if (pieceBB[WHITE][p] & b)
                return Piece(p);
    }

    if (colorBB[BLACK] & b) {
        for (int p = 0; p < 6; p++)
            if (pieceBB[BLACK][p] & b)
                return Piece(p);
    }

    return NO_PIECE;
}

Color Board::color_at(int sq) const {
    Bitboard b = 1ULL << sq;

    if (colorBB[WHITE] & b) return WHITE;
    if (colorBB[BLACK] & b) return BLACK;

    return NO_COLOR;
}

void Board::set_fen(const char* fen) {
    std::memset(pieceBB, 0, sizeof(pieceBB));
    std::memset(colorBB, 0, sizeof(colorBB));
    occupiedBB = 0ULL;

    kingSq[WHITE] = kingSq[BLACK] = -1;

    int sq = 56;
    const char* p = fen;

    // pieces
    while (*p && *p != ' ') {
        char c = *p++;

        if (c == '/') {
            sq -= 16;
            continue;
        }

        if (c >= '1' && c <= '8') {
            sq += (c - '0');
            continue;
        }

        Color col = (c >= 'a' ? BLACK : WHITE);
        Piece pc;

        switch (std::tolower(c)) {
        case 'p': pc = PAWN;   break;
        case 'n': pc = KNIGHT; break;
        case 'b': pc = BISHOP; break;
        case 'r': pc = ROOK;   break;
        case 'q': pc = QUEEN;  break;
        case 'k': pc = KING;   break;
        default:  pc = NO_PIECE; break;
        }

        Bitboard b = 1ULL << sq;
        pieceBB[col][pc] |= b;
        colorBB[col] |= b;
        occupiedBB |= b;

        if (pc == KING)
            kingSq[col] = sq;

        sq++;
    }

    // side to move
    p++;
    stm = (*p == 'w' ? WHITE : BLACK);

    // castling
    p += 2;
    castling = 0;
    if (*p != '-') {
        while (*p != ' ') {
            switch (*p) {
            case 'K': castling |= 1; break;
            case 'Q': castling |= 2; break;
            case 'k': castling |= 4; break;
            case 'q': castling |= 8; break;
            }
            p++;
        }
    }
    else p++;

    // ep
    p++;
    if (*p != '-') {
        int file = p[0] - 'a';
        int rank = p[1] - '1';
        epSquare = rank * 8 + file;
        p += 2;
    }
    else {
        epSquare = -1;
        p++;
    }

    halfmoveClock = 0;
    fullmoveNumber = 1;

    hash = compute_hash(*this);
}

static inline int file_of(int sq) { return sq & 7; }
static inline int rank_of(int sq) { return sq >> 3; }
static inline bool on_board(int sq) { return sq >= 0 && sq < 64; }

void Board::print() const {
    for (int r = 7; r >= 0; r--) {
        std::printf("%d ", r + 1);
        for (int f = 0; f < 8; f++) {
            int sq = r * 8 + f;
            Piece pc = piece_at(sq);
            Color col = color_at(sq);

            char c = '.';
            if (pc != NO_PIECE) {
                static const char* P = "PNBRQK";
                c = P[pc];
                if (col == BLACK) c = std::tolower(c);
            }
            std::printf("%c ", c);
        }
        std::printf("\n");
    }
    std::printf("  a b c d e f g h\n");
}
