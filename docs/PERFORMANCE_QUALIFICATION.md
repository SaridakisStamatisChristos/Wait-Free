# Performance qualification

VeriQueue treats comparative performance as a measured property of a declared environment, not as a universal product attribute. This document defines the repository's current GitHub-hosted qualification protocol and the evidence required before comparative claims are promoted.

## Claim boundary

A qualification run may support statements only about the measured architecture, compiler, topology, payload, capacity, workload, and statistical protocol. It does not establish that VeriQueue is universally fastest or superior on unmeasured hardware.

The benchmark verdict rule is fixed before results are inspected:

- **WIN** when the paired-ratio bootstrap 95% confidence interval lower bound is greater than `1.03`;
- **LOSS** when the upper bound is less than `0.97`;
- otherwise **TIE/INCONCLUSIVE**.

Negative and inconclusive cells remain visible in preserved evidence.

## Source identity and prerequisite qualification

Every run admitted to one performance evidence freeze must identify the same exact source commit SHA. The selected source commit must also have successful evidence from each required workflow class:

- `CI`;
- `Sanitizers`;
- `Fuzz Smoke`;
- `Model Check`;
- `Portability Qualification`;
- `Performance Regression Gate`.

These workflow names are stable evidence identifiers because frozen manifests and the validator use them as provenance keys.

## Independent full measurements

The active full-measurement workflow is `Performance Qualification`. A complete hosted qualification contains three to five successful full runs from one source SHA.

Full runs may be launched manually or by creating branches named:

```text
qualification/run-1
qualification/run-2
...
qualification/run-5
```

Each full run contains independent jobs for:

- Linux x64 / GCC;
- Linux x64 / Clang;
- Linux ARM64 / GCC;
- Linux ARM64 / Clang.

Samples are analyzed within their job and are not silently pooled across architecture/compiler lanes.

The full comparative matrix is:

- capacities: `2, 64, 256, 1024, 65536`;
- payload sizes: `8, 16, 64, 256` bytes;
- implementations: VeriQueue, Rigtorp SPSCQueue, Boost.Lockfree, Moodycamel ReaderWriterQueue, and drogalis SPSC-Queue;
- transfers per invocation: `100000`;
- warm-up rounds per cell: `5`;
- measured rounds per cell: `30`;
- deterministic randomized implementation order;
- deterministic campaign/bootstrap seed derived from the run index.

Comparator revisions are pinned by the build and must not be silently changed when interpreting existing evidence.

## Characterization versus comparative evidence

The qualification workflow can also collect VeriQueue-only throughput, burst, latency, payload, bulk/scalar/consume, and assembly characterization. These outputs are useful for mechanism analysis but are not automatically comparative evidence. Comparative claims remain tied to the correctness-gated comparator matrix and its paired statistics.

## Evidence publication

`Publish Performance Evidence` validates the selected run set, freezes GitHub Actions artifacts under `evidence/campaigns/YYYY-MM-DD/<source-sha>/`, independently verifies checksums and provenance, regenerates the performance scoreboard, and opens an evidence-only pull request.

A complete qualification is admitted only after the validator confirms:

1. every selected run belongs to this repository, succeeded, and still has artifacts;
2. every selected run names the same source SHA;
3. all required qualification workflow classes are present;
4. three to five distinct full performance runs are present;
5. every full performance run contains all four architecture/compiler artifacts;
6. run indices and fixed protocol metadata are internally consistent.

The publisher never creates a release or tag. See [`EVIDENCE_FREEZING.md`](EVIDENCE_FREEZING.md).

## Legacy evidence compatibility

Older frozen evidence may name the workflow `Final Performance Campaign` and use branches named `final-campaign/run-N`. `tools/validate_performance_qualification.py` accepts those names **only as legacy provenance aliases** so historical evidence remains verifiable. New runs and automation use `Performance Qualification` and `qualification/run-N`.

Historical workflow definitions and experimental tooling are archived under [`../research/`](../research/). They are not active CI and do not define the current product.

The source of truth for established claims remains [`EVIDENCE_LEDGER.md`](EVIDENCE_LEDGER.md).
