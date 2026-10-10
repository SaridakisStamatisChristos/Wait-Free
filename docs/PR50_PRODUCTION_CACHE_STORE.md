# PR50: transfer only failed scalar cache-store omission

 — not qualified for production

Source `fd7df64ea30e8423d88c77dbf003680baa03550b`; [run 38058522348](https://github.com/SaridakisStamatisChristos/Wait-Free/actions/runs/38058522348); summary artifact **11672206997**. All six standard gates and the dedicated campaign passed. Ten shards contain 17,600 valid warmup/measured records: all 40 ARM cells have 50 complete randomized paired rounds, correct counts/checksums/affinity, one source head and pinned comparator versions.

| Candidate / lane | vs fastest external | 95% CI | W/T/L | vs production | 95% CI |
| --- | --- | --- | --- | --- | --- |
| raw_progress / ARM40 | 0.9692x | [0.9533, 0.9841] | 1/32/7 | 1.0392x | [0.9760, 1.0996] |
| raw_progress / GCC20 | 0.9493x | [0.9244, 0.9725] | 0/13/7 | 1.0960x | [1.0317, 1.1723] |
| raw_progress / Clang20 | 0.9895x | [0.9750, 1.0043] | 1/19/0 | 0.9854x | [0.8876, 1.0716] |
| raw_combined / ARM40 | 0.9455x | [0.9223, 0.9670] | 1/28/11 | 1.0306x | [0.9720, 1.0872] |
| production / ARM40 | 0.9194x | [0.8607, 0.9851] | 3/18/19 | 1.0000x | [1.0000, 1.0000] |

Post-hoc same-round progress/combined diagnostics using the unchanged 20,000-sample bootstrap helpers support a modest GCC mechanism: **1.0268x [1.0064, 1.0521]**, 1W/19T/0L. Clang is **1.0046x [0.9852, 1.0253]**, 1W/19T/0L. ARM40 is 1.0156x [1.0009, 1.0317]. These diagnostics neither change external classifications nor justify a per-cell selector. GCC's only direct significant cell win is 65536x16; do not specialize to it.

GCC external losses remain 2x64 (Moodycamel; unequal usable capacity), 2x256, 64x64, 1024x8, 1024x16, 65536x8 and 65536x16 (the other six versus Rigtorp). Clang has no external losses in this run but regresses significantly against production at all four 64-byte-payload capacities >=64, including approximately 0.554x at 1024 and 0.536x at 65536. Thus this allocating raw policy is rejected for universal promotion; GCC parity and Clang preservation have not been achieved.

Both compiler artifacts verify all 40 standalone producer/consumer failure paths omit the equal-value cache store (control: one; candidate: zero), retaining one peer acquire and successful release. All 40 actual raw_progress thread loops per compiler guard the cache store with the failure branch after peer acquisition. This establishes the intended codegen change; hardware coherence causality is not measured.

Retain the failed-store omission as a narrow forensic mechanism for a separate test on the existing production representation; its allocating raw queue is not integrated. Production's inline/noexcept constructor, full API and state/storage remain required. Main stays PR30 and the official original 80-cell score remains **35 WIN / 27 TIE / 18 LOSS, 1.0497x [1.0007, 1.1015]**. No experiment is merged; this 40-cell evidence does not replace the official scoreboard.


## Prespecified production experiment

PR50 starts directly from PR30 main, not the allocating raw experiment branches. Apply only the acquired-peer comparison before private-cache assignment on failed scalar `try_emplace` (and both `try_push` overloads), `try_pop` and `try_consume`. Select it broadly on GCC/AArch64, with no capacity or payload thresholds. The bulk operations retain PR30's ordering. All acquire/release events and successful cache updates occur at their original logical points; failed stores wrote the unchanged cache value. Runtime storage, striping, cursor arithmetic, control layout, object size/alignment, exact capacity and public signatures remain PR30. The constructor remains inline and `noexcept`; no heap/raw representation is transplanted.

Verification-only force-progress/force-legacy switches are mutually exclusive. Add a separate CMake verification flag and use it in existing forced single-owner CI, sanitizer and fuzz lanes and the single-owner history generator. Native ARM/GCC portability tests exercise the default new selection. Relacy retains the old scalar/bulk model and additionally tests the new scalar order, including mixed bulk/consume scenarios at capacities 1/2/4. Failed paths assert that the skipped private assignment would write the existing cache value. No bulk policy change is modeled or integrated.

A new state-equivalence unit test compares all six cursor/cache snapshot values, FIFO outputs, failed-pop output and callback behavior, advisory observations and partial bulk results against a namespace-only frozen PR30 header after 80,000 mixed operations, using uint8 cursors to cross rollover repeatedly at capacities 1/2/4/64. The existing forced suite covers constructor exceptions, nontrivial lifetimes, move-only/non-default-constructible values, destructor cleanup, over-alignment, exact capacity and concurrent scalar/bulk/consume FIFO. A representation change would require more qualification; this test does not assert instruction-level store absence, which is checked from assembly.

## Original 80 cells with a same-run control

Use all original 80 architecture/compiler/capacity/payload cells, 1M transfers, five warmups and 50 randomized paired repetitions. Retain all four pinned externals and unchanged workload/CPU-affinity/checksum behavior. Add `pr30_control` as a sixth implementation from the exact main header, changing only its namespace and introducing an alias to production detail types. Both queue factories use identical allocation/capture patterns and short implementation names. Static assertions verify equal object size/alignment. The original external analyzer receives the original five implementations after excluding the frozen control; it continues choosing the fastest *external* by cell median and uses its unchanged classification and bootstrap method. The control never competes for the fastest-external label. Report candidate/control paired diagnostics separately, without selecting per-cell policies or pooling runs.

Emit complete inlined comparator loops and all 40 candidate/control scalar probes per compiler/architecture. Native x64 GCC's 40 local probe pairs are instruction-identical after branch-label normalization. ARM/GCC artifacts must establish the intended failed-store change, while Clang/x64 should retain PR30's hot path.

Local strict GCC C++20 compilation and all 16 unit/model/stress executables passed with forced single-owner/progress order. Local ASan/UBSan passed the mixed state test with leak detection disabled because this environment cannot inspect `/proc`; unrestricted GitHub sanitizer gates remain required. The local environment lacks CMake, so local binaries were compiled directly with the project's warning flags. The new analyzer is integration-tested with an explicitly synthetic 80-cell fixture; those synthetic rates are not performance evidence. Format, Python compilation, original comparator tooling and YAML checks are required before publishing.

Await CI, Model Check, Sanitizers, Fuzz Smoke, Portability Qualification, Performance Regression Gate and the complete 80-cell campaign. Do not merge or promote this branch before full evidence. A GCC gain observed in the raw queue cannot be assumed to transfer to production. If it does not transfer, record the result and reject the policy. Keep capacity-2 Moodycamel comparisons conservative and annotate its 3-versus-2 usable capacity. Main remains PR30 and its official scoreboard is unchanged until a qualified production change is merged and the original 80-cell result is accepted.
