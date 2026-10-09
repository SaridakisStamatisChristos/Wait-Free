# Verifier effectiveness

The mutation catalogue is deliberately separate from production. The campaign is a falsification test of the verifier: the correct Relacy model must pass, while each curated defective protocol must fail.

| Mutant | Defect | Primary killer | Evidence state |
|---|---|---|---|
| MUT-01 | tail release -> relaxed | Relacy weak-memory/data-race detection | killed |
| MUT-02 | tail acquire -> relaxed | Relacy weak-memory/data-race detection | killed |
| MUT-03 | head release -> relaxed | Relacy weak-memory/data-race detection | killed |
| MUT-04 | head acquire -> relaxed | Relacy weak-memory/data-race detection | killed |
| MUT-05 | publish tail before construction | Relacy lifetime/publication assertions | killed |
| MUT-06 | publish head before destruction | Relacy lifetime/reuse assertions | killed |
| MUT-07 | incorrect ring mask | Relacy FIFO/order assertions | killed |
| MUT-08 | off-by-one full condition | Relacy capacity/lifetime assertions | killed |

## Preserved campaign result

For code commit `d051ba8c362259e46ada68c2fb9b25e60725eb26`, GitHub Actions model-check run `37925976310` killed **8/8 curated mutants (100%)**. The mutation artifact is `11613968321`, digest `sha256:81d9de510c8cccc8962c95013d8af5e24646922b0691474d7cfccc8889f7a048`.

A compact machine-readable record is committed at `evidence/relacy/mutation-results-summary.json`. The workflow also preserves the full Relacy traces as an Actions artifact.

This metric applies only to the explicitly curated mutant set. It is not a proof that every possible concurrency defect would be detected.
