# Linearizability laboratory

`history_generator.cpp` records SPSC operations with a sequentially consistent atomic logical clock. `checker.go` converts them to Porcupine operations and checks them against an independent bounded FIFO state machine.

Typical use:

```bash
c++ -std=c++23 -O2 -pthread -I../../include history_generator.cpp -o history_generator
./history_generator history.json 123

go run . history.json history.html
```

Histories are deliberately short because linearizability checking can grow combinatorially. Failed histories should be copied into `../histories/regressions/`, shrunk with `tools/shrink_history.py`, and preserved with the seed and visualization.
