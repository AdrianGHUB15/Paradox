#include "move.h"

static char buf[8];

const char* move_to_string(Move m) {
    int f = from_sq(m);
    int t = to_sq(m);

    buf[0] = 'a' + (f & 7);
    buf[1] = '1' + (f >> 3);
    buf[2] = 'a' + (t & 7);
    buf[3] = '1' + (t >> 3);

    int promo = promo_of(m);

    if (promo)
    {
        switch (promo)
        {
        case PROMO_N: buf[4] = 'n'; break;
        case PROMO_B: buf[4] = 'b'; break;
        case PROMO_R: buf[4] = 'r'; break;
        case PROMO_Q: buf[4] = 'q'; break;
        default:      buf[4] = '?'; break;
        }

        buf[5] = '\0';
    }
    else
    {
        buf[4] = '\0';
    }

    return buf;
}
