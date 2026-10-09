# Linearizability laboratory

`history_generator.cpp` records SPSC operations with a sequentially consistent atomic logical clock. `checker.go` converts them to Porcupine operations and checks them against an independent bounded FIFO state machine.

## Parameterized history generation

The generator accepts:

```text
history_generator <history.json> <seed> <capacity> <ops-per-side> <profile>
```

Supported capacities are `1,2,4,8`. Supported schedule profiles are:

- `uniform`
- `burst`
- `producer-heavy`
- `consumer-heavy`

The seed, capacity, operation count, and profile are embedded in the JSON history, making every case directly replayable.

Typical single-case use:

```bash
cmake -S . -B build/lin -G Ninja \
  -DVERIQUEUE_BUILD_TESTS=OFF \
  -DVERIQUEUE_BUILD_STRESS=OFF \
  -DVERIQUEUE_BUILD_LINEARIZABILITY=ON
cmake --build build/lin --target history_generator
(cd verify/linearizability && go build -o ../../build/lin/history_checker .)

./build/lin/history_generator history.json 123 2 8 burst
./build/lin/history_checker history.json history.html
```

## Campaign

`tools/run_linearizability_campaign.py` executes a deterministic product matrix over capacities, schedule profiles, and seeds. The default GitHub Actions gate runs 128 short histories (4 capacities × 4 profiles × 8 seeds), each with 8 operations per side.

The campaign records `PASS`, `FAIL`, `UNKNOWN`, and harness `ERROR` separately. `UNKNOWN` is never accepted as success. A non-PASS history is preserved with an HTML visualization; an illegal history is additionally delta-debugged, and the shrinker requires candidates to preserve the checker's exact failure exit code so an illegal counterexample cannot silently shrink into a timeout.

Histories are deliberately short because exact linearizability checking can grow combinatorially. Any confirmed illegal minimized history should be promoted from campaign evidence into `verify/histories/regressions/` as a permanent regression fixture.
