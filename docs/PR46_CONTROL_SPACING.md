# PR46: isolated control spacing on the dynamic raw combined path

## Completed PR45 decision

PR45 source `0425ec804b90598acebf7b4a6cbbff0fe9d62909`, corrected run
[38051343850](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38051343850),
summary artifact **11669229048**, measured the 2x2 split-consumer / grouped-cache
factorial with 50 randomized paired repetitions, 1M transfers and five warmups.
All dedicated jobs and all six standard qualification workflows succeeded.

| Candidate | ARM versus fastest external | 95% CI | W/T/L | GCC | Clang |
| --- | --- | --- | --- | --- | --- |
| raw_combined | 0.9790x | [0.9508, 1.0057] | 2/20/2 | 0.9895x | 0.9686x |
| dynamic_grouped | 0.9711x | [0.9561, 0.9863] | 0/22/2 | 0.9648x | 0.9775x |
| dynamic_split | 0.9500x | [0.9083, 0.9841] | 0/20/4 | 0.9593x | 0.9409x |
| dynamic_raw | 0.9308x | [0.8903, 0.9634] | 0/19/5 | 0.9154x | 0.9464x |
| production control | 0.8742x | [0.8001, 0.9570] | 3/11/10 | 0.8376x | 0.9125x |

Combined/production is 1.1368x [1.0296, 1.2519] over these ARM cells.
Remaining combined external losses are GCC 1024x64 versus Rigtorp,
0.9492x [0.8912, 0.9981], and GCC 65536x16 versus Rigtorp,
0.8265x [0.6861, 0.9358]. Original GCC 64x16 and 1024x8/16 blockers
are TIE in this run. Combined also significantly regresses against production
in three Clang 64-byte payload cells: capacities 256, 1024 and 65536.

Reject production promotion: both compiler lanes have point estimates below
parity, two losses remain, and the overall lower bound is below 1. These classes
do not implement the complete production API or equivalent weak-memory models.
Standard gates qualify unchanged production; experimental tests cover only
their stated contract scope. No full 80-cell campaign or integration occurred.

PR45's initial run used a 16-character identifier that induced an extra string
allocation before queue construction. The corrected run uses raw_combined,
within the 15-character inline-string boundary; initial results are superseded.
PR44's initial short unchanged-production regression-gate failure and targeted
successful repeat remain recorded in its PR body and the PR45 confirmation doc.

## One next controlled experiment

Clang's pinned upstream Rigtorp uses 64-byte interference spacing, while our
raw paths use 256 bytes. That mismatch is established by assembly, although it
does not establish the hardware cause of a throughput gap. Previous production
spacing and four-line experiments do not isolate spacing on this dynamic raw,
split-consumer, owner-grouped path. This is not a revival of their policies.

Add a ControlSpan parameter, default 256, to the shared dynamic raw class.
The only measured new alias, raw_ctrl64, uses the combined path with its reader
control placed 64 bytes after its writer control instead of 256. Writer/control
metadata offsets are retained; the reader and its private cache move together.

| Property | raw_combined | raw_ctrl64 |
| --- | --- | --- |
| Runtime physical capacity | Capacity+1 | Capacity+1 |
| Exact usable capacity | Capacity | Capacity |
| Leading/trailing buffer padding | 256 bytes rounded to whole T slots | Same |
| Writer / private cache offsets | 256 / 264 | 256 / 264 |
| Reader / private cache offsets | 512 / 520 | 320 / 328 |
| Standard-allocator object size / alignment | 768 / 256 | 768 / 256 |
| Consumer | Split front/pop | Same |
| Peer synchronization | Acquire / release | Same |

The shorter-spacing class reserves 256 trailing object bytes to keep object
size, alignment and allocation class equal to the control. They are not usable
slots, are not initialized or touched by queue operations, and are not a
capacity increase. This diagnostic reservation is explicitly part of the
experimental layout; removing it during integration would require fresh evidence.
The control span affects no buffer-padding, allocation-count or cursor logic.

Measure both compilers, but assess them separately. GCC results may refute any
universal mechanism; a Clang gain does not authorize changing GCC. Retain
production, dynamic_raw and raw_combined as same-run controls and all four
pinned externals. Keep the original comparator workload and analyzer unchanged.
All measured identifiers remain within 15 characters. No per-cell selector is
introduced. Assembly includes standalone probes and actual measured thread loops.

## Evidence gates and stopping rule

Use capacities 64/256/1024/65536, payloads 8/16/64, 1M transfers, five warmups
and 50 randomized paired repetitions. WIN means lower 95% CI > 1; LOSS means
upper 95% CI < 1; TIE means the interval crosses 1. Use same-run direct
candidate/control diagnostics to assess the spacing hypothesis without pooling
independent runs. A Clang mechanism must improve its lane broadly and retain
correctness; otherwise reject it and record the evidence. Even a favorable
partial result cannot change the official score or authorize a merge.

Extend exact-full, deque-model, wrap, failed-pop preservation, nontrivial
lifetime, throwing-construction rollback and concurrent FIFO checks to the
new layout. Assert equal standard-allocator size and alignment. GitHub runs
these on both measured compilers and ARM, plus unrestricted ASan/UBSan.
All six standard workflows remain required. Full API coverage, equivalent
weak-memory models and the original full 80-cell campaign remain mandatory
before any production promotion. Preserve the PR30 x64 path throughout.

Local strict GCC contract checks, expanded probe compilation, formatting,
workflow YAML and generator syntax passed. Local ASan/UBSan must be recorded
separately from GitHub leak checking; this container requires leak detection
disabled because it cannot inspect /proc tasks. Local x64 probe differences are
only reader/cache displacement changes; ARM assembly remains to be verified.

Main remains PR30 at `60ab1cedb6b79777549888d10fe4a51484f3b159`.
Official full 80-cell score remains **35 WIN / 27 TIE / 18 LOSS**,
**1.0497x [1.0007, 1.1015]**. Retain the two x64/GCC capacity-2 Moodycamel
losses conservatively and annotate their unequal usable capacity in full reporting.
This branch and PR are evidence only and must not be merged.
