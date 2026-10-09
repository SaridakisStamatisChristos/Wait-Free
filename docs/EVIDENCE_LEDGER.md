# Evidence ledger

This ledger prevents experimental language from silently becoming marketing language. The frozen evidence below applies to code commit `d051ba8c362259e46ada68c2fb9b25e60725eb26` unless a row states otherwise.

| Claim | State | Evidence |
|---|---|---|
| Sequential FIFO semantics | verified experimentally | GCC/Clang deterministic + model CI, run `37925976300` |
| Capacity/full/empty boundaries | verified experimentally | unit suite in run `37925976300` |
| `uint8_t` / `uint16_t` rollover behavior | verified experimentally | forced wraparound suite in run `37925976300` |
| Queued object lifetime cleanup | verified experimentally | lifetime oracle + ASan/UBSan |
| Monotonic concurrent transfer under sampled schedules | verified experimentally | stress suite + TSan |
| No detected ASan/UBSan issue | empirically validated for tested executions | sanitizer run `37925976467`, artifact `11613633175` |
| No detected TSan race | empirically validated for tested executions | sanitizer run `37925976467`, artifact `11614500625` |
| Small weak-memory protocol executions satisfy assertions | model-checked for configured Relacy search | model-check run `37925976310`; committed `evidence/relacy/relacy.log` |
| 64 sampled short histories are linearizable | empirically validated | Porcupine run `37925976310`; committed summary + artifact `11614266051` |
| Curated verifier mutation kill rate is 100% | verified for MUT-01..MUT-08 | 8/8 killed; `VERIFIER_EFFECTIVENESS.md` + artifact `11613968321` |
| Coverage-guided fuzz smoke completed without discovered failure | empirically validated for campaign budget | fuzz run `37925976391`, artifact `11614332252` |
| Rigtorp/Boost comparison laboratory builds | verified | benchmark-build job in CI run `37925976300` |
| Representative hot path has no detected mutex/allocation/CAS/locked instruction | assembly-audited for representative specialization/compiler | assembly audit in CI run `37925976300` |
| Synchronization/control path is wait-free | reasoned at algorithm level | bounded-operation argument in `PROGRESS_GUARANTEE.md` |
| Faster than Rigtorp/Boost on a named machine | unsupported until comparative benchmark evidence is produced on that machine | benchmark artifact required |
| Universally fastest SPSC queue | unsupported | must not claim |

Passing the listed tools is evidence, not a universal proof of C++ correctness or performance. Performance remains topology-, compiler-, payload-, and machine-dependent.
