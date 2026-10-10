#!/usr/bin/env python3
"""Generate a lab-only observer-placement ablation from the immutable comparator."""
import argparse
from pathlib import Path


def generate(base):
    source = base.replace('struct run_result final {', '''struct run_result final {
    unsigned observer_mode{0};
    std::array<std::uint64_t, 9> observer_geometry{};''', 1)
    begin = source.index('template <std::size_t Bytes, class Push, class Pop>')
    end = source.index('\nvoid emit(', begin)
    original = source[begin:end]
    original = original.replace('run_result run_pair(', 'run_result run_pair_original(', 1)
    original = original.replace('    return {\n', '''    return {
        0U,
        observe_progress(produced, consumed, failed, start, ready),
''', 1)
    common = original.replace('run_pair_original(', 'run_pair_shared(', 1)
    common = common.replace('unsigned pcpu, unsigned ccpu) {',
                            'unsigned pcpu, unsigned ccpu, bool separate) {', 1)
    common = common.replace('    std::atomic<std::uint64_t> produced{0};\n    std::atomic<std::uint64_t> consumed{0};', '''    progress_storage progress(separate);
    auto& produced = *progress.produced;
    auto& consumed = *progress.consumed;''', 1)
    common = common.replace('        0U,\n', '        separate ? 2U : 1U,\n', 1)
    # The complete timed worker/join section stays verbatim in both modes.
    timed_start = '    std::thread producer([&] {'
    timed_end = '    const auto end = std::chrono::steady_clock::now();'
    def timed(text):
        return text[text.index(timed_start):text.index(timed_end)+len(timed_end)]
    assert timed(original) == timed(common) == timed(base)
    utilities = '''
std::array<std::uint64_t, 9> observe_progress(
    const std::atomic<std::uint64_t>& produced, const std::atomic<std::uint64_t>& consumed,
    const std::atomic<bool>& failed, const std::atomic<bool>& start,
    const std::atomic<unsigned>& ready) noexcept {
    const auto p = reinterpret_cast<std::uintptr_t>(&produced);
    const auto c = reinterpret_cast<std::uintptr_t>(&consumed);
    const auto f = reinterpret_cast<std::uintptr_t>(&failed);
    return {p % 4096, c % 4096, f % 4096,
            reinterpret_cast<std::uintptr_t>(&start) % 4096,
            reinterpret_cast<std::uintptr_t>(&ready) % 4096,
            c >= p ? c - p : p - c, c >= p ? 1U : 0U,
            f >= p ? f - p : p - f, f >= p ? 1U : 0U};
}

struct progress_storage final {
    // Same storage size/alignment and lifetime for packed and split modes.
    // Only the consumed counter address changes (8 to 256).
    alignas(256) std::array<std::byte, 512> bytes;
    std::atomic<std::uint64_t>* produced;
    std::atomic<std::uint64_t>* consumed;
    explicit progress_storage(bool separate) noexcept
        : produced(std::construct_at(reinterpret_cast<std::atomic<std::uint64_t>*>(bytes.data()), 0ULL)),
          consumed(std::construct_at(reinterpret_cast<std::atomic<std::uint64_t>*>(
              bytes.data() + (separate ? 256 : sizeof(std::atomic<std::uint64_t>))), 0ULL)) {
        static_assert(sizeof(std::atomic<std::uint64_t>) == 8);
        static_assert(alignof(std::atomic<std::uint64_t>) <= 8);
    }
    ~progress_storage() noexcept { std::destroy_at(consumed); std::destroy_at(produced); }
    progress_storage(const progress_storage&) = delete;
    progress_storage& operator=(const progress_storage&) = delete;
};
'''
    dispatch = '''
template <std::size_t Bytes, class Push, class Pop>
run_result run_pair_dispatch(Push&& push, Pop&& pop, std::uint64_t transfers,
                             unsigned pcpu, unsigned ccpu, unsigned observer_mode) {
    if (observer_mode == 0U)
        return run_pair_original<Bytes>(std::forward<Push>(push), std::forward<Pop>(pop),
                                        transfers, pcpu, ccpu);
    return run_pair_shared<Bytes>(std::forward<Push>(push), std::forward<Pop>(pop),
                                  transfers, pcpu, ccpu, observer_mode == 2U);
}
'''
    source = source[:begin] + utilities + original + common + dispatch + source[end:]
    signature = ('run_result run_implementation(std::string_view implementation, std::uint64_t transfers,\n'
                 '                              vqbench::cpu_pair cpus) {')
    assert source.count(signature) == 1
    source = source.replace(signature, signature + '''
    unsigned observer_mode = 0;
    if (implementation.starts_with("packed/")) { implementation.remove_prefix(7); observer_mode = 1; }
    else if (implementation.starts_with("split/")) { implementation.remove_prefix(6); observer_mode = 2; }
''', 1)
    assert source.count('run_pair<sizeof(Payload)>') == 5
    source = source.replace('run_pair<sizeof(Payload)>', 'run_pair_dispatch<sizeof(Payload)>')
    assert source.count('transfers, cpus.producer, cpus.consumer);') == 5
    source = source.replace('transfers, cpus.producer, cpus.consumer);',
                            'transfers, cpus.producer, cpus.consumer, observer_mode);')
    emit = r'''              << "\",\"environment\":" << vqbench::environment_json() << "}\n";'''
    assert emit in source
    source = source.replace(emit, r'''              << "\"";
    std::cout << ",\"observer_mode\":" << result.observer_mode
              << ",\"observer_geometry\":[";
    for (std::size_t i = 0; i < result.observer_geometry.size(); ++i) {
        if (i != 0) std::cout << ',';
        std::cout << result.observer_geometry[i];
    }
    std::cout << ']' << ",\"environment\":" << vqbench::environment_json() << "}\n";
''', 1)
    return source


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--base', type=Path, default=Path('bench/bench_compare.cpp'))
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.base.read_text()))
