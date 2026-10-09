# Linearizability laboratory

`history_generator.cpp` records SPSC operations with a sequentially consistent atomic logical clock. `checker.go` converts them to Porcupine operations and checks them against an independent bounded FIFO state machine.

## Parameterized history generation

The generator accepts:

```text
history_generator <history.json> <seed> <capacity> <ops-per-side> <profile> [mode]
```

Supported capacities are `1,2,4,8`. Supported schedule profiles are:

- `uniform`
- `burst`
- `producer-heavy`
- `consumer-heavy`

Supported API modes are:

- `scalar` — scalar push/pop only
- `mixed` — scalar push/pop plus native `push_bulk`, `pop_bulk`, and `consume`

The default mode is `scalar` for backward compatibility. Seed, capacity, operation count, profile, and mode are embedded in the JSON history, making every case directly replayable.

Typical single-case use:

```bash
cmake -S . -B build/lin -G Ninja \
  -DVERIQUEUE_BUILD_TESTS=OFF \
  -DVERIQUEUE_BUILD_STRESS=OFF \
  -DVERIQUEUE_BUILD_LINEARIZABILITY=ON
cmake --build build/lin --target history_generator
(cd verify/linearizability && go build -o ../../build/lin/history_checker .)

./build/lin/history_generator history.json 123 2 8 burst mixed
./build/lin/history_checker history.json history.html
```

## Bulk-operation model

A successful bulk operation is checked as one atomic FIFO-prefix effect. The checker requires:

- returned count is nonnegative and cannot exceed the abstract capacity/data bound
- `success` agrees with whether the returned count is nonzero
- pushed values are exactly the returned-length prefix of the caller input
- popped values are exactly the returned-length FIFO prefix of the abstract queue
- the whole accepted/retired prefix takes effect as one Porcupine operation

For a **positive** concurrent bulk operation, Porcupine deliberately does not require the returned count to equal the maximum prefix available in the abstract state chosen as its serialization point. VeriQueue computes a batch count from an acquired peer cursor and publishes the completed batch later; an overlapping peer operation can change free/data capacity between those events. Requiring both overlapping batches to be maximal at a single serialization order can therefore reject a correct execution.

The stronger no-overlap rule—return the longest prefix that fits—is independently enforced by deterministic unit/model tests. Zero-result operations remain required to serialize at a state where no progress is available.

This separation is intentional: Porcupine tests atomicity, FIFO identity, loss/duplication/phantom behavior, and legal prefix size under overlap; deterministic tests validate exact sequential count semantics.

## Campaign

`tools/run_linearizability_campaign.py` executes a deterministic product matrix over capacities, schedule profiles, API modes, and seeds. The default GitHub Actions gate runs 256 short histories (4 capacities × 4 profiles × 2 modes × 8 seeds), each with 8 operations per side.

The campaign records `PASS`, `FAIL`, `UNKNOWN`, and harness `ERROR` separately. `UNKNOWN` is never accepted as success. A non-PASS history is preserved with an HTML visualization; an illegal history is additionally delta-debugged, and the shrinker requires candidates to preserve the checker's exact failure exit code so an illegal counterexample cannot silently shrink into a timeout.

Histories are deliberately short because exact linearizability checking can grow combinatorially. Any confirmed illegal minimized history should be promoted from campaign evidence into `verify/histories/regressions/` as a permanent regression fixture.
