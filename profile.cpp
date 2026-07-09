#include "profile.h"
#include <chrono>
#include <iostream>

ProfileCounters prof;

uint64_t now_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

static void show(const char* name, uint64_t ns, uint64_t calls) {
    if (!calls) return;
    double per = double(ns) / double(calls);
    std::cout << name << ": "
        << calls << " calls, "
        << ns << " ns total, "
        << per << " ns/call\n";
}
