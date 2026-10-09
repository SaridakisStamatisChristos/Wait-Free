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

template <std::size_t Capacity>
int generate_history(const std::string& path, std::uint64_t seed, std::uint64_t ops_per_side,
                     const std::string& profile_name) {
    const schedule_profile profile = parse_profile(profile_name);

    veriqueue::spsc_queue<std::int64_t, Capacity> q;
    vqlin::recorder rec;
    std::atomic<std::uint64_t> next_id{1};

    std::thread producer([&] {
        std::mt19937_64 rng(seed ^ 0x50524F4455434552ULL);
        for (std::uint64_t i = 0; i < ops_per_side; ++i) {
            vqlin::operation op;
            op.id = next_id.fetch_add(1, std::memory_order_relaxed);
            op.client = 0;
            op.kind = "push";
            op.value = static_cast<std::int64_t>(i + 1);
            op.invoke = rec.event();
            perturb(profile, true, rng);
            op.success = q.try_push(op.value);
            op.complete = rec.event();
            rec.add(std::move(op));
        }
    });

    std::thread consumer([&] {
        std::mt19937_64 rng(seed ^ 0x434F4E53554D4552ULL);
        for (std::uint64_t i = 0; i < ops_per_side; ++i) {
            vqlin::operation op;
            op.id = next_id.fetch_add(1, std::memory_order_relaxed);
            op.client = 1;
            op.kind = "pop";
            op.invoke = rec.event();
            perturb(profile, false, rng);
            std::int64_t value = 0;
            op.success = q.try_pop(value);
            op.result = value;
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
        << "  \"operations\": [\n";
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const auto& op = sorted[i];
        out << "    {\"id\":" << op.id
            << ",\"client\":" << op.client
            << ",\"operation\":\"" << esc(op.kind) << "\""
            << ",\"value\":" << op.value
            << ",\"invoke\":" << op.invoke
            << ",\"complete\":" << op.complete
            << ",\"success\":" << (op.success ? "true" : "false")
            << ",\"result\":" << op.result << "}";
        if (i + 1 != sorted.size()) out << ',';
        out << '\n';
    }
    out << "  ]\n}\n";

    std::cout << "wrote " << sorted.size() << " operations to " << path
              << " capacity=" << Capacity
              << " seed=" << seed
              << " ops_per_side=" << ops_per_side
              << " profile=" << profile_name << '\n';
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

        switch (capacity) {
        case 1: return generate_history<1>(path, seed, ops_per_side, profile);
        case 2: return generate_history<2>(path, seed, ops_per_side, profile);
        case 4: return generate_history<4>(path, seed, ops_per_side, profile);
        case 8: return generate_history<8>(path, seed, ops_per_side, profile);
        default:
            std::cerr << "unsupported capacity " << capacity << "; expected one of 1,2,4,8\n";
            return 2;
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 2;
    }
}
