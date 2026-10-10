# PR43 result and PR44 dynamic raw owner-cache layout experiment

## Completed PR43 evidence — not promotable

Measured source: `fb2f6b9f39c92850187917e498b0aac4d1fa9e81`.
[Rigtorp Consumer Shape run 38050271005](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38050271005) completed successfully, including all eight GCC/Clang shards, both assembly jobs, contract checks and unrestricted ASan/UBSan. Summary artifact: **11669626497**. All six standard workflows on this source are green.

| Candidate | ARM geomean vs fastest external | 95% CI | W/T/L | GCC | Clang |
| --- | --- | --- | --- | --- | --- |
| dynamic_split | 0.9476× | [0.9049, 0.9858] | 1/19/4 | 0.9271× | 0.9685× |
| dynamic_raw | 0.9216× | [0.8753, 0.9634] | 0/18/6 | 0.8904× | 0.9539× |
| production control | 0.9351× | [0.8563, 1.0353] | 3/10/11 | 0.8824× | 0.9908× |

The split candidate's remaining GCC losses are 64×16, 256×8 and 256×16; Clang retains a 64×16 loss. Standalone GCC push/pop probes match upstream's corresponding instructions in all four targeted regimes after label normalization. Actual inlined producer loops also match upstream; consumer loops retain distinct captured queue-pointer reload behavior.

**Decision: reject production promotion.** Consumer shape alone does not establish GCC parity or remove the persistent 64×16 Drogalis deficit. This does not prove that consumer shape has zero performance effect.

The initial PR43 run 38049984738 had a harness defect: Clang's `-save-temps=obj` replayed expanded system macros under project warning flags and failed compilation. The repair emits assembly with a separate direct `-S` compilation, keeping normal benchmark compilation and its warnings unchanged. Initial partial-run results are not combined with the repaired campaign.

The official PR30 full 80-cell score remains **35 WIN / 27 TIE / 18 LOSS**. No production header changed and no merge occurred.

---


## Next isolated hypothesis

The pinned Drogalis header and its actual GCC 64x16 benchmark loops place the
writer's peer cache next to the writer publication (offsets 256/264) and the
reader's peer cache next to the reader publication (512/520).
PR42 dynamic_raw places these four fields on four separate 256-byte boundaries.

PR44 changes **only the alignment of the two private peer-cache fields**.
Runtime physical capacity, raw allocation, payload lifetime, storage padding,
producer/cursor logic and dynamic_raw's original fused consumer remain unchanged.
The class retains 256-byte producer/consumer separation, with private cache
fields adjacent to their owner cursor. Its aligned object shrinks from 1280 to
768 bytes for the tested standard allocator. It still has exactly Capacity
usable elements in Capacity+1 physical slots.

This tests coherence/locality geometry independently of PR43's consumer-shape
experiment. The candidate does not adopt Drogalis's persistent assignment,
default-constructed vector, cached padding/capacity members or different wrap
expression. No imported implementation or per-cell selector is introduced.

PR41 typed_dro_like had static physical capacity and persistent typed assignment,
with a different heap-buffer representation. This is a controlled change from
the dynamic raw forensic control identified by PR42, not a repetition of PR41.

Hypothesis to challenge: grouping owner-private caches can remove enough
working-set/locality overhead to improve broad GCC throughput without changing
semantic behavior. It may instead increase coherence traffic on peer-visible
cursor lines, so neither its acceptance nor direction of effect is assumed.

## Measurement and decision

Same-run controls are production and dynamic_raw. All four external comparators
remain in every randomized round. Both GCC and Clang run capacities
64/256/1024/65536 and payloads 8/16/64 at 1M transfers, 5 warmups and 50 paired
repetitions, with the unchanged bootstrap analyzer and classification rule.

The expanded standalone assembly includes dynamic_grouped. Actual inlined
comparator assembly is emitted directly in a separate compile, preserving the
normal comparator build and source-warning diagnostics.

Exact-capacity, deque-model, wrap, nontrivial lifetime, construction exception,
failed-pop output preservation and concurrent FIFO checks exercise the original,
split and grouped forms. ASan/UBSan checks run unrestricted in GitHub.
The existing production correctness/model/fuzz/sanitizer/portability/performance
gates remain mandatory; they do not by themselves qualify an experimental class
as a complete replacement for the production API.

Reject promotion unless this broad mechanism establishes GCC parity and clears
statistical losses. Any eventual compiler/architecture policy must separately
preserve proven x64 behavior and qualify a broad Clang mechanism. Integration
requires the full API, equivalent model coverage and a new original 80-cell
campaign before changing the official scoreboard.

Local strict GCC contract/model/lifetime/exception/concurrency checks, format,
expanded probe compilation and workflow-generator syntax validation passed.
Promotion verdict is pending PR44's fresh artifacts. This branch is evidence only.
