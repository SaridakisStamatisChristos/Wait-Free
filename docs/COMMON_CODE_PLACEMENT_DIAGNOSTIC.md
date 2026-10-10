# Common-code allocator placement diagnostic

Prespecified before measurement, 10 October 2026. Start from closed unmerged PR53
`194d6271f59bee4e5a80b54de945adf428437c48`, tree
`c1339674ed6c3eb8456f0f0596bf489c8b6863e9`. Main remains PR30
`60ab1cedb6b79777549888d10fe4a51484f3b159`. No merge, tag, release, production
integration, scoreboard update or capacity/payload selector.

PR53 rejected a universal 256-byte alignment policy: aligned/ordinary heap ARM40
0.972780 [0.930141,1.014626], 4W/28T/8L. Clang had five direct losses, GCC three.
The 64x16 ratios were Clang0.695156 LOSS and GCC0.603179 LOSS despite matching
scalar/thread instructions. Ordinary buffers had16-byte residues; aligned had0.
That intervention also used different queue types, lambda identities and linked
thread-function locations. Instruction equality did not isolate data placement
from code location. The rejected policy remains rejected.

## New question and scope

Does allocator/placement sensitivity persist when both modes use one queue type,
one lambda/callsite and the same linked producer/consumer functions? If a material
loss persists under verified common code, distinct hot-function ELF offsets are
not necessary for it in this diagnostic. This is not proof of cache-index aliasing
or the cause of PR53. If the loss is absent, PR53 is not invalidated: different code
placement, allocator state and runner/campaign environment remain possible factors.
No cross-campaign pooling or causal attribution from a TIE.

`runtime_buffer_allocator<T>` stores a one-byte immutable allocation mode. Rebind
copies it into the managed slot allocator. Both modes allocate exactly the existing
physical ring/padding count. Ordinary delegates to std::allocator; aligned delegates
to the checked max(256,alignof(T)) operator-new/delete allocator. Matching deallocation
uses the same state. The explicit queue constructor is new; the existing default
constructor and every push/pop/cache/lifetime method are unchanged.

The common object is768 bytes, aligned256, with unchanged offsets
capacity0/pointer8/write256/read_cache264/read512/write_cache520. The mode occupies
previously unused byte16; it is never read or written in push/pop. Runtime capacity
and pointer, Capacity+1 wrap, exact usable Capacity, split consumption, grouped
controls, progress-only peer caches, placement construction/live accesses, cleanup,
nullable front and acquire/release remain fixed. Slot default initialization begins
byte lifetimes without constructing T or zeroing payload storage.

Both `heap_common` and `align_common` enter one branch in the comparator, use one
make_unique queue type and one run_pair callsite with identical reference captures.
The original production benchmark source remains unchanged. Legacy `raw_dynslot`
is the ordinary-heap calibration control, not an external and not a retried policy.
The rejected separately compiled aligned/inline variants are omitted from timing.

Before any scored sample, ARM assembly must contain exactly40 shared thread functions
for20 common cases, with280 total functions from7 code callsites and8 runtime labels.
The linked nm symbols must resolve each shared function once. Modes share linked ELF
function offsets; separate-process ASLR is not controlled and no identical absolute
runtime code-address claim is made. Complete scalar and actual-loop comparisons,
registers, branches/targets, copies, controls and acquire/release are independently
audited against the legacy control and immutable PR53.

## Outside-timing geometry

Each internal sample observes buffer/first usable residues modulo64/128/256 and
`[owner,write,read,buffer,first]` residues modulo4096, unsigned buffer-owner distance,
and whether buffer follows owner. Values are read after construction, before
run_pair, and attached to the result after timing. Absolute addresses are not emitted.
4096 is an observation modulus, not a cache-set or physical-page claim. Validation
checks owner alignment, control offsets, padding, residue/distance consistency and
non-overlap using the requested buffer/object sizes. Ordinary buffers are not assumed
to be misaligned; record actual values. Aligned must have zero256 residues.
External/production samples mark geometry unobserved and retain zero placeholders.
The exact original timed thread work and timing boundary text is asserted unchanged.

## Correctness and preflight

Both runtime modes, split/fused paths, capacities1/2/4/64/1024/65536:100K deque
histories, exact-full/spare rejection, unchanged failed output, throwing-copy rollback,
leftovers cleanup and200K concurrent FIFO. Recording runtime wrappers verify rebind
state, one matching allocation/deallocation, original counts and1000 reuse cycles
with non-default nontrivial payloads aligned128/512. Allocator comparison reflects
mode; alignment overflow remains checked. Twenty layouts must pass on both compilers.

Native strict GCC, contracts, ASan/UBSan with local leak inspection disabled,20layouts,
shared ELF-function check and generated candidate-only FIFO/geometry smoke passed.
Synthetic integrity, counter and missing/duplicate-code falsification fixtures are
never benchmark/profiling evidence. Full comparator/Clang qualification runs on
GitHub. Unrestricted GitHub sanitizer and six standard gates are required. Those
production model gates do not constitute complete candidate/public API integration.

## Frozen ARM40 score protocol

GCC/Clang x capacities2/64/256/1024/65536 x payloads8/16/64/256, 1M transfers,
5 warmups,50 randomized paired rounds. Eight labels in declared pre-shuffle order:
`veriqueue,raw_dynslot,heap_common,align_common,rigtorp,boost_lockfree,moodycamel,drogalis`.
Primary **align_common/heap_common**. Prespecified calibration
**heap_common/raw_dynslot**, separately reported. External/production comparisons
retain the same fastest-external selection and bootstrap helpers. No oracle/selector.

Fresh seed2026101021 + compiler_id*10000 + capacity, GCC id1/Clang id2.
Bootstrap20,000, seed2026101022; calibration seed2026101022 XOR0x517CC1B7.
WIN lower95CI>1, LOSS upper95CI<1, elseTIE using exact bounds. TIE is uncertainty,
not equivalence. Evaluate compiler lanes independently. Broad mechanism requires
primary lane lowerCI>1 and zero direct LOSS; any cellLOSS rejects universal policy
in this diagnostic. Calibration gains/losses limit transferring results to the old
control. Report every primary/calibration/external/production cell regression.
No pooling, outcome-driven rerun, filtered cells, work reduction or statistical edits.

Pinned Rigtorp59a6a938513ea5004817383711ed35d32385d3ee, Moodycamel
6867b56452352acf077fccd5f6cc7e3a8cfde0fb, Drogalis
c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96, Boost108300. Moodycamel requested
capacity2 holds3 entries; internals remain exactly2. Conservative comparisons and
annotation are retained. Integrity requires10 shards,40 cells,17,600 valid records
(1,600 warmup/16,000 measured),2,200 complete eight-way rounds, immutable source,
checksum/FIFO, affinity, all pins, exact seeds/order/chronology and geometry.

## Separate PMU phase

After uploading scored raw records, install available perf packages and attempt
unprivileged process-level perf stat with fixed events cycles:u,instructions:u,
branches:u,branch-misses:u,cache-references:u,cache-misses:u. Never lower perf security
settings or rerun scoring for PMU access. Preserve package/probe failures. A10000-transfer
heap_common16-byte probe tests all six events, finite nonnegative counts and>=90%
running; cycles/instructions must be positive. No unsupported/not-counted zero filling,
no selection of event sets based on results. If unavailable, retain raw probe stdout,
stderr,CSV and reason, mark PMU unavailable and skip the optional counter schedule.

If supported: each shard's4 payloads x2 warmups+10 randomized rounds x3 labels
raw_dynslot/heap_common/align_common,1M transfers; seed2026101023 + compiler_id*10000
+capacity. Profiles are physically separate artifacts, never score inputs. Record
source,binary hash, rawCSV/stdout/stderr, counts, running fraction, sample order and
benchmark FIFO/checksum/affinity validity. Preserve invalid samples as partial.
No score classification or adjusted gate is computed from counters/profile rates.

Counts cover the entire user-mode process, including construction, thread startup,
join and JSON diagnostics, not queue-only instructions. Scaled/multiplexed counters
are descriptive. Partial/missing PMU evidence blocks cache-causal claims, not the
valid common-code comparison. Even complete PMU/geometry correlations do not by
themselves establish a physical cache-index mechanism. Report capability per shard.

## Disposition

Record source/run/artifact IDs, common-code proof, all losses, actual geometry and
PMU availability in the completed PR body. Close evidence-only unmerged. PR53 stays
closed with its verdict intact. Main/official original80 remain unchanged at
35WIN/27TIE/18LOSS,1.0497 [1.0007,1.1015]. A scoped mechanism finding cannot replace
full public API/model integration and original80 production qualification.
