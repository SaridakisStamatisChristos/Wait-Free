//go:build ignore
// +build ignore

#include "recorder.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>

namespace {
std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch == '"' || ch == '\\') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}
}

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "history.json";
    const std::uint64_t seed = argc > 2 ? std::stoull(argv[2]) : 1ULL;
    constexpr std::size_t capacity = 2;
    constexpr std::uint64_t ops_per_side = 8;

    veriqueue::spsc_queue<std::int64_t, capacity> q;
    vqlin::recorder rec;
    std::atomic<std::uint64_t> next_id{1};

    std::thread producer([&] {
        std::mt19937_64 rng(seed ^ 0x50524F4455434552ULL);
        for (std::uint64_t i = 0; i < ops_per_side; ++i) {
            vqlin::operation op;
            op.id = next_id.fetch_add(1);
            op.client = 0;
            op.kind = "push";
            op.value = static_cast<std::int64_t>(i + 1);
            op.invoke = rec.event();
            if ((rng() & 3U) == 0U) std::this_thread::yield();
            op.success = q.try_push(op.value);
            op.complete = rec.event();
            rec.add(std::move(op));
        }
    });

    std::thread consumer([&] {
        std::mt19937_64 rng(seed ^ 0x434F4E53554D4552ULL);
        for (std::uint64_t i = 0; i < ops_per_side; ++i) {
            vqlin::operation op;
            op.id = next_id.fetch_add(1);
            op.client = 1;
            op.kind = "pop";
            op.invoke = rec.event();
            if ((rng() & 3U) == 0U) std::this_thread::yield();
            std::int64_t value = 0;
            op.success = q.try_pop(value);
            op.result = value;
            op.complete = rec.event();
            rec.add(std::move(op));
        }
    });

    producer.join();
    consumer.join();

    auto ops = rec.operations();
    std::vector<vqlin::operation> sorted(ops.begin(), ops.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.id < b.id; });

    std::ofstream out(path);
    out << "{\n  \"capacity\": " << capacity << ",\n  \"seed\": " << seed << ",\n  \"operations\": [\n";
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
    std::cout << "wrote " << sorted.size() << " operations to " << path << '\n';
}
