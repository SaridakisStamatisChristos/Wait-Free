# Managed slot storage-location experiment

Prespecified before measurement, 10 October 2026. Evidence only; no merge,
production integration, tag, release or official scoreboard update is authorized.
The experiment starts from PR51 final source
`01206cbd73459e0463edf08e46d9a1adc179fa20`, tree
`c7e2e18b9685e8be7b63b278e2fd5786a7265ea2`. Main was reconciled at
PR30 `60ab1cedb6b79777549888d10fe4a51484f3b159`; PR50/51 remain closed unmerged.
No rejected policy is retried. Heap-managed slots are a control for a new
storage-location variable, not a new representation-only confirmation.

## Single changed variable and unavoidable consequences

`inline_dynslot` changes `raw_dynslot` from a separately allocated managed slot
buffer to a member slot array. Both use runtime physical capacity, runtime
`slot<T>*`, Capacity+1 wrapped cursors, split consumption, grouped 256-byte
controls, cached peers, progress-only cache stores and identical scalar methods.
Exact public usable capacity remains Capacity; the spare physical slot is never
available. Peer acquisition and cursor release publication remain intact.

The member array is default-initialized without `{}`. Slot/byte lifetimes begin
automatically without default-constructing T or zeroing payload storage. The
constructor sets the runtime pointer to that array, performs no buffer allocation
and is noexcept. Placement construction uses `storage_ptr()`; only live accesses
use `live_ptr()`. Destruction drains live T objects; member slot lifetimes end
automatically. No buffer deallocation or second slot destruction is performed.

The default heap specialization retains its allocation size/alignment, 768-byte
queue size and control offsets. The added empty member does not move its controls.
The inline specialization necessarily changes total object size, buffer/control
distance, outer allocation size/alignment and allocation topology. It is not an
identical-allocation experiment. The lab's queue owner is allocated uniformly by
`make_unique` for production and all three internal controls/candidates, so large
inline payloads do not exhaust the benchmark stack. Queue-constructor buffer
allocation is distinct from the outer benchmark owner allocation.

All twenty native layout cases have control offsets
`capacity=0, pointer=8, write=256, read_cache=264, read=512, write_cache=520`.
The inline slot array begins at 768 and its first usable payload at 1024; the
leading/trailing padding count matches the heap control. Inline object size ranges
from 1536 to 16,778,752 bytes. ARM GCC and Clang will independently validate and
emit these layouts; native evidence does not substitute for their artifacts.

## Verification and boundaries

The contract suite exercises split/fused inline paths at capacities
1/2/4/64/1024/65536, 100K-operation deque histories, exact-full rejection, unchanged
failed-pop output, throwing-copy rollback, leftovers cleanup and 200K concurrent
FIFO transfers. Repeated over-aligned, non-default-constructible nontrivial reuse
checks object counts and zero inline buffer allocations/deallocations with a
recording allocator. Compile-time checks require noexcept inline construction.

Local strict native GCC and the format check from the repository root passed.
ASan/UBSan passed with `ASAN_OPTIONS=detect_leaks=0`; unrestricted LeakSanitizer
fails locally because process inspection is unavailable. GitHub tooling runs the
same contracts under unrestricted ASan/UBSan. Clang is unavailable locally and
will be checked by GitHub and both measured ARM compiler lanes.

Standalone constructor/push/pop probes use the real sequence-plus-byte-array
payload. Actual comparator thread loops and every capacity/payload instantiation
will be inspected independently for GCC/Clang, including labels/branch targets,
runtime wrap/capacity loads, pointer addressing, caches, payload copies, registers
and acquire/release. Native instruction matches do not establish ARM performance
or explain measured allocation/placement effects without additional evidence.

The six standard gates run on this source. Production remains untouched, so
their production model coverage must not be described as a complete model of the
experimental storage path. The candidate has the explicit deque/lifetime/FIFO
coverage above. This copy-push/pop lab class is not the complete public API.

## Immutable paired protocol

One fresh ARM40 run: GCC and Clang; capacities 2/64/256/1024/65536; payloads
8/16/64/256; 1M transfers; five warmups; fifty randomized paired repetitions.
Exactly eight implementations in each round:
`veriqueue,raw_progress,raw_dynslot,inline_dynslot,rigtorp,boost_lockfree,moodycamel,drogalis`.
Every label is at most fifteen characters. No oracle or per-cell selector.

Primary comparison: **inline-managed / heap-managed**, fixed before measurement.
External and production comparisons are separate, retaining the existing fastest
external selection and median-ratio/bootstrap helpers. Raw progress is a forensic
control, never an external competitor. No run pooling or outcome-driven reruns.

Randomization seed: `2026101017 + compiler_id * 10000 + capacity`, with GCC id 1,
Clang id 2. Bootstrap seed 2026101018; 20,000 samples. Classification is unchanged:
WIN if lower 95% CI >1; LOSS if upper 95% CI <1; otherwise TIE. Exact unrounded
bounds decide classification. TIE is uncertainty, not equivalence.

Pinned externals: Rigtorp `59a6a938513ea5004817383711ed35d32385d3ee`, Moodycamel
`6867b56452352acf077fccd5f6cc7e3a8cfde0fb`, Drogalis
`c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96`, Boost 108300. Moodycamel requested
capacity 2 has three usable elements; keep and annotate those conservative
external comparisons. Internal comparisons remain capacity-equivalent.

Integrity requires ten shards, forty cells, 17,600 valid records including
warmups, complete eight-way rounds, exact FIFO/payload checksums, affinity,
transfer counts, immutable source, comparator pins and the declared randomized
chronology. A dedicated analyzer rejects corrupt/missing data before reporting
the primary diagnostic. Synthetic falsification fixtures are tooling tests only.

## Scoped decision rule

Evaluate each compiler independently. A broad positive storage-location mechanism
requires primary lane lower CI >1 and no primary per-cell LOSS. An aggregate LOSS
or a direct cell LOSS rejects a universal mechanism for that compiler. Otherwise
the mechanism remains unresolved; TIE does not establish safe equivalence.
Report external and production losses even if the primary mechanism is positive.
Do not apply a GCC finding to Clang, or infer production qualification from this
ARM40 forensic run. Full API integration and original80 qualification remain future
work. Record failed/superseded runs and correct actual defects without pooling.

Final measurements and assembly findings will be recorded in the PR result body
on the immutable measured head, leaving source/main unchanged during measurement.
