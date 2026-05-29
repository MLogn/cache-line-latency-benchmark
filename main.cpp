#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "result.hpp"
#include "utils.hpp"

struct alignas(kCacheLineLen) SignalLine {
    std::atomic<uint64_t> value{0};
    char padding[kCacheLineLen - sizeof(std::atomic<uint64_t>)]{};
};

struct alignas(kCacheLineLen) SingleCacheLineMessage {
    static constexpr size_t kBytes = kCacheLineLen;
    char data[kBytes]{};
};

struct alignas(kCacheLineLen) DoubleCacheLineMessage {
    static constexpr size_t kBytes = 2 * kCacheLineLen;
    char data[kBytes]{};
};

struct Config {
    std::vector<int> cores = {0, 1};
    size_t iterations = 100000;
    size_t warmup = 10000;
    double cycles_per_ns = 1.0;
};

template <typename Message> struct alignas(kCacheLineLen) SharedState {
    SignalLine seq;
    SignalLine ack;
    Message msg;
};

template <typename Message> void fill_message(Message &msg, const uint64_t iteration) {
    auto *words = reinterpret_cast<uint64_t *>(msg.data);
    constexpr size_t kWords = Message::kBytes / sizeof(uint64_t);

    for (size_t i = 0; i < kWords; ++i) {
        words[i] = iteration + i;
    }
}

template <typename Message> uint64_t read_message(const Message &msg) {
    const auto *words = reinterpret_cast<const uint64_t *>(msg.data);
    constexpr size_t kWords = Message::kBytes / sizeof(uint64_t);

    uint64_t sum = 0;
    for (size_t i = 0; i < kWords; ++i) {
        sum += words[i];
    }
    return sum;
}

template <typename Message>
void writer(SharedState<Message> &state, const Config &config, const int cpu_id, std::vector<uint64_t> &send_times) {
    set_affinity(cpu_id);

    const size_t total_iterations = config.warmup + config.iterations;

    for (uint64_t i = 1; i <= total_iterations; ++i) {
        while (state.ack.value.load(std::memory_order_acquire) != i - 1) {
            spin_pause();
        }

        const uint64_t t0 = rdtsc_now();
        fill_message(state.msg, i);
        state.seq.value.store(i, std::memory_order_release);

        if (i > config.warmup) {
            send_times[i - config.warmup - 1] = t0;
        }
    }
}

std::atomic<uint64_t> sink{0};

template <typename Message>
void reader(SharedState<Message> &state, const Config &config, const int cpu_id, std::vector<uint64_t> &receive_times) {
    set_affinity(cpu_id);

    const size_t total_iterations = config.warmup + config.iterations;
    uint64_t local_sink = 0;

    for (uint64_t i = 1; i <= total_iterations; ++i) {
        while (state.seq.value.load(std::memory_order_acquire) != i) {
            spin_pause();
        }

        local_sink += read_message(state.msg);
        const uint64_t t1 = rdtsc_now();

        if (i > config.warmup) {
            receive_times[i - config.warmup - 1] = t1;
        }

        state.ack.value.store(i, std::memory_order_release);
    }

    sink.fetch_xor(local_sink, std::memory_order_relaxed);
}

template <typename Message>
BenchmarkResult benchmark_pair(const int writer_cpu, const int reader_cpu, const Config &config) {
    SharedState<Message> state;
    std::vector<uint64_t> send_times(config.iterations);
    std::vector<uint64_t> receive_times(config.iterations);

    std::thread writer_thread(writer<Message>, std::ref(state), std::cref(config), writer_cpu, std::ref(send_times));
    std::thread reader_thread(reader<Message>, std::ref(state), std::cref(config), reader_cpu, std::ref(receive_times));

    writer_thread.join();
    reader_thread.join();

    std::vector<uint64_t> samples(config.iterations);
    for (size_t i = 0; i < config.iterations; ++i) {
        if (receive_times[i] <= send_times[i]) {
            throw std::runtime_error("receive timestamp is not greater than send timestamp");
        }
        samples[i] = receive_times[i] - send_times[i];
    }

    return calculate_result(std::move(samples), config.cycles_per_ns);
}

template <typename Message> void benchmark(const std::string &title, const Config &config) {
    std::vector results(config.cores.size(), std::vector<BenchmarkResult>(config.cores.size()));

    for (size_t writer_cpu = 0; writer_cpu < config.cores.size(); ++writer_cpu) {
        for (size_t reader_cpu = 0; reader_cpu < config.cores.size(); ++reader_cpu) {
            if (writer_cpu == reader_cpu) {
                continue;
            }

            results[writer_cpu][reader_cpu] =
                benchmark_pair<Message>(config.cores[writer_cpu], config.cores[reader_cpu], config);
        }
    }

    print_results(title, config.cores, results);
}

int main(int argc, char **argv) {
    Config config;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--cores") {
                config.cores = parse_cores(argv[++i]);
            } else if (arg == "--iterations") {
                config.iterations = std::stoull(argv[++i]);
            } else if (arg == "--warmup") {
                config.warmup = std::stoull(argv[++i]);
            } else {
                throw std::runtime_error("unknown argument: " + arg);
            }
        }

        config.cycles_per_ns = calibrate_cycles_per_ns();

        std::cout << "cores:";
        for (const auto core : config.cores) {
            std::cout << " " << core;
        }
        std::cout << "\niterations: " << config.iterations << ", warmup: " << config.warmup << "\n";

        benchmark<SingleCacheLineMessage>("Single", config);
        benchmark<DoubleCacheLineMessage>("Double", config);
    } catch (const std::exception &error) {
        std::cerr << "error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
