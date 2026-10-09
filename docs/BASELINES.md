# Baselines

## Primary

- **Rigtorp SPSCQueue** — directly relevant bounded SPSC implementation with cached indices and false-sharing avoidance.
- **Boost.Lockfree `spsc_queue`** — established SPSC baseline.

## Optional

- **Folly ProducerConsumerQueue** — useful if dependency cost is acceptable.
- **bounded `std::queue` + `std::mutex`** — blocking intuition baseline only.

Baseline revisions belong in each benchmark evidence manifest. This document intentionally does not claim performance superiority before controlled comparative runs are preserved.
