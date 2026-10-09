# Performance regression policy

VeriQueue performance regression gates are **paired candidate/reference comparisons**, not absolute throughput thresholds.

The purpose of this gate is to detect material regressions introduced by a pull request while avoiding false confidence from comparing unrelated GitHub-hosted runners.

## Reference and candidate

For a pull request:

- **candidate** is the pull-request head under test;
- **reference** is the pull-request base commit captured by GitHub for that run;
- candidate and reference are built independently with the same compiler family, build type, and benchmark harness interface;
- every measured pair runs inside the same GitHub Actions job on the same runner.

The gate does not compare a current run to an old absolute throughput number from another machine or another workflow run.

## Correctness prerequisite

Every candidate and reference benchmark invocation must pass the benchmark's exact FIFO/content validation before its throughput sample is accepted.

A malformed result, non-positive throughput, missing pair, duplicate pair, benchmark process failure, or correctness-invalid result fails the regression job as a harness/correctness error. It is never converted into a performance result.

## Pairing protocol

Within each declared benchmark cell:

1. warm-up pairs run first and are excluded from analysis;
2. measured repetitions use deterministic pseudo-random candidate/reference order;
3. each repetition contains exactly one candidate and one reference result;
4. the analyzed ratio is:

```text
candidate throughput / reference throughput
```

Pairing is by cell and repetition. Samples are never pooled across runner instances.

## Predeclared thresholds

The following policy is fixed before using this gate for qualification:

- **HARD REGRESSION / FAIL** when both are true:
  - paired median ratio `< 0.95`; and
  - bootstrap 95% confidence-interval upper bound `< 0.98`.
- **WARNING** when the hard-fail rule is not met but paired median ratio `< 0.98`.
- **PASS** otherwise.

This deliberately requires both a practically material median degradation and confidence that the candidate remains below a 2% regression boundary.

The thresholds must not be changed after observing a pull request's data in order to make that pull request pass.

## CI matrix

The pull-request regression gate uses a compact representative matrix suitable for repeated shared-runner CI:

- capacities: `64`, `1024`;
- payload sizes: `8`, `64` bytes;
- one topology selected by the benchmark harness on the current runner;
- warm-up pairs: `2`;
- measured pairs: `15`;
- transfers per invocation: `100000`;
- deterministic order seed: `20261009`;
- bootstrap resamples: `10000`;
- deterministic bootstrap seed: `20261009`.

The final publication campaign remains broader than this regression gate. Passing this compact CI gate is not evidence of universal performance superiority.

## Runner scope

The regression gate is intended for GitHub-hosted Linux x64 and ARM64 jobs when both are available. Each architecture is evaluated independently. A failure on either architecture fails the workflow.

Results from different architectures or runner instances are never merged into one confidence interval.

## Interpretation

A PASS means only that this paired CI protocol did not detect a material regression relative to the pull request's base commit in the declared cells.

A WARNING is preserved as evidence and should be investigated, but does not by itself block the pull request.

A FAIL is a blocking performance regression signal. It does not identify the root cause; optimization, assembly inspection, cache-line behavior, topology interaction, or benchmark-harness defects may require separate investigation.
