# Frozen GitHub-hosted benchmark evidence — 2026-10-09

This directory is an immutable snapshot of the `Benchmark Evidence` GitHub Actions campaign executed against VeriQueue `main`.

## Provenance

- Workflow: `Benchmark Evidence`
- Workflow run: `37928700809`
- Workflow event: `workflow_dispatch`
- Repository: `SaridakisStamatisChristos/Wait-Free`
- Tested commit: `fc9f16a77beea3e983c0a09eceed02dc1c618f9a`
- Actions artifact ID: `11614967224`
- Actions artifact name: `benchmark-evidence-shared-runner`
- Artifact SHA-256: `205d651179fa41a2f3ab97eecf319bd1fc191fd45a7b86e80265ae48f440b7a3`
- Runner: GitHub-hosted Ubuntu 24.04 / Azure `eastus2`
- Compiler: GCC 13.3.0
- Baselines: Rigtorp SPSCQueue commit `59a6a938513ea5004817383711ed35d32385d3ee`; Boost 1.83

The original Actions artifact expires according to GitHub retention policy. The archive committed next to this file preserves the exact evidence payload in Git history.

## Direct comparison result

For `uint64_t`, capacity 1024, 5,000,000 transfers per comparison run, 20 measured repetitions:

| Topology | VeriQueue median | Rigtorp median | Boost.Lockfree median |
|---|---:|---:|---:|
| separate cores, same LLC | 126.6305 M transfers/s | 36.01845 M transfers/s | 47.4820 M transfers/s |
| SMT siblings | 148.1585 M transfers/s | 169.0880 M transfers/s | 113.3785 M transfers/s |

Interpretation is deliberately topology-scoped. On this hosted runner VeriQueue led both baselines on the separate-core/same-LLC placement, while Rigtorp led VeriQueue on SMT siblings. These results are not a universal fastest-queue claim.

## Other observations

- VeriQueue RTT median: 250 ns on separate cores/same LLC and 110 ns on SMT siblings.
- The padded implementation materially outperformed the intentionally unpadded mutant in several steady-state configurations, supporting the false-sharing mitigation design.
- The assembly audit passed and detected no mutex/allocation/CAS/locked instruction in the representative hot path.
- Hardware PMU counters were unavailable on the GitHub-hosted runner because `perf_event_paranoid=4`; the archive preserves that negative result.

## Preserved payload

`benchmark-evidence-shared-runner.zip` is the exact GitHub Actions artifact payload. It contains the raw padded and unpadded JSONL, statistical summaries, machine and topology metadata, assembly, assembly audit, and perf output.

`SHA256SUMS.txt` records hashes for every file inside the original artifact.

## Evidence boundary

This is reproducible shared-runner evidence, not controlled bare-metal performance evidence. Frequency control, physical-host placement, noisy-neighbor effects, and PMU access are not under repository control. A self-hosted mini-PC campaign should be used for the stronger controlled-hardware result.
