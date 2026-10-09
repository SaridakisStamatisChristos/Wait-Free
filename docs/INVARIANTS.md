# Queue invariants

**INV-01 — Capacity.** Under the bounded-distance invariant, unsigned modular distance satisfies `0 <= tail - head <= Capacity`.

**INV-02 — Slot ownership.** Producer constructs before publishing tail; consumer accesses only after observing tail; consumer destroys before publishing head; producer reuses only after observing head.

**INV-03 — Single-writer private cursors.** Only producer mutates producer-private state; only consumer mutates consumer-private state.

**INV-04 — FIFO.** Successful pops consume increasing logical enqueue positions.

**INV-05 — No premature reuse.** A slot cannot be reconstructed until an acquire observation of consumer head proves that the preceding lifetime ended.

**INV-06 — No premature consumption.** A slot cannot be inspected until an acquire observation of producer tail proves construction was published.

**INV-07 — Bounded control path.** Ignoring arbitrary execution inside `T`, each operation executes a statically bounded sequence with no peer-dependent retry loop.

**INV-08 — Quiescent destruction.** Queue object destruction begins only after producer and consumer operations have stopped.

**INV-09 — Counter wraparound.** Unsigned rollover does not change state interpretation while live distance remains bounded and `Capacity <= max(Index)/2`.

**INV-10 — No hot-path allocation.** Queue operations use only preallocated slot storage and control state.
