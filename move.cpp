#include "move.h"
#include <cstdio>

static char buf[8];

const char* move_to_string(Move m) {
    int f = from_sq(m);
    int t = to_sq(m);

    buf[0] = 'a' + (f & 7);
    buf[1] = '1' + (f >> 3);
    buf[2] = 'a' + (t & 7);
    buf[3] = '1' + (t >> 3);

    int promo = promo_of(m);
    if (promo) {
        buf[4] = " nbrq"[promo];
        buf[5] = 0;
    }
    else {
        buf[4] = 0;
    }

    return buf;
}
