#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace vqlin {

struct operation final {
    std::uint64_t id{};
    std::uint32_t client{};
    std::string kind;
    std::int64_t value{};
    std::uint64_t invoke{};
    std::uint64_t complete{};
    bool success{};
    std::int64_t result{};
};

class recorder final {
public:
    [[nodiscard]] std::uint64_t event() noexcept {
        return clock_.fetch_add(1, std::memory_order_seq_cst);
    }

    void add(operation op) {
        std::scoped_lock lock(mutex_);
        operations_.push_back(std::move(op));
    }

    [[nodiscard]] const std::vector<operation>& operations() const noexcept { return operations_; }

private:
    std::atomic<std::uint64_t> clock_{1};
    std::mutex mutex_;
    std::vector<operation> operations_;
};

} // namespace vqlin
