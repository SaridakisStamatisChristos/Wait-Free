# Verifier effectiveness

The mutation catalogue is deliberately separate from production.

| Mutant | Defect | Expected killer(s) | Evidence state |
|---|---|---|---|
| MUT-01 | tail release -> relaxed | Relacy / weak-memory | campaign required |
| MUT-02 | tail acquire -> relaxed | Relacy / weak-memory | campaign required |
| MUT-03 | head release -> relaxed | Relacy / weak-memory | campaign required |
| MUT-04 | head acquire -> relaxed | Relacy / weak-memory | campaign required |
| MUT-05 | publish tail before construction | Relacy / stress / sanitizer | campaign required |
| MUT-06 | publish head before destruction | Relacy / lifetime / sanitizer | campaign required |
| MUT-07 | incorrect ring mask | model / wraparound / fuzz | campaign required |
| MUT-08 | off-by-one full condition | unit / model / fuzz | campaign required |

A 100% kill-rate badge or README claim must not be added until an automated campaign has run against the commit and its result is preserved under `evidence/`.
