# Verification architecture

No single tool establishes correctness. Each layer targets a different failure class.

## Deterministic tests

Boundary behavior, FIFO semantics, capacity 1, full/empty transitions, lifetime accounting, move-only/non-default types, large alignment, queued-object destruction, constructor failure before publication, and the bulk/consume API contracts.

Bulk-specific deterministic tests cover partial batches, exact longest-prefix behavior without overlap, empty spans, capacity 1, mixed scalar/bulk transitions, callback consumption, reduced-width rollover, and exact destruction of retired queue objects.

## Accelerated rollover

The exact production template is instantiated with reduced-width indices. Tens/hundreds of thousands of operations force many modular wraps while preserving the capacity bound. PR13 extends this coverage to bulk push/pop as well as scalar operations.

## Independent sequential model

Generated operation streams execute against both VeriQueue and an independent `std::deque` bounded FIFO. Results, values, and occupancy are compared. The fuzz model mixes scalar push/pop, bulk push/pop, and callback consumption. An exhaustive small-state enumerator covers short scalar sequences for capacities 1 and 2.

## Native concurrency stress

Producer emits strictly increasing 64-bit IDs; consumer requires the exact next ID. This detects loss, duplication, corruption, and reordering rather than accepting only an aggregate checksum. Jitter and burst variants perturb schedules. PR13 adds concurrent variable-width bulk transfer stress and callback-consume stress.

## Sanitizers

ASan+UBSan cover memory/undefined behavior. TSan is a separate lane for concurrent data-race detection. Sanitizer success is empirical evidence, not a proof of race freedom.

## libFuzzer

The command-stream target attacks scalar/bulk/consume semantics against an independent deque model; the other targets focus on object lifetime and rollover. Any discovered reproducer belongs in `fuzz/corpus/` and is replayed in CI.

## Relacy

The Relacy mirror models the cursor/cache protocol with Relacy-tracked atomics and variables. Scalar and native bulk/consume oracles cover capacities 1, 2, and 4. The bulk mirror models one final release publication after a batch and refreshes cached peer cursors under the same insufficient-span rule as production. It attacks weak-memory interleavings that ordinary x86 execution may not expose. It does not prove arbitrary `T` lifetime behavior.

## Linearizability

C++ records operation intervals using a sequentially consistent atomic logical event clock. The Go checker uses Porcupine and an independently implemented bounded FIFO state machine. The campaign runs scalar and mixed scalar/bulk/consume histories across capacities 1, 2, 4, and 8 and multiple scheduling profiles.

For positive bulk operations, Porcupine checks that the operation is one legal atomic FIFO-prefix effect whose count cannot exceed the abstract bound. It intentionally does not require a positive concurrent batch to equal the abstract-state maximum at its chosen serialization point, because VeriQueue obtains its peer cursor observation before its final batch publication and an overlapping peer operation can change capacity between those events. Exact longest-prefix behavior without overlap is checked by deterministic tests instead. Zero-result operations must still serialize where no progress is available.

Failed histories are preserved, shrunk, and visualized. `UNKNOWN` is distinct from `PASS` and fails the campaign.

## Mutation effectiveness

A verifier that only accepts the correct queue is uncalibrated. The curated mutation campaign contains acquire/release weakenings, premature publication/reuse, mask corruption, full-condition errors, and PR13-specific bulk/consume faults. The PR13 catalogue contains 32 mutants, including bulk publication order/value, over-capacity acceptance, wrong-slot mapping, and consume publication/mapping faults. No mutation score is claimed until the corresponding GitHub Actions campaign has completed.

`docs/VERIFIER_EFFECTIVENESS.md` records which layers kill which mutants. Unsupported claims remain explicitly unsupported until the campaign has actually produced evidence.

## Benchmark correctness gates

Performance evidence is accepted only from runs that also validate exact transfer counts and FIFO sequence/content. The PR13 bulk microbenchmark compares scalar, batch-width 4/16/64, and callback-consume modes inside one GitHub-hosted run and marks every record with an explicit `valid` field. It is an API characterization tool, not a universal performance claim.
