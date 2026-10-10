# Runtime pointer to lifetime-managed slots

This is an evidence-only representation experiment. Production remains PR30
(`60ab1cedb6b79777549888d10fe4a51484f3b159`), with the official original
80-cell score of 35 WIN / 27 TIE / 18 LOSS and 1.0497x [1.0007, 1.1015].
Nothing in this experiment changes the production queue header or public API.

## Why this experiment follows PR50

PR49's raw progress-only cache-store mechanism improved its GCC forensic control
by 1.0268x [1.0064, 1.0521] versus `raw_combined` in that same run. It still
failed external parity: GCC 0.9493x [0.9244, 0.9725], 0W/13T/7L.

PR50 tested the scalar cache-store change on production in the full original
80-cell matrix, with a frozen PR30 queue in each randomized paired round.
All six normal gates passed, but its candidate was 1.0312x [0.9798, 1.0872]
versus fastest external, 32W/25T/23L. The same-run GCC candidate/PR30 ratio was
0.9864x [0.9663, 1.0050], with two direct LOSS cells (2x16 and 1024x16).
PR50 was closed unmerged. Historical PR30 and PR50 scores are not a paired
causal comparison. The frozen PR30 same-run external diagnostic was
1.0412x [0.9932, 1.0957], 33W/25T/22L; it does not replace the official score.

The raw discovery has therefore not transferred to production. Moving multiple
representation/layout/cursor properties together would obscure the cause.

## Single changed variable

`raw_dynslot` differs from the PR49 `raw_progress` control only in storage:
runtime `T*` becomes runtime `veriqueue::detail::slot<T>*`. Both retain:

- runtime physical capacity `Capacity + 1` and exact public usable capacity;
- the same wrapped cursor, split consumer and grouped 256-byte controls;
- the same acquire/release operations and progress-only cache updates;
- heap allocation with identical slot size, alignment and end padding;
- construction per successful push and destruction per successful pop;
- identical queue size/alignment for the standard allocator.

The slot allocator explicitly default-initializes byte-buffer lifetimes without
zero-initializing them or constructing T. Placement construction uses
`storage_ptr()`, while live access uses `live_ptr()`/`std::launder`. No T pointer
arithmetic traverses separate byte buffers in the managed path. Destruction
cleans queued T objects before releasing the slot array.

This is neither persistent typed-object assignment from PR41 nor the static
physical-capacity slot representation from PR42. It does not yet move storage
inline or implement a new production policy. The forensic queue still provides
only the copy-push/pop benchmark subset; complete move-only, emplace, consume,
bulk and advisory APIs remain prerequisites for any production integration.
The allocating forensic constructor also cannot replace PR30's noexcept inline
constructor without further isolated work.

## Prespecified evidence

The new campaign covers both ARM compilers, capacities 2/64/256/1024/65536 and
payloads 8/16/64/256: 40 cells. Eight implementations run in each randomized
paired round: production, original dynamic raw, progress raw, managed slots,
and the four pinned external queues. Each cell uses 1M transfers, five warmups,
50 repetitions and a fresh seed. Classification remains lower95 > 1 WIN,
upper95 < 1 LOSS, otherwise TIE; bootstrap sampling remains 20,000.

The managed/progress-raw ratio is a separate, prespecified representation
diagnostic. TIE does not prove equivalence. No oracle or per-cell selector is
promoted. The external report continues to select only among the four external
queues. The analyzer requires all ten shards, forty cells, complete paired
rounds, warmups, exact transfer counts, affinity, validity, source and pins.

GCC and Clang assembly includes all standalone scalar probes and constructors,
plus actual inlined benchmark loops. Probe payloads now match the benchmark's
sequence-plus-byte-array representation; original benchmark code is unchanged.
Important GCC regimes remain 64x16, 1024x8, 1024x16 and 1024x64.

Local strict x64 GCC correctness checks and ASan/UBSan passed. Leak detection was
disabled locally because of the execution environment; unrestricted GitHub
sanitizer checks remain required. All 60 managed/raw standalone constructor and
scalar probe functions had identical x64 GCC instructions. The 95 emitted raw
control functions checked against the immutable PR49 header were unchanged.
These are local codegen checks, not ARM performance evidence.

Moodycamel requested capacity 2 has three usable elements; VeriQueue remains
exactly two. Keep those conservative comparisons and annotate their mismatch.
Only a qualified production candidate's full original 80-cell campaign can
change the official scoreboard. This PR must remain unmerged.
