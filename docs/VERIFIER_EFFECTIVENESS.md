# Verifier effectiveness

The mutation catalogue is deliberately separate from production. The campaign is a falsification test of the verifier: mutation ID 0 (the correct model) must pass first, while each curated defective protocol must fail. A failed baseline invalidates the campaign rather than being counted as a mutation kill.

## Campaign v2 catalogue

The current campaign contains 24 curated mutants grouped by fault class rather than one undifferentiated percentage.

| IDs | Fault class | Representative defects |
|---|---|---|
| MUT-01..04 | memory-order | weaken producer/consumer release-acquire edges |
| MUT-05..06 | publication-order | publish availability/reuse before slot access |
| MUT-07,14,15 | slot-mapping | collapse or offset physical slot selection |
| MUT-08,09,22 | capacity | overfill, premature-full, or omit full check |
| MUT-10,11,21 | cache-refresh | stale cached remote cursor or inverted refresh predicate |
| MUT-12,13,19,20 | cursor | double increment or suppress owner-local cursor update |
| MUT-16,17,23,24 | publication-value | publish one-ahead or stale head/tail values |
| MUT-18 | data-integrity | corrupt the stored payload |

Two complementary Relacy oracles are run for every mutant:

1. a deterministic bounded FIFO contract scenario that checks exact full/empty behavior, slot reuse, FIFO identity, and final occupancy;
2. a concurrent semantic scenario that attacks weak-memory publication while checking the FIFO prefix of successful pushes and final logical occupancy.

Machine-readable evidence is emitted as:

- `evidence/relacy/mutation-results.json`
- `evidence/relacy/mutation-matrix.csv`
- `evidence/relacy/mutation-summary.md`

The JSON includes per-mutant traces and per-fault-class kill statistics. CI fails on any unexplained survivor.

## Previously frozen campaign

For code commit `d051ba8c362259e46ada68c2fb9b25e60725eb26`, GitHub Actions model-check run `37925976310` killed **8/8 curated mutants (100%)**. The mutation artifact is `11613968321`, digest `sha256:81d9de510c8cccc8962c95013d8af5e24646922b0691474d7cfccc8889f7a048`.

That historical 8-mutant result remains valid for its exact commit and catalogue. The v2 24-mutant campaign is a new evidence gate and must not be described as passing until its own GitHub Actions run completes successfully.

Mutation score is evidence about these explicit fault models. It is not a proof that every possible concurrency defect would be detected.
