# Relacy weak-memory verification

The Relacy lane is optional in ordinary local builds and enabled in CI with `-DVERIQUEUE_BUILD_RELACY=ON`. The dependency is pinned to a commit, not a moving branch.

The model mirrors the production cursor/cached-index algorithm while replacing object lifetime with Relacy-tracked slot variables. Capacities 1, 2, and 4 are explored so the search exercises single-slot handoff, repeated full/empty transitions, and a larger bounded ring while remaining tractable.

The semantic oracle records every producer success/failure and every successful consumer value. After each explored execution it independently reconstructs the FIFO sequence of successful pushes and requires successful pops to be exactly its prefix. It also checks operation accounting, `pop_count <= push_count`, the final logical occupancy, and the physical capacity bound. This is materially stronger than merely asserting that observed values increase.

The production queue itself is also covered by native sanitizers, deterministic/model tests, fuzzing, and an independent Porcupine linearizability checker. Relacy does not prove arbitrary C++ object-lifetime behavior or the entire implementation; it attacks the bounded weak-memory synchronization protocol plus these explicit queue-semantic invariants.
