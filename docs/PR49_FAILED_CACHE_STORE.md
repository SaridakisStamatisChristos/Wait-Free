# PR49: retain caching, omit redundant stores on failed attempts

## PR48 is rejected and closed unmerged

PR48 source `31066d4f043d366f90822303ae5c5b7ec8ff3baf`, run
[38057444117](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38057444117),
summary artifact **11671174981**, completed all 20 GCC cells at 50 paired
repetitions. All six standard gates, both ARM compiler contract/assembly jobs
and unrestricted ASan/UBSan passed. The new direct-refresh policy failed:

| Candidate | GCC vs fastest external | 95% CI | W/T/L | vs production | 95% CI |
| --- | --- | --- | --- | --- | --- |
| raw_combined | 0.9985x | [0.9747, 1.0231] | 1/17/2 | 1.1008x | [1.0361, 1.1824] |
| dynamic_raw | 0.9310x | [0.8900, 0.9679] | 0/14/6 | 1.0136x | [0.9547, 1.0800] |
| production control | 0.9143x | [0.8627, 0.9598] | 1/11/8 | 1.0000x | [1.0000, 1.0000] |
| raw_direct | 0.4211x | [0.3212, 0.5502] | 0/0/20 | 0.4663x | [0.3694, 0.5871] |

Reject direct peer acquisition as a broad policy. Every external cell is LOSS.
Even the post-hoc direct/combined comparison at GCC 2x256 is only
1.0276x [0.9945, 1.0541], TIE, using the unchanged bootstrap helpers.
No cache-free capacity cutoff or per-cell selector is justified.

The cached combined control retains a supported broad production gain on GCC,
1.1008x [1.0361, 1.1824], consistent with PR47's positive GCC gain. Its latest
external losses are 2x256 versus Boost (0.9363x [0.9046, 0.9468]) and
64x64 versus Drogalis (0.9753x [0.9485, 0.9964]). PR47's zero losses at
capacities >=64 is therefore not stable across confirmation runs. Do not pool
these runs, revise classifications, or promote a cell lookup.

The full integrity audit verified 20 cells, 50 complete paired rounds, all eight
implementations, one exact source head, expected counts/checksums and valid
affinity/FIFO flags in all 8,800 warmup/measured records. The existing combined
control's 24 standalone GCC functions shared with PR46 remain instruction-
equivalent after label normalization. Direct refresh retains acquire/release
but increases peer acquire frequency; the experiment shows that its reduced
branch/store count does not make it a competitive broad mechanism.

## A smaller, independent codegen issue

The cached slow paths assign the acquired peer to the private cache before
checking whether an attempt failed. GCC emits a private-cache str before the
failure comparison/branch, including at 2x256 and 1024x16. Those failure stores
write a value that is provably unchanged:

- Producer enters refresh when next_write == old_read_cache. Failure means
  next_write == observed_read. Thus observed_read == old_read_cache.
- Consumer enters refresh when read == old_write_cache. Failure means
  read == observed_write. Thus observed_write == old_write_cache.

Private cache values have one owner and are not synchronization publications.
Omitting their equal-value failure stores leaves the operation result, output,
payload lifetime and complete cursor/cache value state unchanged. Successful
refreshes still update the cache before payload work; construction exceptions
therefore retain the same cache state as the control. Peer acquire and release
publication events are retained at the same logical points. This is a value-
state equivalence argument, not a replacement for production model qualification.

In the owner-grouped layout, these private caches share their owner's published
cursor line. An unnecessary write during failed polling might disturb a peer's
read copy of that line. Actual coherence cost and polling frequencies are not
established by assembly alone; no hardware-counter explanation is claimed.

## One controlled change

Add a CacheOnProgress boolean, default false. Only raw_progress selects true
on the cached, split-consumer, owner-grouped, 256-byte combined control.
The acquired peer is compared to the full/empty boundary first; failure returns
before cache assignment. Cache assignment remains on a successful refresh.

Retain every cache-hit optimization and the same peer-acquire frequency.
No direct-refresh policy is adopted. The control/cache offsets, runtime
physical capacity, exact usable capacity, allocator, raw pointer, padding,
object size/alignment, owner reload, wrap logic, lifetime and release
publication remain unchanged. No hint, prefetch, delay, capacity threshold,
lookup selector, new synchronization or benchmark workload is added.
Prior default instantiations keep their original behavior. Assert equal object
size/alignment between raw_progress and raw_combined.

## Fresh full ARM evidence

Measure all **40 original ARM cells**, capacities 2/64/256/1024/65536 and
payloads 8/16/64/256, on GCC and Clang separately. Same-run candidates are
production, dynamic_raw, raw_combined and raw_progress; retain all four pinned
externals, 1M transfers, five warmups and 50 randomized paired repetitions.
Use a fresh prespecified seed; do not pool earlier measurements. The rejected
direct policy is not remeasured for promotion. All labels remain <=15 characters.

Expanded probes and actual inlined thread-loop assembly must show that failure
stores are omitted while peer acquire/release sequences remain intact.
Both measured ARM compilers run exact-capacity/deque/wrap checks at capacities
1/2/4/64/1024/65536, plus failed-pop preservation, nontrivial lifetime,
throwing-construction rollback and concurrent FIFO. The unmeasured fused form
receives the same checks. GitHub runs unrestricted ASan/UBSan; all six standard
workflows remain required. Assess direct progress/combined paired diagnostics
without choosing a different algorithm for each cell.

Reject broad promotion if omitting those stores fails to establish useful broad
improvement and external parity with correctness retained. Qualify Clang
separately; do not sacrifice its production path. Keep all capacity-2 losses
conservatively and annotate Moodycamel's unequal usable capacity where relevant.
Keep WIN = lower 95% CI > 1, LOSS = upper 95% CI < 1, TIE = CI crosses 1.

These allocating raw classes still lack production's noexcept inline constructor,
complete API/custom configuration coverage and equivalent weak-memory models.
This optimization may later be transferable to the production representation,
but no such transfer is made here and its benefit cannot be assumed. Integration
still requires full API/model tests, all gates and the original 80-cell campaign.

Main remains PR30 at `60ab1cedb6b79777549888d10fe4a51484f3b159`.
Official score remains **35 WIN / 27 TIE / 18 LOSS**,
**1.0497x [1.0007, 1.1015]**. This evidence-only PR must not be merged.
