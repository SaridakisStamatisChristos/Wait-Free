# Limitations

- SPSC only: exactly one producer thread and one consumer thread.
- Fixed power-of-two capacity known at compile time.
- No blocking/wait API.
- No MPSC/MPMC semantics.
- No dynamic resizing or allocator abstraction.
- Queue destruction requires external quiescence.
- `T` must be nothrow destructible because `try_pop()` and queue destruction destroy live elements on `noexcept` paths.
- `try_pop` additionally requires nothrow move assignment.
- `empty()` and `size_approx()` are advisory concurrent observations, not linearizable snapshots.
- `size_approx()` guarantees only the physical bound `0 <= result <= capacity()` during concurrent observation; the returned value may be stale or may combine cursor observations from different logical instants.
- Wait-free claim applies to queue synchronization/control logic, not arbitrary user type code.
- Sanitizers, stress, fuzzing, Relacy bounded models, and sampled linearizability histories each cover limited execution sets; none alone proves the full implementation.
- Benchmark results are machine/topology/workload-specific and must not be generalized beyond their evidence.
