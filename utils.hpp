#pragma once

#include <chrono>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <sched.h>
#include <time.h>
#include <x86intrin.h>

constexpr size_t kCacheLineLen = 64;
constexpr int kCalibrationMs = 200;

inline void spin_pause() { _mm_pause(); }

inline uint64_t rdtsc_now() {
    _mm_lfence();
    const uint64_t tsc = __rdtsc();
    _mm_lfence();
    return tsc;
}

inline uint64_t now_ns() {
    timespec ts{};
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        throw std::runtime_error("clock_gettime failed");
    }
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

inline double calibrate_cycles_per_ns() {
    const uint64_t ns0 = now_ns();
    const uint64_t tsc0 = rdtsc_now();

    std::this_thread::sleep_for(std::chrono::milliseconds(kCalibrationMs));

    const uint64_t tsc1 = rdtsc_now();
    const uint64_t ns1 = now_ns();

    return static_cast<double>(tsc1 - tsc0) / static_cast<double>(ns1 - ns0);
}

inline void set_affinity(const int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);

    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        throw std::runtime_error("sched_setaffinity failed");
    }
}

inline std::vector<int> parse_cores(const std::string &value) {
    std::vector<int> cores;
    std::stringstream stream(value);
    std::string item;

    while (std::getline(stream, item, ',')) {
        cores.push_back(std::stoi(item));
    }

    if (cores.size() < 2) {
        throw std::runtime_error("at least two cores are required");
    }

    return cores;
}
