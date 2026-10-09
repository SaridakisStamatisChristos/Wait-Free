# Benchmark methodology

Correctness, progress, and performance are separate claims. A correctness result never substitutes for a benchmark and a benchmark never establishes correctness.

## Required experiment metadata

Every preserved result should record: Git commit, compiler/version, standard library, flags, LTO status, CPU/microarchitecture, OS/kernel, frequency governor, SMT status, NUMA topology, exact thread affinity, payload, capacity, workload, warmup, measurement duration/count, and repetition number.

`tools/collect_machine_info.sh` captures a portable subset. Dedicated benchmark evidence should add topology-specific details when available.

## Workloads

The harnesses cover steady throughput, round-trip latency, burst proxies, and payload sizes 8/16/64/256 bytes. The roadmap extends the capacity matrix across 1, 2, 64, 256, 1024, and 65536 and compares same-SMT, shared-cache, same-LLC, cross-CCD, and cross-NUMA placements where the machine supports them.

## Statistics

Use independent repeated runs. Default target: five warmups followed by 20–30 measured repetitions. Report median, IQR, 5th/95th percentiles, and coefficient of variation. Do not choose the best run as the headline result.

## Latency

Round-trip latency uses two queues so a token travels thread A -> thread B -> thread A. Report median, p90, p95, p99, p99.9 when sample size supports it, plus min/max. Minimum latency is never presented as representative.

## Baselines

Primary same-class baselines are Rigtorp SPSCQueue and Boost.Lockfree `spsc_queue`; Folly ProducerConsumerQueue is optional. Versions/commit hashes must be pinned in each comparative evidence bundle. A mutex-bounded `std::queue` may be included only as a clearly labeled blocking baseline.

## Hardware counters

On Linux, collect cycles, instructions, branches, branch misses, cache references/misses, LLC activity, context switches, and migrations when supported. Useful derived metrics include cycles/item, instructions/item, IPC, and misses per million items.

## False-sharing experiment

Build both normal and `VERIQUEUE_DISABLE_PADDING=ON` variants. Preserve both results even if padding does not help.

## Claim discipline

Acceptable wording identifies machine, compiler, topology, workload, payload, capacity, and measured difference. The project does not claim universal superiority without broad reproducible evidence.
