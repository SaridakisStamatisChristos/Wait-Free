# Contributing

Changes to the queue require stronger review than ordinary utility code.

1. Explain the invariant or performance hypothesis being changed.
2. Add/modify a falsifying test before relying on a benchmark.
3. Keep the SPSC hot path allocation-free and free of locks/CAS unless the architecture itself is intentionally changed.
4. Run GCC and Clang builds with warnings-as-errors.
5. Run deterministic/model tests and sanitizers.
6. For memory-order changes, rerun Relacy and the mutation campaign.
7. For performance changes, preserve before/after raw data and machine metadata. Delete optimizations that do not produce a reproducible improvement.
8. Do not strengthen README claims beyond the evidence ledger.

Formatting follows the repository `.clang-format`. Every warning or static-analysis suppression needs a local rationale.
