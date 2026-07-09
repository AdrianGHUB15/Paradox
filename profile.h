#pragma once
#include <cstdint>
#include <vector>

// Per‑category counters
struct ProfileCounters {
    uint64_t make_move_calls = 0;
    uint64_t make_move_ns = 0;

    uint64_t unmake_move_calls = 0;
    uint64_t unmake_move_ns = 0;

    uint64_t movegen_calls = 0;
    uint64_t movegen_ns = 0;

    uint64_t qsearch_calls = 0;
    uint64_t qsearch_ns = 0;

    uint64_t eval_calls = 0;
    uint64_t eval_ns = 0;

    uint64_t see_calls = 0;
    uint64_t see_ns = 0;

    uint64_t square_attacked_calls = 0;
    uint64_t square_attacked_ns = 0;

    uint64_t in_check_calls = 0;
    uint64_t in_check_ns = 0;

    uint64_t alphabeta_calls = 0;
    uint64_t alphabeta_ns = 0;

    // Sub‑components inside alphabeta
    uint64_t ab_tt_ns = 0;
    uint64_t ab_movegen_ns = 0;
    uint64_t ab_order_ns = 0;
    uint64_t ab_lmr_ns = 0;
    uint64_t ab_pv_ns = 0;
    uint64_t ab_ttstore_ns = 0;
    uint64_t ab_recurse_ns = 0;

    uint64_t ab_tt_calls = 0;
    uint64_t ab_movegen_calls = 0;
    uint64_t ab_order_calls = 0;
    uint64_t ab_lmr_calls = 0;
    uint64_t ab_pv_calls = 0;
    uint64_t ab_ttstore_calls = 0;
    uint64_t ab_recurse_calls = 0;

    uint64_t rec_call1_calls = 0;
    uint64_t rec_call1_ns = 0;

    uint64_t rec_call2_calls = 0;
    uint64_t rec_call2_ns = 0;

    uint64_t rec_call3_calls = 0;
    uint64_t rec_call3_ns = 0;

    uint64_t null_make_calls = 0;
    uint64_t null_make_ns = 0;

    uint64_t null_unmake_calls = 0;
    uint64_t null_unmake_ns = 0;

    uint64_t ab_null_calls = 0;
    uint64_t ab_null_ns = 0;

};

extern ProfileCounters prof;

// High‑res timer
uint64_t now_ns();

// Reset + print
void print_profile();

// --------- Macros ---------

#ifdef PROFILE

#define PROF_START(var) \
        uint64_t var = now_ns()

#define PROF_END(ns_field, calls_field, t0) \
        do { \
            (ns_field)    += now_ns() - (t0); \
            (calls_field) += 1; \
        } while (0)

#else

#define PROF_START(var)
#define PROF_END(ns_field, calls_field, t0)

#endif
