# Verification architecture

No single tool establishes correctness. Each layer targets a different failure class.

## Deterministic tests

Boundary behavior, FIFO semantics, capacity 1, full/empty transitions, lifetime accounting, move-only/non-default types, large alignment, queued-object destruction, and constructor failure before publication.

## Accelerated rollover

The exact production template is instantiated with `uint8_t` and `uint16_t` indices. Tens/hundreds of thousands of push/pop operations force many modular wraps while preserving the capacity bound.

## Independent sequential model

Generated operation streams execute against both VeriQueue and an independent `std::deque` bounded FIFO. Results, values, and occupancy are compared. An exhaustive small-state enumerator covers short sequences for capacities 1 and 2.

## Native concurrency stress

Producer emits strictly increasing 64-bit IDs; consumer requires the exact next ID. This detects loss, duplication, corruption, and reordering rather than accepting only an aggregate checksum. Jitter and burst variants perturb schedules.

## Sanitizers

ASan+UBSan cover memory/undefined behavior. TSan is a separate lane for concurrent data-race detection. Sanitizer success is empirical evidence, not a proof of race freedom.

## libFuzzer

Three targets attack command-stream semantics, object lifetime, and rollover behavior. Any discovered reproducer belongs in `fuzz/corpus/` and is replayed in CI.

## Relacy

The Relacy mirror models the cursor/cache protocol with Relacy-tracked atomics and variables at capacities 1 and 2. It attacks weak-memory interleavings that ordinary x86 execution may not expose. It does not prove arbitrary `T` lifetime behavior.

## Linearizability

C++ records operation intervals using a sequentially consistent atomic logical event clock. The Go checker uses Porcupine and an independently implemented bounded FIFO state machine. Failed histories are preserved, shrunk, visualized, and promoted to regressions.

## Mutation effectiveness

A verifier that only accepts the correct queue is uncalibrated. The curated mutation campaign contains acquire/release weakenings, premature publication/reuse, mask corruption, and full-condition errors. `docs/VERIFIER_EFFECTIVENESS.md` records which layers kill which mutants. Unsupported claims remain explicitly unsupported until the campaign has actually produced evidence.
