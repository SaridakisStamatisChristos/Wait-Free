# Research archive

This directory contains dated, experiment-specific, or retired automation material that is scientifically useful for reproducing how VeriQueue was measured and optimized, but is not part of the current product surface.

Current API, architecture, memory-model, verification, limitation, benchmark-methodology, and performance-qualification documentation lives in [`docs/`](../docs/). Generated or deliberately frozen machine evidence lives in [`evidence/`](../evidence/). Active GitHub automation lives only in [`.github/workflows/`](../.github/workflows/).

## Layout

- `benchmarks/` — dated benchmark captures and their integrity records.
- `performance/` — historical optimization and qualification records.
- `verification/` — dated validation snapshots.
- `workflows/` — retired GitHub Actions definitions preserved byte-for-byte as provenance.
- `experiments/` — retired experimental benchmark/tool sources removed from the production build surface.
- [`PULL_REQUEST_ARCHIVE.md`](PULL_REQUEST_ARCHIVE.md) — disposition index for closed evidence-only experimental PRs.

## Reproduction boundary

Archived workflow and experiment files are **records**, not maintained CI entry points. Some preserved workflow files intentionally reference the paths that existed when the experiment ran. For exact execution, use the source commit or PR recorded by that experiment. Current `main` does not silently rewrite historical protocols to make them look contemporary.

## Historical terminology

Research records may deliberately retain PR numbers, prior campaign names, words such as "final" or "SOTA", and exact branch/workflow identifiers. Those strings are provenance: changing them would make it harder to map a preserved result back to the GitHub run or experiment that produced it.

Their presence here does **not** make them current milestones or current product claims. Historical hypotheses and rejected experimental branches do not alter the production queue unless a separately qualified change was merged into `main`.

When adding new material, prefer descriptive, dated names and record the exact source commit, environment, protocol, and evidence boundary.
