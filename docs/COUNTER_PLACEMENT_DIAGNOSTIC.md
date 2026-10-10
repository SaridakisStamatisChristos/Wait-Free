# Full80 bottleneck diagnosis: benchmark progress-counter placement

Prespecified before measurement, 10 October 2026. Production main remains PR30
60ab1cedb6b79777549888d10fe4a51484f3b159. PR54 remains closed unmerged; its
shared-code alignment losses and missing PMU evidence remain valid. This diagnostic
is a different intervention in benchmark observers, not a repeated queue policy.

The user targets full statistical WIN everywhere. That is stronger than zero LOSS.
WIN means lower95CI>1, LOSS upper95CI<1, otherwise TIE, exact bounds. No target
waiver, rounding, resampling until green, or TIE-as-equivalence. Full WIN is a target,
not an assured outcome or a claim that one bottleneck explains every cell.

## Evidence and new question

The original comparator writes produced and consumed atomics after every successful
transfer. Both worker loops read failed on every iteration. Compilers choose their
stack placement; source adjacency alone is not proof of cache-line sharing. PR54
changed queue data placement while leaving these observer stores intact. Its direct
64x16 alignment losses persisted with common queue code, but PMU access was denied.

Question: does moving only the consumer progress counter off the producer counter's
line materially change throughput and/or external-relative queue rankings? All queues
have this workload. It might constrain throughput or amplify layout sensitivity.
This is harness diagnosis, not permission to remove benchmark work or change scoring.

Three arms for each of production VeriQueue and the four unchanged pinned externals:

| Arm | Counter placement | Code relationship |
|---|---|---|
| original | Original stack declarations | Calibration; same timed source text, different ELF functions from shared arms |
| packed | Two counters at byte0/8 in an aligned256,512-byte byte-buffer | Shared worker functions with split |
| split | Two counters at byte0/256 in that same storage type | Only consumed counter's address changes |

Explicit construct_at/destroy_at starts/ends atomic lifetimes. Both shared arms have
the same type, storage size/alignment and number of per-transfer stores. Queue types,
allocation, exact usable capacity, acquire/release, publication frequency, payload
generation, FIFO validation, checksum, failure polling, affinity and timing boundaries
remain fixed. No counters are disabled, delayed, sampled or replaced with local values.
The complete timed worker/join text is asserted equal to the original comparator.
Production spsc_queue.hpp, slot.hpp and bench_compare.cpp stay byte-identical.

Each queue has one shared runtime-selected callsite for packed/split. Before samples,
the full compiler output must have400 worker functions:200 original and200 shared
for20capacity/payload cases ×5queue callsites ×2owners. Linked nm symbols resolve
each once. Both shared layouts therefore have identical linked hot-function offsets;
separate-process absolute ASLR locations are not controlled. Original/shared calibration
also changes compiler frame/layout/code location and is not a clean one-variable
causal comparison. Report its cells separately; never pool them with the primary.

Outside timing, emit unsigned residues modulo4096 for produced,consumed,failed,start,
ready and unsigned distances/directions from produced to consumed and failed.
Validate common counter alignment/8-versus256 distance, consistent residues and flag
non-overlap. Absolute addresses are not emitted. 4096 is an observation modulus, not
a physical cache-index or hardware page-size assertion. Report actual original
counter/control line relationships; no source-only cache claim.

## Frozen full80 protocol

Two architectures x64/arm64 × GCC/Clang × capacities2/64/256/1024/65536 ×
payload8/16/64/256, 1M transfers,5warmups,50randomized paired rounds.
Declared pre-shuffle label order: original five (veriqueue,rigtorp,boost_lockfree,
moodycamel,drogalis), then packed/ of each, then split/ of each. No selector.
Seed2026101041 + architecture_id*100000 + compiler_id*10000 + capacity; x64id0,
arm64id1,GCCid1,Clangid2. Bootstrap20,000 seed2026101042 with fixed queue/cell/lane
salts. Primary per queue split/packed; calibration per queue packed/original.
Use the existing median paired-ratio and geometric-mean bootstrap helpers without
changes. Separately compute production/fastest-external for each harness using the
unchanged external selection rules; never mix external modes. All observations are
one campaign family. Local reanalysis is not independent replication.

Require20shards,80cells,66,000records (6,000warmup/60,000measured),4,400complete
fifteen-way rounds. Verify full source/pins/FIFO/checksum/affinity/random chronology,
finite positive rates, geometry and shared ELF worker matrix. Standard six gates
must pass. Synthetic fixtures and local smoke are never scored evidence.

Comparator pins remain Rigtorp59a6a938513ea5004817383711ed35d32385d3ee,
Moodycamel6867b56452352acf077fccd5f6cc7e3a8cfde0fb,
Drogalisc959bd4f7204dd73c4e75a2b00c3459c3a5e9a96,Boost108300.
Moodycamel requestedcapacity2 still holds3; VeriQueue remains exactly2. Keep the
conservative losses and annotate the mismatch; no spare-capacity workaround.

## Decision and limits

A broad production observer bottleneck requires primary lower95CI>1 and zero direct
LOSS in every architecture/compiler lane. Report all queues, all primary/calibration
losses and every production/external cell; any mixed result narrows the conclusion.
If packed and split are indistinguishable, TIE is unresolved, not no observer effect.
If the intervention matters, distinguish absolute throughput from changed queue
ranking. Common improvements across externals need not reduce VeriQueue's relative
gap. No PMU evidence exists from hosted runners; a measured address-placement effect
does not establish a specific physical coherence/cache-set mechanism.

This PR is evidence-only and closes unmerged. It cannot update the official PR30
original80 score (35W/27T/18L,1.0497[1.0007,1.1015]), excuse queue regressions, change
benchmark policy or qualify a production optimization. New queue changes require
controlled comparisons on the original frozen harness and full API/model/lifetime
qualification. No merge, tag or release. Preserve rejected PR31–54 decisions.
