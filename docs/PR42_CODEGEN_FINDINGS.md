# PR42 assembly findings and isolated consumer-shape experiment

## Evidence and scope

Production remains PR30 at `60ab1cedb6b79777549888d10fe4a51484f3b159`.
Official full 80-cell score: **35 WIN / 27 TIE / 18 LOSS**, overall
**1.0497x [1.0007, 1.1015]**. This evidence-only branch changes no production header.

Inspected PR42 source `3a9594d8caf25f61ff7b9cb5c6cdf20e2e3911e8`,
successful workflow run [38045494216](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38045494216),
complete GCC and Clang assembly archives (artifacts 11667009094 and 11667333568),
and summary/raw archives (artifact 11667298900). GCC is Ubuntu 13.3.0;
Clang is Ubuntu 18.1.3. All ten push/pop functions in each original assembly were inspected.

## What the assembly establishes

The original probe covers **only capacity 1024, payload 16**. It cannot establish
instruction sequences at capacity 64 or payload 8/64, nor the actual inlined benchmark loops.

| Path | GCC/AArch64 evidence | Consequence |
| --- | --- | --- |
| Production | Monotonic distance check; 16B stripe uses `ubfiz`, `ubfx`, `orr`; inline storage; controls at 0/64, caches at 8/72 | Different cursor, mapping, allocation and coherence geometry. Its throughput gap cannot be attributed solely to raw storage. |
| dynamic_raw push | Exact same instructions/registers as upstream push after label normalization; capacity at 0, pointer at 8, controls/caches at 256/512/768/1024 | No remaining producer instruction difference in this probe. |
| dynamic_raw pop | Keeps earlier relaxed read cursor through copy; GCC pairs capacity/pointer loads with `ldp`; wrap uses `add/cmp/csel` | Consumer source shape remains different. |
| Upstream pop | `front()` computes pointer and checks null; after copy `pop()` reloads the relaxed read cursor, then loads runtime capacity and publishes | Extra instructions do not prove slower execution; inlining, aliasing and scheduling must be examined. |
| static_raw and static_slot | Identical instruction sequences after register/label normalization; immediate wrap uses `cmp/csinc` | No placement-new, destructor or launder instruction penalty in the isolated trivial 16B probe. Runtime capacity changes instruction scheduling, not simply instruction count. |

All paths retain peer acquire `ldar` and release publication `stlr`.
Trivial payload lifetime operations emit no destructor calls; the original probes
contain no stack spills or vector unrolling. GCC copies 16B with `ldp/stp`;
Clang uses `ldr/str q0`.

**Clang layout is not matched to upstream.** Local candidates hard-code a 256-byte
span, while pinned upstream uses its 64-byte fallback when the hardware-interference
library feature is absent. Clang upstream control/cache offsets are 64/128/192/256
and its leading storage padding is 64 bytes. The local candidates use
256/512/768/1024 and 256-byte leading padding. This is a concrete confound for
Clang equivalence claims; it does not by itself prove why a benchmark regresses.

## Remaining GCC cells from PR42

Ratios and classifications below are the downloaded paired summary, not new measurements.

| Capacity | Bytes | Fastest external | dynamic_raw ratio | Classification |
| --- | --- | --- | --- | --- |
| 64 | 16 | Drogalis | 0.6894 | LOSS |
| 1024 | 8 | Rigtorp | 0.7665 | LOSS |
| 1024 | 16 | Drogalis | 0.8891 | LOSS |
| 1024 | 64 | Rigtorp | 0.9314 | LOSS |

Rigtorp equivalence alone cannot explain all fastest-external blockers.
The detailed cause of these regime-dependent losses remains unproven.

## One controlled experiment: dynamic_split

Hypothesis: splitting the consumer internally into `front()` and `pop_front()`,
including the non-null check and owner-cursor reload, can change GCC's inlined
aliasing/scheduling in the same way as upstream's consumer adapter.

The sole candidate difference from `dynamic_raw` is consumer source/API shape.
A template boolean selects the variant. Layout, allocator, runtime physical
capacity, producer path, storage padding, exact usable capacity and acquire/release
protocol are unchanged. This is one broad mechanism across all capacities/payloads;
no cell selector or architecture promotion is added. No upstream code is vendored.

The expanded probe covers 64/256/1024/65536 x 8/16/64 for production, dynamic_raw,
dynamic_split, static_raw, static_slot and upstream on GCC and Clang.
The actual comparator compilation additionally saves its inlined assembly so the
wrapper results can be checked against the measured thread loops.

Contract checks cover exact-full rejection (including capacity 1), randomized
deque-model comparison, repeated wrapping, failed-pop output preservation,
nontrivial lifetime/destructor cleanup, construction exceptions, and concurrent FIFO.
They run on both measured compilers/architectures; tooling also runs ASan/UBSan.
These are experiment checks, not a claim that the evidence-only class implements
the complete production API or has completed production qualification.

Measurement retains the original comparator harness and all four pinned externals:
24 ARM cells, 1M transfers, 5 warmups, 50 randomized paired repetitions.
Classification remains WIN iff lower 95% CI > 1; LOSS iff upper 95% CI < 1; TIE otherwise.
Static variants are probed but not remeasured in this consumer experiment.
The production control and dynamic_raw are same-run forensic controls.

Reject the hypothesis if the paired evidence fails to support a broad benefit
or leaves the GCC blocker pattern effectively unchanged. Even a favorable ARM result
does not authorize integration: complete API/model coverage and the original full
80-cell campaign remain required. Keep x64 on PR30 and qualify Clang separately.
The two capacity-2 Moodycamel fairness annotations remain applicable to final reporting.

## Local validation before GitHub execution

- Strict GCC C++20 contract build and execution passed.
- Expanded probe compiled at O3; format and workflow-generator syntax passed.
- ASan/UBSan execution passed with local leak detection disabled because this
  container cannot inspect /proc tasks. The GitHub experiment does not disable
  leak detection and must supply the unrestricted sanitizer result.
- Promotion decision: pending new 50-repetition artifacts; no production change.
