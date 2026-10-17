# Final GitHub SOTA campaign

This document predeclares the final VeriQueue evidence campaign. PR17 changes campaign orchestration and evidence validation only; it does not change the production queue algorithm.

## Goal

Produce a permanent, machine-verifiable GitHub evidence package supporting the bounded claim:

> VeriQueue is a SOTA-quality wait-free SPSC implementation with unusually extensive independently layered verification and competitive/frontier-grade performance evidence across the declared GitHub-hosted x64/ARM64 matrix.

The campaign does **not** authorize claims that VeriQueue is universally fastest, fastest on every topology, or superior outside measured cells.

## One exact source commit

All evidence admitted to the final freeze must name exactly one source commit SHA.

The intended source is the final PR17 head after PR qualification. Qualification runs and full performance runs must all report that same `head_sha`. A run from a different commit is rejected even if its code is believed equivalent.

## Required qualification workflow classes

At least one successful run at the final source SHA is required from each class:

- `CI`
- `Sanitizers`
- `Fuzz Smoke`
- `Model Check`
- `Portability Qualification`
- `Performance Regression Gate`

These preserve the distinction between code/test correctness evidence and performance evidence.

## Independent full performance runs

The final source SHA must have **3 to 5 distinct successful `Final Performance Campaign` workflow runs**.

The default target is three independent dispatches. Each dispatch uses a different `run_index` and is expected to receive an independently provisioned GitHub-hosted runner for every architecture/compiler matrix job.

A full performance dispatch contains four independent jobs:

- Linux x64 / GCC
- Linux x64 / Clang
- Linux ARM64 / GCC
- Linux ARM64 / Clang

No samples are pooled across those jobs before per-job analysis.

## Fixed comparative protocol

Each full performance job runs the pinned comparative benchmark matrix with:

- capacities: `2,64,256,1024,65536`
- payload sizes: `8,16,64,256` bytes
- implementations: VeriQueue, Rigtorp, Boost.Lockfree, Moodycamel ReaderWriterQueue, drogalis SPSC-Queue
- transfers per invocation: `100000`
- warm-up rounds per cell: **5**
- measured rounds per cell: **30**
- deterministic randomized implementation order
- deterministic per-dispatch campaign seed derived from `20261009 + run_index`
- deterministic bootstrap seed derived from the same run index
- paired VeriQueue/competitor analysis within one job

The statistical classification remains the already-declared protocol:

- WIN when paired-ratio bootstrap 95% CI lower bound `> 1.03`
- LOSS when paired-ratio bootstrap 95% CI upper bound `< 0.97`
- otherwise TIE/INCONCLUSIVE

Thresholds must not be changed after final data are observed.

## Characterization evidence

The same jobs also execute the existing VeriQueue-only characterization harnesses:

- throughput: steady, producer-dominant, consumer-dominant, fill/drain
- burst widths
- RTT latency percentiles
- payload sensitivity
- bulk/scalar/consume characterization
- assembly audit

Characterization output is preserved as raw evidence. Comparative claims remain tied to the correctness-gated `bench_compare` protocol; characterization output is not silently promoted into competitor evidence.

## Final freeze

After the final source SHA has all required qualification runs and 3–5 successful full performance runs, dispatch `Finalize SOTA Campaign` from the repository default branch with their comma-separated run IDs.

The finalizer must fail closed unless:

1. every run exists, belongs to this repository, succeeded, and has artifacts;
2. every run has the same exact source SHA;
3. all required qualification workflow classes are present;
4. there are 3–5 distinct `Final Performance Campaign` runs;
5. each performance run exposes all four expected architecture/compiler artifacts;
6. final-performance run indices are distinct;
7. evidence freezing and independent checksum verification succeed.

The finalizer then:

- freezes all selected Actions artifacts under `evidence/campaigns/YYYY-MM-DD/<source-sha>/`;
- regenerates `docs/PERFORMANCE_SCOREBOARD.md` from checksum-valid frozen paired summaries;
- independently verifies the frozen campaign;
- creates an evidence-only branch and pull request against the default branch.

## Operational sequence for the stacked campaign

PR17 itself qualifies the orchestration with pull-request smoke jobs. The full 5/30 campaign is intentionally not executed as ordinary PR CI.

After PR17's workflow files are available on the default branch, the final campaign can still target the exact qualified PR17 source branch/ref. Dispatch three full performance runs against that ref, verify their `head_sha` matches the qualified source SHA, collect the required PR17 qualification run IDs, and invoke the finalizer.

This avoids changing code between qualification and performance measurement.

## Evidence interpretation

The final evidence package can support a SOTA-quality engineering statement if the frozen results remain consistent with it. It cannot predetermine the performance conclusion. Confirmed LOSS cells must remain visible in the generated scoreboard and final report.

No tag or release is created by PR17 or by the finalizer.
