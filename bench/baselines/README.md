# Benchmark baselines

Primary apples-to-apples baselines are Rigtorp `SPSCQueue` and Boost.Lockfree `spsc_queue`. Baseline source versions must be pinned in benchmark evidence before comparative claims are published. Folly `ProducerConsumerQueue` is optional because of dependency cost.

The repository intentionally does not hard-code benchmark claims in source. `tools/run_bench.py` emits raw JSONL; `tools/analyze_bench.py` derives summaries.
