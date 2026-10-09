#include "test_support.hpp"
#include "veriqueue/spsc_queue.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <utility>

struct tracked final {
    static inline std::atomic<int> constructed{0};
    static inline std::atomic<int> destroyed{0};
    static inline std::atomic<int> live{0};
    static inline std::atomic<bool> violation{false};

    std::uint64_t id{0};
    bool valid{true};

    explicit tracked(std::uint64_t value) noexcept : id(value) {
        ++constructed;
        ++live;
    }

    tracked(tracked&& other) noexcept : id(other.id) {
        if (!other.valid) violation = true;
        other.valid = false;
        ++constructed;
        ++live;
    }

    tracked& operator=(tracked&& other) noexcept {
        if (!valid || !other.valid) violation = true;
        id = other.id;
        other.valid = false;
        return *this;
    }

    tracked(const tracked&) = delete;
    tracked& operator=(const tracked&) = delete;

    ~tracked() noexcept {
        ++destroyed;
        --live;
    }

    static void reset() noexcept {
        constructed = 0;
        destroyed = 0;
        live = 0;
        violation = false;
    }
};

struct copy_tracked final {
    static inline std::atomic<int> constructed{0};
    static inline std::atomic<int> destroyed{0};
    static inline std::atomic<int> live{0};

    std::uint64_t id{0};

    explicit copy_tracked(std::uint64_t value) noexcept : id(value) {
        ++constructed;
        ++live;
    }

    copy_tracked(const copy_tracked& other) noexcept : id(other.id) {
        ++constructed;
        ++live;
    }

    copy_tracked(copy_tracked&& other) noexcept : id(other.id) {
        ++constructed;
        ++live;
    }

    copy_tracked& operator=(copy_tracked&& other) noexcept {
        id = other.id;
        return *this;
    }

    copy_tracked& operator=(const copy_tracked&) = delete;

    ~copy_tracked() noexcept {
        ++destroyed;
        --live;
    }

    static void reset() noexcept {
        constructed = 0;
        destroyed = 0;
        live = 0;
    }
};

int main() {
    vqtest::run("drained lifetime", [] {
        tracked::reset();
        {
            veriqueue::spsc_queue<tracked, 8> q;
            VQ_CHECK(q.try_emplace(1));
            VQ_CHECK(q.try_emplace(2));
            tracked out{999};
            VQ_CHECK(q.try_pop(out));
            VQ_CHECK(out.id == 1);
            VQ_CHECK(q.try_pop(out));
            VQ_CHECK(out.id == 2);
        }
        VQ_CHECK(tracked::live.load() == 0);
        VQ_CHECK(tracked::constructed.load() == tracked::destroyed.load());
        VQ_CHECK(!tracked::violation.load());
    });

    vqtest::run("quiescent destructor destroys queued objects", [] {
        tracked::reset();
        {
            veriqueue::spsc_queue<tracked, 8> q;
            VQ_CHECK(q.try_emplace(11));
            VQ_CHECK(q.try_emplace(12));
            VQ_CHECK(q.try_emplace(13));
            VQ_CHECK(tracked::live.load() == 3);
        }
        VQ_CHECK(tracked::live.load() == 0);
        VQ_CHECK(tracked::constructed.load() == tracked::destroyed.load());
        VQ_CHECK(!tracked::violation.load());
    });

    vqtest::run("try_consume retires each live slot exactly once", [] {
        tracked::reset();
        {
            veriqueue::spsc_queue<tracked, 4> q;
            VQ_CHECK(q.try_emplace(21));
            VQ_CHECK(q.try_emplace(22));
            VQ_CHECK(tracked::live.load() == 2);

            std::uint64_t observed = 0;
            VQ_CHECK(q.try_consume([&](tracked& value) noexcept { observed = value.id; }));
            VQ_CHECK(observed == 21);
            VQ_CHECK(tracked::live.load() == 1);
            VQ_CHECK(q.try_consume([&](tracked& value) noexcept { observed = value.id; }));
            VQ_CHECK(observed == 22);
            VQ_CHECK(tracked::live.load() == 0);
        }
        VQ_CHECK(tracked::constructed.load() == tracked::destroyed.load());
        VQ_CHECK(!tracked::violation.load());
    });

    vqtest::run("bulk pop destroys every retired queue object exactly once", [] {
        copy_tracked::reset();
        {
            std::array<copy_tracked, 3> input{copy_tracked{31}, copy_tracked{32}, copy_tracked{33}};
            std::array<copy_tracked, 3> output{copy_tracked{0}, copy_tracked{0}, copy_tracked{0}};
            const int baseline_live = copy_tracked::live.load();

            veriqueue::spsc_queue<copy_tracked, 4> q;
            VQ_CHECK(q.try_push_bulk(std::span<const copy_tracked>{input}) == 3);
            VQ_CHECK(copy_tracked::live.load() == baseline_live + 3);
            const int destroyed_before = copy_tracked::destroyed.load();

            VQ_CHECK(q.try_pop_bulk(std::span<copy_tracked>{output}) == 3);
            VQ_CHECK(output[0].id == 31);
            VQ_CHECK(output[1].id == 32);
            VQ_CHECK(output[2].id == 33);
            VQ_CHECK(copy_tracked::live.load() == baseline_live);
            VQ_CHECK(copy_tracked::destroyed.load() == destroyed_before + 3);
            VQ_CHECK(q.empty());
        }
        VQ_CHECK(copy_tracked::live.load() == 0);
        VQ_CHECK(copy_tracked::constructed.load() == copy_tracked::destroyed.load());
    });
}
