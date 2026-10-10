# Managed heap buffer-alignment experiment

Prespecified before measurement, 10 October 2026. Evidence only: main remains
PR30 `60ab1cedb6b79777549888d10fe4a51484f3b159`. Start from immutable PR52
`0a54252d2265cf29181d6d33adcb3f99c21b7b7d`, tree
`6ea33e1aaa2f817f57e233f5b70869285db67fe6`. PR52 is closed unmerged:
inline/heap ARM40 1.028348 [0.992323,1.076795], 5W/33T/2L. Clang's aggregate
storage-location gain has a cell regression; GCC has no established gain.
Neither universal inline policy survived. Do not repeat or pool that experiment.
PR36 used ordinary heap allocation and different controls/padding; it did not
isolate guaranteed 256-byte buffer alignment under this fixed protocol.

## Hypothesis and minimal change

A guaranteed 256-byte heap-buffer alignment may improve the managed heap path
without moving the buffer inline. `aligned_dynslot` changes only its stateless
storage allocator: aligned operator new/delete use max(256, alignof(T)). The
allocator checks element-count multiplication overflow before allocating. Rebind
to slot<T> retains T's size/alignment. The queue requests the same physical ring
and leading/trailing padding counts; it starts slot lifetimes by default
initialization, without default-constructing T or zeroing the payload buffer.

Runtime capacity/pointer, exact public usable Capacity, Capacity+1 wrapped
cursors, split consumer, grouped 256-byte controls, peer caches, progress-only
cache stores, nullable front, placement construction, live access/destruction and
acquire/release operations stay fixed. The object remains 768 bytes aligned 256
with control offsets 0/8/256/264/512/520 for every measured payload/capacity.
No padding, batching, prefetch, public API or production policy is added.

The allocator may change placement and allocation service behavior; construction
and deallocation are outside timing. Guaranteeing alignment is not proof that
alignment causes a throughput change. Record base and first usable slot offsets
modulo 64/128/256 for each internal sample, immediately after construction and
before starting run_pair. No address observation occurs in timed loops. Metadata
is attached after timing. External/production entries explicitly mark observation
false, with zero placeholders, rather than asserting their alignment. Inline is
retained as a location control; it is not a proposed universal policy.

All internal queues use make_unique with identical reference captures. The
original benchmark and production headers stay unchanged; a dedicated generator
adds candidates and outside-timing result metadata. Whole producer/consumer
assembly will be audited across all twenty ARM cases for each compiler. Native
GCC's forty standalone candidate/control scalar bodies match; this does not
substitute for ARM artifacts or establish causality.

## Correctness and coverage limits

The existing deque/FIFO/lifetime/exception tests now also use the actual aligned
allocator for split and fused paths: capacities 1/2/4/64/1024/65536, 100K deque
operations, exact-full/spare rejection, unchanged failed output, copy rollback,
leftovers cleanup, and 200K concurrent FIFO. Recording wrappers verify one buffer
allocation/deallocation, unchanged requested element/byte counts, no persistent
T construction, 1000 nontrivial reuse cycles and alignment for non-default types
requiring 128 or 512 bytes. Arithmetic overflow must throw bad_array_new_length.
The twenty layout probes assert identical heap/candidate object geometry and
zero buffer alignment residues. Both measured compilers run the contracts.

Native strict GCC, formatting, synthetic integrity tests and ASan/UBSan passed.
Local sanitizers disable leak inspection because process inspection is unavailable;
GitHub runs unrestricted ASan/UBSan. A native candidate-only generated-source
smoke verifies JSON observations and FIFO; missing local external dependencies
mean the full comparator and Clang are verified on GitHub. Smoke rates are never
campaign evidence. The six standard gates cover unchanged production, not a
complete newly integrated model of this experimental copy-push/pop class.

## Frozen statistical protocol

One fresh ARM40 campaign: GCC/Clang, capacities 2/64/256/1024/65536, payloads
8/16/64/256, 1M transfers, five warmups, fifty randomized paired repetitions.
Eight implementations, in fixed declared order before per-round randomization:
`veriqueue,raw_dynslot,aligned_dynslot,inline_dynslot,rigtorp,boost_lockfree,moodycamel,drogalis`.
Labels are at most fifteen characters. Primary: **aligned_dynslot/raw_dynslot**.
External and production comparisons are separately reported using unchanged
median-paired-ratio and bootstrap helpers. Inline comparisons remain forensic.

Fresh seed `2026101019 + compiler_id * 10000 + capacity`, GCC id1, Clang id2.
Bootstrap seed 2026101020, 20,000 samples. WIN iff lower95CI >1; LOSS iff
upper95CI <1; otherwise TIE. Exact unrounded bounds decide; TIE is unresolved,
not equivalence. No pooling, outcome-driven rerun, cherry-picking, selector or
change to statistical rules. Preserve all conservative capacity comparisons.

Pinned Rigtorp `59a6a938513ea5004817383711ed35d32385d3ee`, Moodycamel
`6867b56452352acf077fccd5f6cc7e3a8cfde0fb`, Drogalis
`c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96`, Boost108300. Moodycamel requested
capacity2 has three usable slots; internals remain exactly two. Keep annotations.
Integrity requires ten shards, forty cells, 17,600 valid records including 1,600
warmup/16,000 measured records, 2,200 complete eight-way rounds, correct checksums,
FIFO, affinity, pins, frozen source and declared random chronology. Alignment
observations must be consistent, and aligned/inline samples must have zero
residues. Synthetic corruption fixtures are tooling checks, never evidence.

Evaluate compilers independently. A broad positive mechanism requires primary
lane lowerCI >1 and zero direct cell LOSS. Any direct cell LOSS or aggregate LOSS
rejects a universal alignment policy for that compiler; otherwise unresolved.
Report all candidate external/production losses even if the mechanism passes.
No conclusion transfers between compilers; ARM40 does not update official80.

Record final immutable source/run/artifact IDs, audited assembly, provenance,
losses and scoped verdict in the PR result body. Failed/superseded runs must be
recorded, never pooled. Full public API/model integration and original80 remain
mandatory before production qualification. No merge, tag or release.
