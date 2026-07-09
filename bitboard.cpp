#include "bitboard.h"
#include <cstdio>

// Global attack tables
Bitboard KNIGHT_ATTACKS[64];
Bitboard KING_ATTACKS[64];
Bitboard PAWN_ATTACKS[2][64];
Bitboard RAYS[64][8];

Bitboard PAWN_PUSH[2][64];
Bitboard PAWN_PUSH2[2][64];
Bitboard bishopMasks[64];
Bitboard rookMasks[64];

Bitboard bishopAttacks[64][512];
Bitboard rookAttacks[64][4096];

// --------------------------------------------
// Knight attacks
// --------------------------------------------
// in some common header (e.g. bitboard.cpp / bitboard.h)
const int DIRS[8] = {
    8,   // 0: N
   -8,   // 1: S
    1,   // 2: E
   -1,   // 3: W
    9,   // 4: NE
    7,   // 5: NW
   -7,   // 6: SE
   -9    // 7: SW
};


static Bitboard knight_from(int sq) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    auto add = [&](int nr, int nf) {
        if ((unsigned)nr < 8u && (unsigned)nf < 8u)
            bb |= 1ULL << (nr * 8 + nf);
        };

    add(r + 2, f + 1);
    add(r + 2, f - 1);
    add(r - 2, f + 1);
    add(r - 2, f - 1);
    add(r + 1, f + 2);
    add(r + 1, f - 2);
    add(r - 1, f + 2);
    add(r - 1, f - 2);

    return bb;
}

// --------------------------------------------
// King attacks
// --------------------------------------------
static Bitboard king_from(int sq) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    for (int dr = -1; dr <= 1; dr++) {
        for (int df = -1; df <= 1; df++) {
            if (dr == 0 && df == 0) continue;

            int nr = r + dr;
            int nf = f + df;

            if ((unsigned)nr < 8u && (unsigned)nf < 8u)
                bb |= 1ULL << (nr * 8 + nf);
        }
    }

    return bb;
}

// --------------------------------------------
// Pawn attacks
// --------------------------------------------
static Bitboard pawn_from(Color c, int sq) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    int nr = r + (c == WHITE ? 1 : -1);

    if ((unsigned)nr < 8u) {
        if (f > 0) bb |= 1ULL << (nr * 8 + (f - 1));
        if (f < 7) bb |= 1ULL << (nr * 8 + (f + 1));
    }

    return bb;
}

// --------------------------------------------
// Bishop sliding attacks
// --------------------------------------------
Bitboard attacks_bishop(int sq, Bitboard occ) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    auto add = [&](int nr, int nf) {
        int s = nr * 8 + nf;
        bb |= 1ULL << s;
        return (occ >> s) & 1ULL;  // stop if occupied
        };

    // NE
    for (int nr = r + 1, nf = f + 1; nr < 8 && nf < 8; nr++, nf++)
        if (add(nr, nf)) break;

    // NW
    for (int nr = r + 1, nf = f - 1; nr < 8 && nf >= 0; nr++, nf--)
        if (add(nr, nf)) break;

    // SE
    for (int nr = r - 1, nf = f + 1; nr >= 0 && nf < 8; nr--, nf++)
        if (add(nr, nf)) break;

    // SW
    for (int nr = r - 1, nf = f - 1; nr >= 0 && nf >= 0; nr--, nf--)
        if (add(nr, nf)) break;

    return bb;
}

// --------------------------------------------
// Rook sliding attacks
// --------------------------------------------
Bitboard attacks_rook(int sq, Bitboard occ) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    auto add = [&](int nr, int nf) {
        int s = nr * 8 + nf;
        bb |= 1ULL << s;
        return (occ >> s) & 1ULL;
        };

    // North
    for (int nr = r + 1; nr < 8; nr++)
        if (add(nr, f)) break;

    // South
    for (int nr = r - 1; nr >= 0; nr--)
        if (add(nr, f)) break;

    // East
    for (int nf = f + 1; nf < 8; nf++)
        if (add(r, nf)) break;

    // West
    for (int nf = f - 1; nf >= 0; nf--)
        if (add(r, nf)) break;

    return bb;
}
Bitboard file_mask[8];
Bitboard rank_mask[8];
Bitboard adjacent_files_mask[8];
Bitboard passed_mask[2][64];

void init_bitboards() {
    for (int sq = 0; sq < 64; sq++) {
        KNIGHT_ATTACKS[sq] = knight_from(sq);
        KING_ATTACKS[sq] = king_from(sq);
        PAWN_ATTACKS[WHITE][sq] = pawn_from(WHITE, sq);
        PAWN_ATTACKS[BLACK][sq] = pawn_from(BLACK, sq);
    }

    // --------------------------------------------------------
    // STEP 7: Pawn push masks
    // --------------------------------------------------------
    for (int sq = 0; sq < 64; sq++) {
        int r = sq >> 3;

        // White single push
        if (r < 7)
            PAWN_PUSH[WHITE][sq] = 1ULL << (sq + 8);
        else
            PAWN_PUSH[WHITE][sq] = 0;

        // Black single push
        if (r > 0)
            PAWN_PUSH[BLACK][sq] = 1ULL << (sq - 8);
        else
            PAWN_PUSH[BLACK][sq] = 0;

        // White double push
        if (r == 1)
            PAWN_PUSH2[WHITE][sq] = 1ULL << (sq + 16);
        else
            PAWN_PUSH2[WHITE][sq] = 0;

        // Black double push
        if (r == 6)
            PAWN_PUSH2[BLACK][sq] = 1ULL << (sq - 16);
        else
            PAWN_PUSH2[BLACK][sq] = 0;
    }
    // --------------------------------------------------------
   // STEP 8: Sliding rays
   // --------------------------------------------------------
    for (int sq = 0; sq < 64; sq++) {
        int f0 = sq & 7;

        for (int d = 0; d < 8; d++) {
            Bitboard ray = 0;
            int step = DIRS[d];
            int s = sq + step;
            int prevFile = f0;

            while (s >= 0 && s < 64) {
                int f = s & 7;

                // stop on wrap (e.g. from h‑file to a‑file)
                int df = f - prevFile;
                if (df > 1 || df < -1)
                    break;


                ray |= 1ULL << s;
                prevFile = f;
                s += step;
            }

            RAYS[sq][d] = ray;
        }
    }

    // --------------------------------------------------------
    // Files
    // --------------------------------------------------------
    for (int f = 0; f < 8; ++f) {
        Bitboard bb = 0;
        for (int r = 0; r < 8; ++r)
            bb |= 1ULL << (r * 8 + f);
        file_mask[f] = bb;
    }

    // ranks
    for (int r = 0; r < 8; ++r) {
        Bitboard bb = 0;
        for (int f = 0; f < 8; ++f)
            bb |= 1ULL << (r * 8 + f);
        rank_mask[r] = bb;
    }

    // adjacent files
    for (int f = 0; f < 8; ++f) {
        Bitboard bb = 0;
        if (f > 0)     bb |= file_mask[f - 1];
        if (f < 7)     bb |= file_mask[f + 1];
        adjacent_files_mask[f] = bb;
    }

    // passed pawn masks
    for (int sq = 0; sq < 64; ++sq) {
        int file = sq & 7;
        int rank = sq >> 3;

        passed_mask[WHITE][sq] = 0;
        for (int r = rank + 1; r < 8; ++r)
            passed_mask[WHITE][sq] |= (adjacent_files_mask[file] | file_mask[file]) & rank_mask[r];

        passed_mask[BLACK][sq] = 0;
        for (int r = rank - 1; r >= 0; --r)
            passed_mask[BLACK][sq] |= (adjacent_files_mask[file] | file_mask[file]) & rank_mask[r];
    }
    init_magics();
}
// -------------------------
// Magic helpers
// -------------------------
Bitboard mask_rook(int sq) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    for (int nr = r + 1; nr < 7; nr++) bb |= 1ULL << (nr * 8 + f);
    for (int nr = r - 1; nr > 0; nr--) bb |= 1ULL << (nr * 8 + f);
    for (int nf = f + 1; nf < 7; nf++) bb |= 1ULL << (r * 8 + nf);
    for (int nf = f - 1; nf > 0; nf--) bb |= 1ULL << (r * 8 + nf);

    return bb;
}

Bitboard mask_bishop(int sq) {
    Bitboard bb = 0;
    int r = sq >> 3;
    int f = sq & 7;

    for (int nr = r + 1, nf = f + 1; nr < 7 && nf < 7; nr++, nf++)
        bb |= 1ULL << (nr * 8 + nf);

    for (int nr = r + 1, nf = f - 1; nr < 7 && nf > 0; nr++, nf--)
        bb |= 1ULL << (nr * 8 + nf);

    for (int nr = r - 1, nf = f + 1; nr > 0 && nf < 7; nr--, nf++)
        bb |= 1ULL << (nr * 8 + nf);

    for (int nr = r - 1, nf = f - 1; nr > 0 && nf > 0; nr--, nf--)
        bb |= 1ULL << (nr * 8 + nf);

    return bb;
}

Bitboard index_to_occ(int index, Bitboard mask) {
    Bitboard occ = 0;
    int bits = popcount(mask);

    for (int i = 0; i < bits; i++) {
        int sq = lsb(mask);
        mask &= mask - 1;
        if (index & (1 << i))
            occ |= 1ULL << sq;
    }

    return occ;
}

// -------------------------
// Global magic data
// -------------------------
Bitboard bishopMagics[64] = {
    0x400c5404032028ULL,
    0x4010412801284024ULL,
    0x41010220840501ULL,
    0x2608087100a00200ULL,
    0x940308640000a0ULL,
    0xa08441005000800ULL,
    0x2180248840102ULL,
    0x11008070080434ULL,
    0x480220c801810c0cULL,
    0x2480148221840ULL,
    0x10040ab320c2ULL,
    0x1010044100201012ULL,
    0x2202020211200004ULL,
    0x201008860090010ULL,
    0x3000408a10a22008ULL,
    0x800120104424202ULL,
    0x16402208280868c0ULL,
    0x2002001014286480ULL,
    0xa10029214811100ULL,
    0x8002104110280ULL,
    0x210101202100800ULL,
    0x108200602112000ULL,
    0x4084230041045000ULL,
    0x423000440c0425ULL,
    0x20042022480244ULL,
    0x202100008b000c0ULL,
    0x40404404110a00ULL,
    0x5088080000202020ULL,
    0x1001001004001ULL,
    0x200490002100200ULL,
    0x8410c2000420830ULL,
    0x2090033020a0100ULL,
    0x804c012040914ULL,
    0x8480478020413ULL,
    0xc2480200304400ULL,
    0x14040400180210ULL,
    0x804004010840100ULL,
    0x291110200040a00ULL,
    0x1042020220004820ULL,
    0x841411100002401ULL,
    0x10c412010044910ULL,
    0x2180510411082000ULL,
    0x22010401020210ULL,
    0x1032011100805ULL,
    0x88100400400ULL,
    0x481040114104200ULL,
    0x40c304010408320eULL,
    0x8240842044a1422ULL,
    0x40004410088a0000ULL,
    0x4040220210c40404ULL,
    0x900120042222000ULL,
    0x1046081100ULL,
    0x888002008504011ULL,
    0x4071042120009ULL,
    0x14100428408c2040ULL,
    0x2300401204854ULL,
    0x440240842086000ULL,
    0x1210004104212101ULL,
    0x104020821080888ULL,
    0x4000080030420220ULL,
    0x10100204104420ULL,
    0x2800001003500108ULL,
    0x2800080350120210ULL,
    0x4004080200401cULL,
};

Bitboard rookMagics[64] = {
    0x4880004000801024ULL,
    0x4040001000200040ULL,
    0x1100104100082000ULL,
    0x2100100009002004ULL,
    0x280024400800800ULL,
    0x4200080402001001ULL,
    0x80020001000080ULL,
    0x1c80084100082080ULL,
    0x11002100800040ULL,
    0x1000401000200040ULL,
    0x1002000410010ULL,
    0x120040200804100ULL,
    0x1000800110004ULL,
    0x1002001008020005ULL,
    0x4412008402000801ULL,
    0x31000040810002ULL,
    0x1080044000200840ULL,
    0x1c0002008100020ULL,
    0x2120004010080040ULL,
    0x210008100100ULL,
    0x98004040040200ULL,
    0x24004040020100ULL,
    0xaa004040800100ULL,
    0x10e64a0000804c11ULL,
    0x80024140002001ULL,
    0x200040100040ULL,
    0x2440410100200010ULL,
    0x4010040040080041ULL,
    0x4000110100080005ULL,
    0x400c010040400200ULL,
    0x4002080400021001ULL,
    0x4081404200010084ULL,
    0x3480002000400040ULL,
    0x100200040401001ULL,
    0x10040800200020ULL,
    0x4208100009002301ULL,
    0x1881000801001004ULL,
    0x916001002000408ULL,
    0x21000401000200ULL,
    0x5002204102002084ULL,
    0x6980022004434000ULL,
    0x50004020004009ULL,
    0x60200100410018ULL,
    0x4000084200220010ULL,
    0x1400100801010004ULL,
    0xc4000201004040ULL,
    0x103081001040002ULL,
    0x4480588c10520001ULL,
    0x280004000200040ULL,
    0x2010004000200040ULL,
    0x2010040800200020ULL,
    0x3010000800440040ULL,
    0x28004004020040ULL,
    0xe000810040200ULL,
    0x9000402000100ULL,
    0x7008208420410200ULL,
    0x200c108000a34101ULL,
    0x4001008020400011ULL,
    0x8088040201202ULL,
    0x30100204c30000bULL,
    0x6002004081002ULL,
    0x7000204000801ULL,
    0x800408802104104ULL,
    0x30210024004082ULL,
};

int bishopShifts[64] = {
    0x3a, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3a,
    0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
    0x3b, 0x3b, 0x39, 0x39, 0x39, 0x39, 0x3b, 0x3b,
    0x3b, 0x3b, 0x39, 0x37, 0x37, 0x39, 0x3b, 0x3b,
    0x3b, 0x3b, 0x39, 0x37, 0x37, 0x39, 0x3b, 0x3b,
    0x3b, 0x3b, 0x39, 0x39, 0x39, 0x39, 0x3b, 0x3b,
    0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b,
    0x3a, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3b, 0x3a,
};

int rookShifts[64] = {
    0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x34,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35,
    0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x34,
};

// -------------------------
// init_magics
// -------------------------
void init_magics() {

    // 1. Build masks
    for (int sq = 0; sq < 64; sq++) {
        bishopMasks[sq] = mask_bishop(sq);
        rookMasks[sq] = mask_rook(sq);
    }

    // 2. Build bishop attack tables
    for (int sq = 0; sq < 64; sq++) {
        Bitboard mask = bishopMasks[sq];
        int shift = bishopShifts[sq];
        int size = 1 << (64 - shift);

        for (int idx = 0; idx < size; idx++) {
            Bitboard occ = index_to_occ(idx, mask);
            Bitboard attacks = attacks_bishop(sq, occ);
            uint32_t magicIndex = (uint32_t)(((occ & bishopMasks[sq]) * bishopMagics[sq]) >> bishopShifts[sq]);
            bishopAttacks[sq][magicIndex] = attacks;
        }
    }

    // 3. Build rook attack tables
    for (int sq = 0; sq < 64; sq++) {
        Bitboard mask = rookMasks[sq];
        int shift = rookShifts[sq];
        int size = 1 << (64 - shift);

        for (int idx = 0; idx < size; idx++) {
            Bitboard occ = index_to_occ(idx, mask);
            Bitboard attacks = attacks_rook(sq, occ);
            uint32_t magicIndex = (uint32_t)(((occ & rookMasks[sq]) * rookMagics[sq]) >> rookShifts[sq]);
            rookAttacks[sq][magicIndex] = attacks;
        }
    }
}