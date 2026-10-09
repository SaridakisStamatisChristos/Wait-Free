# VeriQueue Performance Scoreboard

> Generated from checksum-valid frozen GitHub campaign evidence. Do not edit benchmark numbers by hand.

Ratio is `VeriQueue throughput / competitor throughput` within the same measured repetition. The predeclared verdict rule is: **WIN** when the bootstrap 95% CI lower bound is `> 1.03`; **LOSS** when the upper bound is `< 0.97`; otherwise **TIE/INCONCLUSIVE**.

This scoreboard summarizes only committed evidence under `evidence/campaigns/`. Absence of a platform or cell means **no frozen evidence**, not a win.

## Frozen campaign inventory

| Date | Source commit | Benchmark summaries | Comparison cells |
|---|---|---:|---:|
| 2026-10-09 | `bf81ce3b1181` | 12 | 960 |

## Platform / competitor scorecard

The median column is a descriptive median of already-paired cell medians; it is not a replacement for the per-cell confidence-interval verdicts.

| Platform | Competitor | Cells | WIN | TIE/INCONCLUSIVE | LOSS | Median cell ratio | Range |
|---|---|---:|---:|---:|---:|---:|---|
| linux-arm64-clang | boost_lockfree | 60 | 52 | 8 | 0 | 2.1797 | [0.9730, 7.2561] |
| linux-arm64-clang | drogalis | 60 | 6 | 30 | 24 | 0.9369 | [0.3890, 2.0660] |
| linux-arm64-clang | moodycamel | 60 | 52 | 8 | 0 | 1.8002 | [1.0038, 5.5737] |
| linux-arm64-clang | rigtorp | 60 | 12 | 23 | 25 | 0.9422 | [0.4202, 2.7332] |
| linux-arm64-gcc | boost_lockfree | 60 | 57 | 3 | 0 | 2.1598 | [0.9612, 3.7664] |
| linux-arm64-gcc | drogalis | 60 | 2 | 34 | 24 | 0.9793 | [0.3505, 1.1598] |
| linux-arm64-gcc | moodycamel | 60 | 55 | 5 | 0 | 1.6264 | [0.9784, 2.9643] |
| linux-arm64-gcc | rigtorp | 60 | 5 | 33 | 22 | 0.9702 | [0.5459, 1.2073] |
| linux-x64-clang | boost_lockfree | 60 | 35 | 15 | 10 | 1.0681 | [0.8890, 2.1593] |
| linux-x64-clang | drogalis | 60 | 18 | 35 | 7 | 1.0112 | [0.9004, 1.1707] |
| linux-x64-clang | moodycamel | 60 | 18 | 19 | 23 | 0.9998 | [0.6424, 1.4484] |
| linux-x64-clang | rigtorp | 60 | 34 | 25 | 1 | 1.1039 | [0.9446, 1.5313] |
| linux-x64-gcc | boost_lockfree | 60 | 38 | 7 | 15 | 1.0864 | [0.6068, 2.8224] |
| linux-x64-gcc | drogalis | 60 | 22 | 29 | 9 | 1.0130 | [0.5841, 1.2265] |
| linux-x64-gcc | moodycamel | 60 | 27 | 13 | 20 | 1.0297 | [0.6091, 1.8518] |
| linux-x64-gcc | rigtorp | 60 | 22 | 25 | 13 | 1.0086 | [0.5780, 1.7284] |

## Confirmed loss cells

| Platform | Run | Capacity | Payload | Topology | Competitor | Median ratio | 95% CI | Evidence |
|---|---:|---:|---:|---|---|---:|---|---|
| linux-arm64-clang | 37972045862 | 64 | 8 | unspecified | drogalis | 0.5090 | [0.4455, 0.5905] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 64 | 8 | unspecified | drogalis | 0.5621 | [0.4868, 0.6345] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 64 | 8 | unspecified | drogalis | 0.5637 | [0.4008, 0.6595] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 64 | 16 | unspecified | drogalis | 0.5246 | [0.4372, 0.6251] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 64 | 16 | unspecified | drogalis | 0.5535 | [0.5353, 0.6794] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 64 | 16 | unspecified | drogalis | 0.6449 | [0.5512, 0.7774] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 256 | 8 | unspecified | drogalis | 0.4235 | [0.3419, 0.4498] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 256 | 8 | unspecified | drogalis | 0.5185 | [0.4271, 0.5930] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 256 | 8 | unspecified | drogalis | 0.5211 | [0.4056, 0.6970] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 256 | 16 | unspecified | drogalis | 0.5446 | [0.4689, 0.7942] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972688384 | 256 | 16 | unspecified | drogalis | 0.6000 | [0.5034, 0.8532] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972688384 | 256 | 64 | unspecified | drogalis | 0.8389 | [0.7570, 0.9410] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 1024 | 8 | unspecified | drogalis | 0.5390 | [0.4628, 0.5804] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 1024 | 8 | unspecified | drogalis | 0.5251 | [0.4150, 0.5705] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 1024 | 8 | unspecified | drogalis | 0.4751 | [0.4152, 0.5383] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 1024 | 16 | unspecified | drogalis | 0.5537 | [0.4404, 0.6983] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 1024 | 16 | unspecified | drogalis | 0.4591 | [0.4164, 0.5338] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 1024 | 16 | unspecified | drogalis | 0.7395 | [0.5948, 0.8732] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 65536 | 8 | unspecified | drogalis | 0.5370 | [0.4568, 0.6017] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 65536 | 8 | unspecified | drogalis | 0.3892 | [0.2711, 0.5686] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 65536 | 8 | unspecified | drogalis | 0.3890 | [0.2819, 0.4291] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 65536 | 16 | unspecified | drogalis | 0.5838 | [0.5354, 0.6209] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 65536 | 16 | unspecified | drogalis | 0.4598 | [0.3324, 0.5647] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 65536 | 16 | unspecified | drogalis | 0.4103 | [0.3637, 0.5033] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 64 | 8 | unspecified | rigtorp | 0.5815 | [0.4329, 0.7025] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 64 | 8 | unspecified | rigtorp | 0.4882 | [0.4402, 0.5890] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 64 | 8 | unspecified | rigtorp | 0.5402 | [0.4528, 0.5997] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 64 | 16 | unspecified | rigtorp | 0.5517 | [0.4304, 0.6003] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 64 | 16 | unspecified | rigtorp | 0.5974 | [0.5148, 0.8219] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 64 | 16 | unspecified | rigtorp | 0.6806 | [0.5191, 0.8149] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 256 | 8 | unspecified | rigtorp | 0.4272 | [0.3637, 0.5211] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 256 | 8 | unspecified | rigtorp | 0.5568 | [0.4787, 0.7119] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 256 | 8 | unspecified | rigtorp | 0.4309 | [0.3769, 0.5065] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 256 | 16 | unspecified | rigtorp | 0.5699 | [0.4815, 0.7248] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 256 | 16 | unspecified | rigtorp | 0.7880 | [0.5660, 0.8928] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 256 | 16 | unspecified | rigtorp | 0.6652 | [0.5262, 0.8107] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972688384 | 256 | 64 | unspecified | rigtorp | 0.8565 | [0.7618, 0.9544] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 1024 | 8 | unspecified | rigtorp | 0.5376 | [0.4088, 0.5875] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 1024 | 8 | unspecified | rigtorp | 0.4667 | [0.3990, 0.5827] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 1024 | 8 | unspecified | rigtorp | 0.4854 | [0.3953, 0.6154] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 1024 | 16 | unspecified | rigtorp | 0.4202 | [0.3808, 0.6006] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 1024 | 16 | unspecified | rigtorp | 0.4416 | [0.3687, 0.4944] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 1024 | 16 | unspecified | rigtorp | 0.8805 | [0.6210, 0.9487] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 65536 | 8 | unspecified | rigtorp | 0.5366 | [0.4889, 0.5845] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 65536 | 8 | unspecified | rigtorp | 0.4325 | [0.4002, 0.5080] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 65536 | 8 | unspecified | rigtorp | 0.4442 | [0.3819, 0.4820] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-clang | 37972045862 | 65536 | 16 | unspecified | rigtorp | 0.6306 | [0.5998, 0.6720] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972045862/summary.json` |
| linux-arm64-clang | 37972422306 | 65536 | 16 | unspecified | rigtorp | 0.5924 | [0.5390, 0.6408] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972422306/summary.json` |
| linux-arm64-clang | 37972688384 | 65536 | 16 | unspecified | rigtorp | 0.5510 | [0.4980, 0.6597] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-clang/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 64 | 8 | unspecified | drogalis | 0.6238 | [0.5319, 0.6828] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 64 | 8 | unspecified | drogalis | 0.5428 | [0.5124, 0.6294] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 64 | 8 | unspecified | drogalis | 0.5620 | [0.4701, 0.7017] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 64 | 16 | unspecified | drogalis | 0.7328 | [0.6329, 0.8319] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 64 | 16 | unspecified | drogalis | 0.8044 | [0.7391, 0.9697] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 64 | 16 | unspecified | drogalis | 0.7652 | [0.6986, 0.8472] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 256 | 8 | unspecified | drogalis | 0.6883 | [0.5509, 0.7812] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 256 | 8 | unspecified | drogalis | 0.5895 | [0.4795, 0.7399] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 256 | 8 | unspecified | drogalis | 0.6836 | [0.5631, 0.8951] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 256 | 16 | unspecified | drogalis | 0.6785 | [0.5959, 0.7972] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 256 | 16 | unspecified | drogalis | 0.7150 | [0.4562, 0.8151] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 256 | 16 | unspecified | drogalis | 0.6630 | [0.5838, 0.7844] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 1024 | 8 | unspecified | drogalis | 0.5661 | [0.5323, 0.6829] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 1024 | 8 | unspecified | drogalis | 0.5826 | [0.5140, 0.6858] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 1024 | 8 | unspecified | drogalis | 0.7471 | [0.6313, 0.9089] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 1024 | 16 | unspecified | drogalis | 0.7804 | [0.6458, 0.8534] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 1024 | 16 | unspecified | drogalis | 0.6007 | [0.5318, 0.8058] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 1024 | 16 | unspecified | drogalis | 0.7549 | [0.5691, 0.9505] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 65536 | 8 | unspecified | drogalis | 0.3505 | [0.3043, 0.4732] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 65536 | 8 | unspecified | drogalis | 0.6434 | [0.5598, 0.7130] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 65536 | 8 | unspecified | drogalis | 0.4707 | [0.3729, 0.5271] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 65536 | 16 | unspecified | drogalis | 0.6949 | [0.6470, 0.7431] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 65536 | 16 | unspecified | drogalis | 0.8039 | [0.6746, 0.8954] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 65536 | 16 | unspecified | drogalis | 0.6448 | [0.5662, 0.7406] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 64 | 8 | unspecified | rigtorp | 0.5763 | [0.5452, 0.6374] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 64 | 8 | unspecified | rigtorp | 0.5752 | [0.4726, 0.6335] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 64 | 8 | unspecified | rigtorp | 0.5715 | [0.5083, 0.7283] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 256 | 8 | unspecified | rigtorp | 0.7871 | [0.6481, 0.8379] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 256 | 8 | unspecified | rigtorp | 0.6092 | [0.5727, 0.7498] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 256 | 8 | unspecified | rigtorp | 0.6293 | [0.5674, 0.7490] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 256 | 16 | unspecified | rigtorp | 0.7778 | [0.5756, 0.9334] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 256 | 16 | unspecified | rigtorp | 0.7143 | [0.5522, 0.8621] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 256 | 16 | unspecified | rigtorp | 0.7453 | [0.6003, 0.8989] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 1024 | 8 | unspecified | rigtorp | 0.5459 | [0.4264, 0.6828] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 1024 | 8 | unspecified | rigtorp | 0.5907 | [0.4787, 0.6774] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 1024 | 8 | unspecified | rigtorp | 0.7062 | [0.6093, 0.8246] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 1024 | 16 | unspecified | rigtorp | 0.5840 | [0.5015, 0.7357] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 1024 | 16 | unspecified | rigtorp | 0.6415 | [0.5598, 0.8695] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 1024 | 16 | unspecified | rigtorp | 0.5795 | [0.5093, 0.6577] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 65536 | 8 | unspecified | rigtorp | 0.6454 | [0.5023, 0.7132] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972422306 | 65536 | 8 | unspecified | rigtorp | 0.6273 | [0.5530, 0.7080] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 65536 | 8 | unspecified | rigtorp | 0.6005 | [0.4384, 0.6502] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972045862 | 65536 | 16 | unspecified | rigtorp | 0.8113 | [0.7401, 0.8776] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972045862/summary.json` |
| linux-arm64-gcc | 37972688384 | 65536 | 16 | unspecified | rigtorp | 0.7756 | [0.6762, 0.8330] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-arm64-gcc | 37972422306 | 65536 | 64 | unspecified | rigtorp | 0.9515 | [0.9264, 0.9696] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972422306/summary.json` |
| linux-arm64-gcc | 37972688384 | 65536 | 256 | unspecified | rigtorp | 0.9540 | [0.9305, 0.9671] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-arm64-gcc/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 8 | unspecified | boost_lockfree | 0.8890 | [0.8674, 0.9408] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 16 | unspecified | boost_lockfree | 0.9404 | [0.9231, 0.9629] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 16 | unspecified | boost_lockfree | 0.9606 | [0.8949, 0.9681] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 256 | unspecified | boost_lockfree | 0.9311 | [0.9260, 0.9422] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 2 | 256 | unspecified | boost_lockfree | 0.9054 | [0.9015, 0.9081] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 256 | unspecified | boost_lockfree | 0.9087 | [0.9021, 0.9184] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 64 | 256 | unspecified | boost_lockfree | 0.9358 | [0.9214, 0.9395] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 64 | 256 | unspecified | boost_lockfree | 0.8951 | [0.8863, 0.8993] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 64 | 256 | unspecified | boost_lockfree | 0.8918 | [0.8777, 0.8977] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 1024 | 256 | unspecified | boost_lockfree | 0.9153 | [0.9065, 0.9288] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 2 | 256 | unspecified | drogalis | 0.9029 | [0.8945, 0.9050] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 256 | unspecified | drogalis | 0.9077 | [0.9011, 0.9223] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972422306 | 64 | 16 | unspecified | drogalis | 0.9292 | [0.8977, 0.9470] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972422306 | 64 | 256 | unspecified | drogalis | 0.9004 | [0.8965, 0.9039] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 64 | 256 | unspecified | drogalis | 0.9010 | [0.8970, 0.9025] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972422306 | 1024 | 16 | unspecified | drogalis | 0.9099 | [0.8818, 0.9585] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 1024 | 16 | unspecified | drogalis | 0.9127 | [0.9051, 0.9383] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 8 | unspecified | moodycamel | 0.6424 | [0.6242, 0.6710] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 2 | 8 | unspecified | moodycamel | 0.8328 | [0.8058, 0.8577] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 8 | unspecified | moodycamel | 0.8440 | [0.8081, 0.8599] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 16 | unspecified | moodycamel | 0.7475 | [0.7183, 0.7778] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 2 | 16 | unspecified | moodycamel | 0.8554 | [0.8160, 0.9276] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 16 | unspecified | moodycamel | 0.8082 | [0.7486, 0.8549] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 64 | unspecified | moodycamel | 0.9081 | [0.8818, 0.9334] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 2 | 64 | unspecified | moodycamel | 0.9197 | [0.8750, 0.9595] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 64 | unspecified | moodycamel | 0.9543 | [0.9160, 0.9681] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 256 | unspecified | moodycamel | 0.9604 | [0.9473, 0.9688] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 2 | 256 | unspecified | moodycamel | 0.9337 | [0.9278, 0.9439] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 2 | 256 | unspecified | moodycamel | 0.9346 | [0.9319, 0.9458] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 64 | 256 | unspecified | moodycamel | 0.9607 | [0.9535, 0.9671] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 64 | 256 | unspecified | moodycamel | 0.8863 | [0.8812, 0.8972] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 64 | 256 | unspecified | moodycamel | 0.8871 | [0.8842, 0.8969] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972422306 | 1024 | 8 | unspecified | moodycamel | 0.8078 | [0.3552, 0.9010] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 1024 | 8 | unspecified | moodycamel | 0.8142 | [0.8026, 0.9212] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972422306 | 1024 | 16 | unspecified | moodycamel | 0.7340 | [0.7098, 0.7859] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 1024 | 16 | unspecified | moodycamel | 0.7314 | [0.7151, 0.7972] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972422306 | 1024 | 64 | unspecified | moodycamel | 0.8461 | [0.8073, 0.8851] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972688384 | 1024 | 64 | unspecified | moodycamel | 0.8265 | [0.7829, 0.8525] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972688384/summary.json` |
| linux-x64-clang | 37972045862 | 1024 | 256 | unspecified | moodycamel | 0.9038 | [0.8938, 0.9175] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-clang | 37972422306 | 1024 | 256 | unspecified | moodycamel | 0.9456 | [0.9363, 0.9672] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972422306/summary.json` |
| linux-x64-clang | 37972045862 | 2 | 8 | unspecified | rigtorp | 0.9446 | [0.9205, 0.9635] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-clang/run-37972045862/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 8 | unspecified | boost_lockfree | 0.9144 | [0.8925, 0.9200] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 8 | unspecified | boost_lockfree | 0.9129 | [0.9047, 0.9211] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 8 | unspecified | boost_lockfree | 0.9084 | [0.8800, 0.9285] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 16 | unspecified | boost_lockfree | 0.9393 | [0.9206, 0.9588] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 16 | unspecified | boost_lockfree | 0.9339 | [0.9122, 0.9439] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 16 | unspecified | boost_lockfree | 0.8993 | [0.8827, 0.9112] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 64 | unspecified | boost_lockfree | 0.7267 | [0.7146, 0.7409] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 64 | unspecified | boost_lockfree | 0.7417 | [0.7168, 0.7653] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 64 | unspecified | boost_lockfree | 0.7709 | [0.6976, 0.8036] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 256 | 256 | unspecified | boost_lockfree | 0.6183 | [0.6160, 0.6193] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 256 | 256 | unspecified | boost_lockfree | 0.6185 | [0.6157, 0.6261] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 256 | 256 | unspecified | boost_lockfree | 0.8891 | [0.8444, 0.9326] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 1024 | 64 | unspecified | boost_lockfree | 0.6882 | [0.6562, 0.6936] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 1024 | 64 | unspecified | boost_lockfree | 0.6695 | [0.6551, 0.6877] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 1024 | 64 | unspecified | boost_lockfree | 0.6068 | [0.5991, 0.6301] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 64 | unspecified | drogalis | 0.6325 | [0.6317, 0.6333] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 64 | unspecified | drogalis | 0.6330 | [0.6323, 0.6350] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 64 | unspecified | drogalis | 0.7973 | [0.7436, 0.8492] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 256 | 256 | unspecified | drogalis | 0.6017 | [0.6013, 0.6023] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 256 | 256 | unspecified | drogalis | 0.6013 | [0.6009, 0.6020] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 256 | 256 | unspecified | drogalis | 0.7908 | [0.7688, 0.8424] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 1024 | 64 | unspecified | drogalis | 0.6167 | [0.6151, 0.6179] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 1024 | 64 | unspecified | drogalis | 0.6169 | [0.6143, 0.6174] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 1024 | 64 | unspecified | drogalis | 0.5841 | [0.5691, 0.5971] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 8 | unspecified | moodycamel | 0.7915 | [0.7874, 0.7954] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 8 | unspecified | moodycamel | 0.7845 | [0.7731, 0.7879] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 8 | unspecified | moodycamel | 0.7427 | [0.7255, 0.7579] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 16 | unspecified | moodycamel | 0.7706 | [0.7674, 0.7777] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 16 | unspecified | moodycamel | 0.7800 | [0.7765, 0.7942] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 16 | unspecified | moodycamel | 0.7446 | [0.7337, 0.7605] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 64 | unspecified | moodycamel | 0.6854 | [0.6745, 0.6898] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 64 | unspecified | moodycamel | 0.6937 | [0.6870, 0.7046] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 64 | unspecified | moodycamel | 0.7695 | [0.7080, 0.7939] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 256 | 16 | unspecified | moodycamel | 0.9441 | [0.9331, 0.9664] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 256 | 16 | unspecified | moodycamel | 0.9365 | [0.9326, 0.9479] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 256 | 64 | unspecified | moodycamel | 0.9531 | [0.9431, 0.9638] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 256 | 256 | unspecified | moodycamel | 0.6091 | [0.6087, 0.6164] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 256 | 256 | unspecified | moodycamel | 0.6187 | [0.6149, 0.6233] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 256 | 256 | unspecified | moodycamel | 0.8333 | [0.8151, 0.8727] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 1024 | 8 | unspecified | moodycamel | 0.6920 | [0.6829, 0.7118] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 1024 | 8 | unspecified | moodycamel | 0.7058 | [0.6970, 0.7242] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972045862 | 1024 | 16 | unspecified | moodycamel | 0.6510 | [0.6397, 0.6657] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 1024 | 16 | unspecified | moodycamel | 0.6466 | [0.6430, 0.6573] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 1024 | 64 | unspecified | moodycamel | 0.8378 | [0.8043, 0.8674] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 8 | unspecified | rigtorp | 0.9330 | [0.9252, 0.9380] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 8 | unspecified | rigtorp | 0.9373 | [0.9309, 0.9481] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 16 | unspecified | rigtorp | 0.9294 | [0.9257, 0.9357] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 16 | unspecified | rigtorp | 0.9354 | [0.9234, 0.9552] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972045862 | 2 | 64 | unspecified | rigtorp | 0.6322 | [0.6303, 0.6339] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 2 | 64 | unspecified | rigtorp | 0.6328 | [0.6314, 0.6343] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 2 | 64 | unspecified | rigtorp | 0.8054 | [0.7637, 0.8352] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 256 | 256 | unspecified | rigtorp | 0.6118 | [0.6112, 0.6122] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 256 | 256 | unspecified | rigtorp | 0.6107 | [0.6079, 0.6120] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 256 | 256 | unspecified | rigtorp | 0.8122 | [0.7898, 0.8300] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |
| linux-x64-gcc | 37972045862 | 1024 | 64 | unspecified | rigtorp | 0.6183 | [0.6170, 0.6200] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972045862/summary.json` |
| linux-x64-gcc | 37972422306 | 1024 | 64 | unspecified | rigtorp | 0.6193 | [0.6169, 0.6209] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972422306/summary.json` |
| linux-x64-gcc | 37972688384 | 1024 | 64 | unspecified | rigtorp | 0.5780 | [0.5595, 0.5818] | `evidence/campaigns/2026-10-09/bf81ce3b1181bb1d3885a9f58800a61a72f23b1d/linux-x64-gcc/run-37972688384/summary.json` |

## Claim boundary

The scoreboard supports bounded statements about the frozen GitHub-hosted matrix only. It does **not** support claims that VeriQueue is universally fastest, fastest on every topology, or superior outside the measured compiler/architecture/workload cells.
