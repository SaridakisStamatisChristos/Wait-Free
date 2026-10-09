# Evidence ledger

This ledger prevents experimental language from silently becoming marketing language. The original verification freeze below applies to code commit `d051ba8c362259e46ada68c2fb9b25e60725eb26` unless a row states otherwise. The frozen shared-runner benchmark campaign applies to merge commit `fc9f16a77beea3e983c0a09eceed02dc1c618f9a`.

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
| Representative hot path has no detected mutex/allocation/CAS/locked instruction | assembly-audited for representative specialization/compiler | CI run `37925976300`; repeated PASS in benchmark run `37928700809` |
| Synchronization/control path is wait-free | reasoned at algorithm level | bounded-operation argument in `PROGRESS_GUARANTEE.md` |
| Shared-runner same-LLC comparison at capacity 1024 / 8-byte payload | empirically validated on named GitHub-hosted runner | run `37928700809`: VeriQueue 126.6305 M/s median, Rigtorp 36.01845 M/s, Boost.Lockfree 47.4820 M/s; 20 repetitions; frozen under `evidence/benchmarks/shared-runner-2026-10-09/` |
| Shared-runner SMT-sibling comparison at capacity 1024 / 8-byte payload | empirically validated on named GitHub-hosted runner | run `37928700809`: VeriQueue 148.1585 M/s median, Rigtorp 169.0880 M/s, Boost.Lockfree 113.3785 M/s; 20 repetitions; frozen under `evidence/benchmarks/shared-runner-2026-10-09/` |
| GitHub-hosted benchmark PMU counters available | not established / unavailable | `perf_event_paranoid=4`; failure preserved in the frozen Actions artifact |
| Faster than Rigtorp/Boost on controlled dedicated hardware | unsupported until self-hosted comparative benchmark evidence is produced | mini-PC campaign required |
| Universally fastest SPSC queue | unsupported | must not claim |

Passing the listed tools is evidence, not a universal proof of C++ correctness or performance. Performance remains topology-, compiler-, payload-, workload-, and machine-dependent. Shared GitHub-runner results are preserved as reproducible comparative evidence but are not treated as controlled bare-metal measurements.
