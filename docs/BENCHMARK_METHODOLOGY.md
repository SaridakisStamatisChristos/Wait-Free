# Benchmark methodology

Correctness, progress, and performance are separate claims. A correctness result never substitutes for a benchmark and a benchmark never establishes correctness.

## Required experiment metadata

Every preserved result should record: Git commit, compiler/version, standard library, flags, LTO status, CPU/microarchitecture, OS/kernel, frequency governor, SMT status, NUMA topology, exact thread affinity, payload, capacity, workload, warmup, measurement duration/count, and repetition number.

`tools/collect_machine_info.sh` captures a portable subset. GitHub-hosted evidence records whatever topology/environment facts the runner exposes and explicitly leaves unavailable host controls unknown rather than inferring them.

## Comparative correctness gate

`bench_compare` verifies the transferred sequence and every byte of each synthetic payload while timing the run. A measurement is valid only when producer and consumer counts both equal the requested transfer count and no payload corruption, loss, duplication, or FIFO violation is observed. Invalid runs emit `valid=false`, zero throughput, and return a non-zero exit status.

This check is intentionally part of the comparative harness: a queue implementation does not receive a performance result for doing less work or returning incorrect data.

## Workloads and matrix

The dedicated harnesses cover steady throughput, round-trip latency, burst proxies, and payload sizes 8/16/64/256 bytes.

The comparative throughput harness supports the declared GitHub campaign matrix:

- capacities: 2, 64, 256, 1024, 65536;
- payloads: 8, 16, 64, 256 bytes;
- one implementation per process invocation so the campaign driver can deterministically randomize order;
- exact thread-affinity metadata where pinning is available.

Topology labels are evidence, not assumptions. Hosted runners cannot be treated as controlled bare metal, cross-CCD, or cross-NUMA machines unless the exposed environment proves that placement.

## Statistics

Use repeated paired runs on the same GitHub runner. Default target for headline cells: five warmups followed by 20–30 measured repetitions. Randomize implementation order deterministically for each repetition so no implementation is systematically first or last.

For each VeriQueue/baseline pair, compute the within-repetition throughput ratio and report at minimum median ratio, IQR, 5th/95th percentiles, coefficient of variation, and a deterministic bootstrap 95% confidence interval. Do not choose the best run as the headline result.

Predeclared classification for hosted-runner paired ratios:

- **WIN**: the 95% confidence interval lower bound is above `1.03`;
- **LOSS**: the 95% confidence interval upper bound is below `0.97`;
- **TIE/INCONCLUSIVE**: otherwise.

These thresholds are fixed before the final campaign and must not be tuned per result.

## Latency

Round-trip latency uses two queues so a token travels thread A -> thread B -> thread A. Report median, p90, p95, p99, p99.9 when sample size supports it, plus min/max. Minimum latency is never presented as representative.

## Baselines

The default GitHub-hosted same-class baseline set is pinned in the build:

- Rigtorp `SPSCQueue` — commit `59a6a938513ea5004817383711ed35d32385d3ee`;
- Boost.Lockfree `spsc_queue` — exact installed Boost version recorded in every result;
- Moodycamel `ReaderWriterQueue` — commit `6867b56452352acf077fccd5f6cc7e3a8cfde0fb`;
- drogalis `SPSC-Queue` — commit `c959bd4f7204dd73c4e75a2b00c3459c3a5e9a96`.

Folly `ProducerConsumerQueue` remains an optional external comparison because importing the full Folly dependency surface into the default hosted CI would materially change build cost and failure modes. It must not be silently represented as tested when it is not present.

A mutex-bounded `std::queue` may be included only as a clearly labeled blocking baseline.

## Hardware counters

On Linux, collect cycles, instructions, branches, branch misses, cache references/misses, LLC activity, context switches, and migrations only when permitted by the environment. GitHub-hosted runners with restrictive `perf_event_paranoid` settings are recorded as PMU-unavailable; missing counters are never fabricated or used as a failed correctness gate.

## False-sharing experiment

Build both normal and `VERIQUEUE_DISABLE_PADDING=ON` variants. Preserve both results even if padding does not help.

## Claim discipline

Acceptable wording identifies runner class, compiler, topology information actually exposed, workload, payload, capacity, repetitions, and measured paired difference. GitHub-hosted comparative evidence is useful and reproducible but is not controlled bare-metal evidence. The project does not claim universal superiority without broad reproducible evidence.
