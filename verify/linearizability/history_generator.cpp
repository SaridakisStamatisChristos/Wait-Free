//go:build ignore
// +build ignore

#include "recorder.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

enum class schedule_profile {
    uniform,
    burst,
    producer_heavy,
    consumer_heavy,
};

enum class api_mode {
    scalar,
    mixed,
};

std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == '"' || ch == '\\') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}

schedule_profile parse_profile(const std::string& profile) {
    if (profile == "uniform") return schedule_profile::uniform;
    if (profile == "burst") return schedule_profile::burst;
    if (profile == "producer-heavy") return schedule_profile::producer_heavy;
    if (profile == "consumer-heavy") return schedule_profile::consumer_heavy;
    throw std::invalid_argument("unknown schedule profile: " + profile);
}

api_mode parse_mode(const std::string& mode) {
    if (mode == "scalar") return api_mode::scalar;
    if (mode == "mixed") return api_mode::mixed;
    throw std::invalid_argument("unknown API mode: " + mode);
}

void perturb(schedule_profile profile, bool producer, std::mt19937_64& rng) {
    switch (profile) {
    case schedule_profile::uniform:
        if ((rng() & 3ULL) == 0ULL) std::this_thread::yield();
        break;
    case schedule_profile::burst:
        if ((rng() & 7ULL) == 0ULL) {
            for (int i = 0; i < 4; ++i) std::this_thread::yield();
        }
        break;
    case schedule_profile::producer_heavy:
        if (producer) {
            if ((rng() & 15ULL) == 0ULL) std::this_thread::yield();
        } else if ((rng() & 1ULL) == 0ULL) {
            std::this_thread::yield();
        }
        break;
    case schedule_profile::consumer_heavy:
        if (!producer) {
            if ((rng() & 15ULL) == 0ULL) std::this_thread::yield();
        } else if ((rng() & 1ULL) == 0ULL) {
            std::this_thread::yield();
        }
        break;
    }
}

void write_array(std::ostream& out, const std::vector<std::int64_t>& values) {
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << ',';
        out << values[i];
    }
    out << ']';
}

template <std::size_t Capacity>
int generate_history(const std::string& path, std::uint64_t seed, std::uint64_t ops_per_side,
                     const std::string& profile_name, const std::string& mode_name) {
    const schedule_profile profile = parse_profile(profile_name);
    const api_mode mode = parse_mode(mode_name);

    veriqueue::spsc_queue<std::int64_t, Capacity> q;
    vqlin::recorder rec;
    std::atomic<std::uint64_t> next_id{1};

    std::thread producer([&] {
        std::mt19937_64 rng(seed ^ 0x50524F4455434552ULL);
        std::int64_t next_value = 1;
        for (std::uint64_t i = 0; i < ops_per_side; ++i) {
            vqlin::operation op;
            op.id = next_id.fetch_add(1, std::memory_order_relaxed);
            op.client = 0;

            const bool use_bulk = mode == api_mode::mixed && (i % 2U) == 1U;
            if (use_bulk) {
                op.kind = "push_bulk";
                op.requested = 2U + (i % 2U);
                op.values.reserve(static_cast<std::size_t>(op.requested));
                for (std::uint64_t j = 0; j < op.requested; ++j) {
                    op.values.push_back(next_value++);
                }
                op.invoke = rec.event();
                perturb(profile, true, rng);
                op.count = q.try_push_bulk(std::span<const std::int64_t>{op.values});
                op.success = op.count != 0U;
                op.complete = rec.event();
            } else {
                op.kind = "push";
                op.value = next_value++;
                op.invoke = rec.event();
                perturb(profile, true, rng);
                op.success = q.try_push(op.value);
                op.complete = rec.event();
            }
            rec.add(std::move(op));
        }
    });

    std::thread consumer([&] {
        std::mt19937_64 rng(seed ^ 0x434F4E53554D4552ULL);
        for (std::uint64_t i = 0; i < ops_per_side; ++i) {
            vqlin::operation op;
            op.id = next_id.fetch_add(1, std::memory_order_relaxed);
            op.client = 1;
            op.invoke = rec.event();
            perturb(profile, false, rng);

            if (mode == api_mode::mixed && (i % 3U) == 0U) {
                op.kind = "pop_bulk";
                op.requested = 2U + ((i / 3U) % 2U);
                std::vector<std::int64_t> output(static_cast<std::size_t>(op.requested));
                op.count = q.try_pop_bulk(std::span<std::int64_t>{output});
                op.success = op.count != 0U;
                op.results.assign(output.begin(), output.begin() + static_cast<std::ptrdiff_t>(op.count));
            } else if (mode == api_mode::mixed && (i % 3U) == 1U) {
                op.kind = "consume";
                std::int64_t value = 0;
                op.success = q.try_consume([&](std::int64_t& queued) noexcept { value = queued; });
                op.result = value;
            } else {
                op.kind = "pop";
                std::int64_t value = 0;
                op.success = q.try_pop(value);
                op.result = value;
            }

            op.complete = rec.event();
            rec.add(std::move(op));
        }
    });

    producer.join();
    consumer.join();

    const auto ops = rec.operations();
    std::vector<vqlin::operation> sorted(ops.begin(), ops.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });

    std::ofstream out(path);
    if (!out) {
        std::cerr << "failed to open history output: " << path << '\n';
        return 2;
    }

    out << "{\n"
        << "  \"capacity\": " << Capacity << ",\n"
        << "  \"seed\": " << seed << ",\n"
        << "  \"ops_per_side\": " << ops_per_side << ",\n"
        << "  \"profile\": \"" << esc(profile_name) << "\",\n"
        << "  \"mode\": \"" << esc(mode_name) << "\",\n"
        << "  \"operations\": [\n";
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const auto& op = sorted[i];
        out << "    {\"id\":" << op.id
            << ",\"client\":" << op.client
            << ",\"operation\":\"" << esc(op.kind) << "\""
            << ",\"value\":" << op.value
            << ",\"values\":";
        write_array(out, op.values);
        out << ",\"requested\":" << op.requested
            << ",\"invoke\":" << op.invoke
            << ",\"complete\":" << op.complete
            << ",\"success\":" << (op.success ? "true" : "false")
            << ",\"result\":" << op.result
            << ",\"count\":" << op.count
            << ",\"results\":";
        write_array(out, op.results);
        out << '}';
        if (i + 1 != sorted.size()) out << ',';
        out << '\n';
    }
    out << "  ]\n}\n";

    std::cout << "wrote " << sorted.size() << " operations to " << path
              << " capacity=" << Capacity
              << " seed=" << seed
              << " ops_per_side=" << ops_per_side
              << " profile=" << profile_name
              << " mode=" << mode_name << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string path = argc > 1 ? argv[1] : "history.json";
        const std::uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 1ULL;
        const std::size_t capacity = argc > 3 ? static_cast<std::size_t>(std::stoull(argv[3])) : 2U;
        const std::uint64_t ops_per_side = argc > 4 ? std::stoull(argv[4]) : 8ULL;
        const std::string profile = argc > 5 ? argv[5] : "uniform";
        const std::string mode = argc > 6 ? argv[6] : "scalar";

        switch (capacity) {
        case 1: return generate_history<1>(path, seed, ops_per_side, profile, mode);
        case 2: return generate_history<2>(path, seed, ops_per_side, profile, mode);
        case 4: return generate_history<4>(path, seed, ops_per_side, profile, mode);
        case 8: return generate_history<8>(path, seed, ops_per_side, profile, mode);
        default:
            std::cerr << "unsupported capacity " << capacity << "; expected one of 1,2,4,8\n";
            return 2;
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
