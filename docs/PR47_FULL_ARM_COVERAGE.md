# PR47: full ARM coverage of unchanged raw mechanisms

## PR46 result and limits

PR46 source `aa4bd5b1a80bede33c0a1e9b74b630398322639a`, run
[38055636473](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38055636473),
summary artifact **11671905351**, completed all 50 randomized paired repetitions
and all six standard gates. Dedicated contract checks and unrestricted
ASan/UBSan passed. The production header remained unchanged.

| Candidate | ARM vs fastest external | 95% CI | W/T/L | GCC | Clang |
| --- | --- | --- | --- | --- | --- |
| raw_ctrl64 | 0.9993x | [0.9806, 1.0206] | 0/24/0 | 1.0058x | 0.9928x |
| raw_combined | 0.9826x | [0.9637, 1.0019] | 0/23/1 | 0.9929x | 0.9723x |
| dynamic_raw | 0.9305x | [0.8819, 0.9715] | 0/18/6 | 0.9349x | 0.9261x |
| production control | 0.9104x | [0.8332, 1.0124] | 2/10/12 | 0.8628x | 0.9607x |

The new raw_ctrl64 GCC/production geomean was
1.1788x [1.0628, 1.3189]. Its external GCC interval was [0.9768, 1.0400];
Clang's external interval was [0.9704, 1.0155]. Zero losses in this partial
matrix does not prove robust parity or superiority: all 24 comparisons are TIE.

Post-hoc direct paired spacing diagnostics, using the unchanged bootstrap
helpers, gave ctrl64/combined 1.0093x [0.9834, 1.0385] on GCC and
1.0068x [0.9870, 1.0283] on Clang, both TIE. All per-cell direct spacing
diagnostics were also TIE. A broad spacing benefit is not established.
We cannot attribute loss-count changes to spacing from these results alone.

Clang raw_ctrl64 retained significant losses versus production at
1024x64 (0.6428x [0.6015, 0.6836]) and
65536x64 (0.4856x [0.4458, 0.5332]). Do not use it as a universal ARM path
or sacrifice the production Clang path for a GCC gain.

All 24 standalone push/pop pairs per compiler match combined after local-label
normalization and reader/cache displacement substitutions, including branch
topology. Actual inlined producer/consumer loops match except displacements at
64x16, 1024x8/16/64 and 65536x16 on both GCC and Clang. Atomic acquire/release
instructions are retained. This isolates layout; no PMU cause is established.

Reject production promotion. Retain these as evidence and forensic controls.

## Why this fresh measurement is necessary

The original PR42 scope omitted capacity 2 and payload 256. Those omissions
exclude 16 ARM cells from the original full campaign and could hide small-ring
contention or large-payload regressions. PR46's favorable partial result warrants
checking those missing regimes before writing a production integration.

PR47 changes no queue code, storage policy, synchronization or benchmark workload.
It expands the workflow to the original five capacities (2/64/256/1024/65536)
and four payload sizes (8/16/64/256), across both ARM compilers: **40 cells**.
Raw_ctrl64, raw_combined, dynamic_raw and production are same-run candidates;
Rigtorp, Boost.Lockfree, Moodycamel and Drogalis remain pinned external controls.
Use 1M transfers, five warmups and 50 randomized paired repetitions with a fresh
prespecified seed. Do not pool PR46 and PR47 data or select winners per cell.

Expand standalone probes to all 20 capacity/payload combinations; actual inlined
comparator assembly is also retained. Extend the deterministic exact-capacity
and deque-model suite to capacity 65536. Existing lifetime, rollback, failed-pop
and concurrent FIFO checks cover every experimental form. All standard gates
remain required; they still qualify unchanged production, not a complete raw
replacement API or an equivalent weak-memory model.

Judge compiler lanes separately. A simple future GCC-only path may be considered
only if the full GCC scope supports parity, broad improvement over production,
and a materially reduced deficit without cell lookup. Keep x64 and Clang on
their proven production paths unless separately justified. A promising scope
result still requires full API/model integration and the original 80-cell
campaign before promotion. If new cells disprove the candidate, record them and
return to causal investigation rather than inventing a per-cell selector.

Keep WIN = lower 95% CI > 1, LOSS = upper 95% CI < 1, TIE = CI crosses 1.
Keep all capacity-2 comparisons conservatively. Moodycamel's requested capacity
2 can hold three elements; our queue still exposes exactly two. Record that
inequivalence without removing the cells, changing capacity or weakening the rule.

The official full 80-cell merged-production score remains
**35 WIN / 27 TIE / 18 LOSS**, **1.0497x [1.0007, 1.1015]**.
Main remains PR30 at `60ab1cedb6b79777549888d10fe4a51484f3b159`.
This is a scope-expansion evidence PR, not a production candidate. Do not merge.
