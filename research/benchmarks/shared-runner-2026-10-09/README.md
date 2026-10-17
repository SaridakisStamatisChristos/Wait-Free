# Frozen GitHub-hosted benchmark evidence — 2026-10-09

This directory is the permanent repository record of the `Benchmark Evidence` GitHub Actions campaign executed against VeriQueue `main`.

## Provenance

- Workflow: `Benchmark Evidence`
- Workflow run: `37928700809`
- Workflow event: `workflow_dispatch`
- Repository: `SaridakisStamatisChristos/Wait-Free`
- Tested commit: `fc9f16a77beea3e983c0a09eceed02dc1c618f9a`
- Actions artifact ID: `11614967224`
- Actions artifact name: `benchmark-evidence-shared-runner`
- Artifact SHA-256: `205d651179fa41a2f3ab97eecf319bd1fc191fd45a7b86e80265ae48f440b7a3`
- Artifact retention expiry: `2027-01-07T12:14:07Z`
- Runner: GitHub-hosted Ubuntu 24.04 / Azure `eastus2`
- Compiler: GCC 13.3.0
- Baselines: Rigtorp SPSCQueue commit `59a6a938513ea5004817383711ed35d32385d3ee`; Boost 1.83

The repository permanently preserves the campaign provenance, topology-scoped results, interpretation boundary, and cryptographic identities of every file in the original Actions artifact. The raw ZIP itself is retained by GitHub Actions under artifact ID `11614967224` until the retention expiry above; it is **not duplicated in this directory**. `SHA256SUMS.txt` allows any downloaded copy of the artifact payload to be checked against the evidence frozen here.

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
- Hardware PMU counters were unavailable on the GitHub-hosted runner because `perf_event_paranoid=4`; that limitation is part of the original artifact payload and is reflected in the evidence ledger.

## Preserved evidence identity

`SHA256SUMS.txt` records the SHA-256 of every file in the original artifact:

- raw padded and unpadded JSONL measurements;
- statistical summaries;
- machine and topology metadata;
- generated assembly and assembly-audit output;
- the unsuccessful PMU/perf attempt.

The Actions artifact itself is independently identified by its artifact ID and SHA-256 above. If a raw copy is archived elsewhere, matching those hashes establishes byte identity with this campaign.

## Evidence boundary

This is reproducible shared-runner evidence, not controlled bare-metal performance evidence. Frequency control, physical-host placement, noisy-neighbor effects, and PMU access are not under repository control. A self-hosted mini-PC campaign should be used for the stronger controlled-hardware result.
