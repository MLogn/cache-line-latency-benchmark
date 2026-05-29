#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct BenchmarkResult {
    double p50_ns = 0.0;
    double p90_ns = 0.0;
    double p95_ns = 0.0;
};

inline double percentile(const std::vector<uint64_t> &sorted, const double p) {
    const auto index = static_cast<size_t>(std::ceil(p * sorted.size())) - 1;
    return static_cast<double>(sorted[std::min(index, sorted.size() - 1)]);
}

inline BenchmarkResult calculate_result(std::vector<uint64_t> samples, const double cycles_per_ns) {
    std::sort(samples.begin(), samples.end());

    return {
        .p50_ns = static_cast<double>(samples[samples.size() / 2]) / cycles_per_ns,
        .p90_ns = percentile(samples, 0.90) / cycles_per_ns,
        .p95_ns = percentile(samples, 0.95) / cycles_per_ns,
    };
}

inline void print_results(const std::string &title, const std::vector<int> &cores,
                          const std::vector<std::vector<BenchmarkResult>> &results) {
    std::cout << "\n" << title << " cache line latency, ns (p50/p90/p95)\n";
    std::cout << "from\\to ";
    for (const int core : cores) {
        std::cout << std::setw(22) << core;
    }
    std::cout << "\n";

    for (size_t i = 0; i < cores.size(); ++i) {
        std::cout << std::setw(7) << cores[i];
        for (size_t j = 0; j < cores.size(); ++j) {
            if (i == j) {
                std::cout << std::setw(22) << "-";
                continue;
            }

            std::ostringstream cell;
            cell << std::fixed << std::setprecision(2) << results[i][j].p50_ns << "/" << results[i][j].p90_ns << "/"
                 << results[i][j].p95_ns;
            std::cout << std::setw(22) << cell.str();
        }
        std::cout << "\n";
    }
}
