# Research archive

This directory contains dated or experiment-specific material that is scientifically useful for reproducing how VeriQueue was measured and optimized, but is not part of the current product documentation.

Current API, architecture, memory-model, verification, limitation, benchmark-methodology, and performance-qualification documentation lives in [`docs/`](../docs/). Generated or deliberately frozen machine evidence lives in [`evidence/`](../evidence/).

## Layout

- `benchmarks/` — dated benchmark captures and their integrity records.
- `performance/` — historical optimization and qualification records.
- `verification/` — dated validation snapshots.

## Historical terminology

Research records may deliberately retain PR numbers, prior campaign names, words such as "final" or "SOTA", and exact branch/workflow identifiers. Those strings are provenance: changing them would make it harder to map a preserved result back to the GitHub run or experiment that produced it.

Their presence here does **not** make them current milestones or current product claims. In particular, historical performance hypotheses and rejected experimental branches do not alter the production queue unless a separately qualified change was merged into `main`.

When adding new material, prefer descriptive, dated names and record the exact source commit, environment, protocol, and evidence boundary.
