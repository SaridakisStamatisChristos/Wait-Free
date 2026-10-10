# PR48: direct peer refresh on the GCC raw combined forensic control

## PR47 full ARM result

PR47 source `ae4e81561539c1eb2e17295e224b881036b8e073`, run
[38056209636](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38056209636),
summary artifact **11671737596**, completed all 40 ARM cells with 50 paired
repetitions, 1M transfers and five warmups. Dedicated contract/sanitizer checks
and all six standard workflows passed. No production queue code changed.

| Candidate | ARM vs fastest external | 95% CI | W/T/L | GCC | Clang |
| --- | --- | --- | --- | --- | --- |
| raw_combined | 0.9948x | [0.9747, 1.0150] | 2/33/5 | 1.0153x | 0.9747x |
| raw_ctrl64 | 0.9899x | [0.9674, 1.0129] | 1/34/5 | 1.0004x | 0.9795x |
| dynamic_raw | 0.9387x | [0.9041, 0.9711] | 0/34/6 | 0.9308x | 0.9467x |
| production control | 0.9591x | [0.9056, 1.0192] | 4/27/9 | 0.9297x | 0.9895x |

PR46's partial zero-loss observation was not sustained. Both combined forms
retain five external losses in this fresh full ARM run. No runs were pooled.
Reject production promotion; do not replace Clang with either raw path.

The stronger full GCC control is raw_combined, with its original 256-byte
control distance: **1.0153x [0.9925, 1.0441], 0W/18T/2L** over 20 cells.
Against same-run GCC production it reaches **1.0996x [1.0343, 1.1848]**.
All 16 GCC cells at capacity >=64 are TIE versus fastest external in this run.
The remaining losses are both at exact capacity 2:

| Bytes | Fastest external | Ratio | 95% CI | Classification |
| --- | --- | --- | --- | --- |
| 64 | Moodycamel | 0.9614x | [0.9415, 0.9875] | LOSS |
| 256 | Boost.Lockfree | 0.9576x | [0.9329, 0.9845] | LOSS |

Keep the Moodycamel loss conservatively despite its three usable elements
versus our exact two. The Boost loss is not covered by that fairness annotation.
Combined also loses against GCC production at 2x8. Ctrl64 has an additional
GCC 65536x8 external loss. Its 2x256 upper CI is 0.999914, which remains LOSS
under the unchanged rule; it must not be rounded into a TIE.

An independent audit checked all 40 cells, 50 complete paired rounds, all eight
implementations, one exact source head, affinity/FIFO validity, and expected
counts/checksums for all 17,600 warmup/measured records. Between combined and
ctrl64, all 40 inlined producer/consumer loop pairs per compiler match after
only local-label and scalar control-displacement normalization. Stack and SIMD
payload offsets remain untouched by normalization. No PMU cause is claimed.

## One isolated next hypothesis

Inspected the
[Boost 1.83.0 scalar SPSC implementation](https://github.com/boostorg/lockfree/blob/boost-1.83.0/include/boost/lockfree/spsc_queue.hpp).
Its scalar push and consume-one paths acquire the peer cursor on every attempt;
they do not maintain a private cached peer cursor. The combined raw control
acquires only when its cached-peer equality indicates possible full/empty,
then writes the private cache. Those extra cache-hit tests and refresh stores
may be unhelpful in a tiny ring. Their actual traffic frequency and hardware
cost remain a hypothesis, not an established explanation of the Boost loss.

Append a CachePeer template flag, default true, to the shared raw class.
The only new measured alias, raw_direct, selects false on the 256-byte combined
control. Producer fullness and consumer emptiness each use a direct acquire
load of the peer cursor. Release publication remains unchanged. This increases
peer observation frequency; it does not weaken synchronization.

The split consumer shape, owner-cursor reload, runtime Capacity+1 physical
capacity, exact Capacity usable slots, allocator, raw pointer, buffer padding,
control/cache offsets, object size/alignment and payload lifetime are unchanged.
The two private cache fields remain present at the same offsets, initialized
as before but unused by the new path, preserving allocation/layout geometry.
Do not remove them during this experiment. There is no capacity cutoff, cell
selector, branch hint, prefetch, spin delay or workload change.

Prior PR10/22/29 lab definitions were checked for this mechanism; their
cached-limit, owner-cursor, storage and split-control ablations do not implement
direct refresh on this dynamic raw combined representation. This does not
revive a previously rejected selector or universal layout experiment.

## Measurement and decision

Measure the full **20-cell ARM64/GCC scope**: capacities 2/64/256/1024/65536,
payloads 8/16/64/256. Same-run candidates are production, dynamic_raw,
raw_combined and raw_direct, with all four pinned externals. Retain 1M transfers,
five warmups, 50 randomized paired repetitions and the unchanged analyzer.
Use a fresh prespecified seed and do not pool measurements with earlier runs.
All implementation labels remain within 15 characters.

Clang is not a performance-promotion target in this GCC experiment. Its proven
production path remains unchanged. Both ARM compilers emit full standalone
assembly and run the new contract suite; measured GCC shards also save actual
inlined thread-loop assembly. All six standard workflows remain required.
Exact-full/deque/wrap checks cover capacities 1/2/4/64/1024/65536. Lifetime,
construction exceptions, failed-pop preservation and concurrent FIFO checks
exercise direct refresh, including its unmeasured fused-consumer instantiation.
Assert equal new/control object size and alignment. GitHub runs unrestricted
ASan/UBSan as before.

Reject the mechanism as a broad policy if it fails to improve the remaining
exact-capacity deficit while preserving lane-wide parity and correctness.
Retain all losses, including unequal-capacity comparisons. Record the same-run
direct/combined diagnostic without a per-cell oracle. WIN means lower 95% CI > 1;
LOSS means upper 95% CI < 1; TIE means the interval crosses 1.

Even a favorable GCC result cannot authorize merging this class. Production's
default construction is noexcept with inline storage; this forensic class
allocates. Preserve that contract, complete move-only/nontrivial and bulk/
consume/advisory APIs, custom CacheLine/Index and compact configurations, and
equivalent weak-memory models in any subsequent integration. Then run all
gates and the original full 80-cell campaign before changing the official score.

Main remains PR30 at `60ab1cedb6b79777549888d10fe4a51484f3b159`.
Official full matrix remains **35 WIN / 27 TIE / 18 LOSS**,
**1.0497x [1.0007, 1.1015]**. This PR is evidence only and must not be merged.
