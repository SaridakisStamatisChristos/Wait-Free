# Paired performance regression report

Ratio is `candidate throughput / reference throughput` within the same runner and repetition.
Hard FAIL is predeclared as median < 0.95 AND bootstrap 95% CI upper bound < 0.98. WARN is median < 0.98 when the hard-fail rule is not met.

| Capacity | Payload | Topology | n | Median | IQR | p05 | p95 | CV | 95% CI | Verdict |
|---:|---:|---|---:|---:|---|---:|---:|---:|---|---|
| 64 | 8 | unspecified | 15 | 0.9189 | [0.8035, 1.0572] | 0.7183 | 1.1492 | 0.1767 | [0.7779, 1.0462] | WARN |
| 64 | 64 | unspecified | 15 | 0.9954 | [0.9685, 1.0130] | 0.9397 | 1.0544 | 0.0416 | [0.9679, 1.0124] | PASS |
| 1024 | 8 | unspecified | 15 | 0.9037 | [0.7625, 1.0855] | 0.4807 | 1.3483 | 0.2978 | [0.7905, 1.0864] | WARN |
| 1024 | 64 | unspecified | 15 | 0.9519 | [0.8519, 1.0055] | 0.7875 | 1.3027 | 0.1741 | [0.8682, 1.0119] | WARN |
