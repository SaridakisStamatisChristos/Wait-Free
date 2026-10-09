# Paired performance regression report

Ratio is `candidate throughput / reference throughput` within the same runner and repetition.
Hard FAIL is predeclared as median < 0.95 AND bootstrap 95% CI upper bound < 0.98. WARN is median < 0.98 when the hard-fail rule is not met.

| Capacity | Payload | Topology | n | Median | IQR | p05 | p95 | CV | 95% CI | Verdict |
|---:|---:|---|---:|---:|---|---:|---:|---:|---|---|
| 64 | 8 | unspecified | 15 | 0.9973 | [0.9528, 1.0429] | 0.9300 | 1.3124 | 0.1855 | [0.9535, 1.0669] | PASS |
| 64 | 64 | unspecified | 15 | 1.0013 | [0.9634, 1.0061] | 0.8136 | 1.0165 | 0.0666 | [0.9522, 1.0090] | PASS |
| 1024 | 8 | unspecified | 15 | 0.9937 | [0.9585, 1.0363] | 0.9206 | 1.0646 | 0.0501 | [0.9495, 1.0432] | PASS |
| 1024 | 64 | unspecified | 15 | 0.9994 | [0.9965, 1.0027] | 0.9931 | 1.0094 | 0.0050 | [0.9956, 1.0027] | PASS |
