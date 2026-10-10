# PR45 factorial confirmation

## Why combine now

PR43 independently isolated consumer shape. Its paired GCC split/raw diagnostic
was 1.0683x [1.0235, 1.1283]; at 1024x16 it was
1.3669x [1.1221, 1.5679], WIN. It remained below external parity overall.

PR44 independently isolated private peer-cache placement. At GCC 64x16 the
grouped/raw diagnostic was 1.5543x [1.3790, 1.7599], WIN, and the grouped
candidate beat Drogalis at 1.0973x, WIN. Its GCC lane remained below parity
(0.9486x), with losses at 64x8 and 1024x16. Its broad grouped/raw interval
crossed 1, so a broad layout benefit is not established.

These findings justify testing an interaction. They do not prove additivity.
Neither individual candidate is promotable.

## Single next experiment

| Candidate | Split consumer | Group owner caches |
| --- | --- | --- |
| dynamic_raw | No | No |
| dynamic_split | Yes | No |
| dynamic_grouped | No | Yes |
| dynamic_combined | Yes | Yes |

All four variants share one implementation template, identical raw allocation,
runtime physical capacity, exact usable capacity, storage padding and producer
algorithm. Each retains acquire/release peer synchronization. Production remains
a separate PR30 same-run control. No cell lookup or production policy is added.

The combined alias is the only new candidate. Individual variants are remeasured
as necessary controls for the interaction experiment, not revived for promotion.
There is no per-cell oracle in the promotion decision.

Run GCC and Clang separately across 64/256/1024/65536 x 8/16/64, 1M transfers,
5 warmups and 50 randomized paired repetitions, retaining all four external
comparators. Use the unchanged analyzer and WIN/LOSS/TIE definition.
Probe standalone and actual inlined assembly and exercise all four forms with
exact-capacity, deque-model, wrap, nontrivial lifetime, construction exception,
failed-pop output preservation, concurrent FIFO and ASan/UBSan checks.

A favorable partial ARM result is only evidence for the next integration step.
Production promotion still needs the complete API and equivalent model coverage,
then the original full 80-cell, 50-repetition cross-algorithm campaign, including
capacity 2 and payload 256 and the conservative Moodycamel fairness annotations.
Keep the proven x64 PR30 path and qualify Clang independently.

## Preserved evidence

- PR42 source: 3a9594d8caf25f61ff7b9cb5c6cdf20e2e3911e8;
  run 38045494216.
- PR43 repaired source: fb2f6b9f39c92850187917e498b0aac4d1fa9e81;
  run 38050271005; summary artifact 11669626497.
  ARM: 0.9476x [0.9049, 0.9858], 1W/19T/4L.
- PR44 source: 1284004b8aecec9ae46c137f46f3904665e697c7;
  run 38050725941; summary artifact 11669657270.
  ARM: 0.9692x [0.9379, 0.9949], 1W/21T/2L.
- PR44's unchanged-production short regression gate initially failed at 64x8
  (0.7907x [0.6534, 0.9324], 15 reps/100K transfers, artifact 11668889322).
  One targeted repeat passed. Preserve both observations; this repeat supplies
  no promotion evidence for the experimental queue.
- All six standard qualification workflows are green on PR43 and the latest
  PR44 attempt. Experiment contract/sanitizer checks passed on both measured
  compilers and architectures.

The official production score remains **35 WIN / 27 TIE / 18 LOSS**.
No production headers have changed and no experimental PR has been merged.
