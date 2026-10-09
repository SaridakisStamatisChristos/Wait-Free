#pragma once

#include <cstddef>
#include <deque>
#include <optional>

namespace vqmodel {

template <class T>
class bounded_fifo final {
public:
    explicit bounded_fifo(std::size_t capacity) : capacity_(capacity) {}

    bool push(const T& value) {
        if (items_.size() == capacity_) {
            return false;
        }
        items_.push_back(value);
        return true;
    }

    std::optional<T> pop() {
        if (items_.empty()) {
            return std::nullopt;
        }
        T value = items_.front();
        items_.pop_front();
        return value;
    }

    [[nodiscard]] std::size_t size() const noexcept { return items_.size(); }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }

private:
    std::size_t capacity_;
    std::deque<T> items_;
};

} // namespace vqmodel
